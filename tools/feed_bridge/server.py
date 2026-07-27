"""TCP bridge server: streams positional samples to the Unreal sim as JSON lines.

Usage:
    python server.py --source mock
    python server.py --source epics            # uses DEFAULT_PV_MAP from pv_map.py
    python server.py --source mock --host 0.0.0.0 --port 9100 --rate 20

CA search mode for --source epics (mutually exclusive, default is plain UDP broadcast):
    --ca_addr "<ip>"                                       # UDP broadcast to a known host
    --name_servers "10.26.70.200:5064 10.26.70.200:5065"   # TCP name servers (tunnel/gateway)

Wire format: one JSON object per line, terminated by '\n'. The Unreal ULiveDataFeed
connects as a client, drains the socket each tick, and applies each complete line.

BRIDGE-STAMPED: t, age, stale; indicate the data quality of payload in terms of time. 

This is a prototype: it serves one client at a time and re-accepts when that client
disconnects (e.g. when the sim toggles Manual -> Live again).
"""

from __future__ import annotations

import argparse
import json
import logging
import socket
import time
import os
import select

from data_source import DataSource, MockSource, EpicsSource, PAYLOAD_KEYS

STALE_AFTER_S = 2.0


def build_source(args: argparse.Namespace) -> DataSource:
    if args.source == "mock":
        return MockSource()
    if args.source == "epics":
        from pv_map import DEFAULT_PV_MAP
        return EpicsSource(DEFAULT_PV_MAP)
    raise ValueError(f"unknown source: {args.source}")


def serve_client(conn: socket.socket, source: DataSource, rate: float) -> None:
    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    period = 1.0 / rate if rate > 0 else 0.0
    last_value = {}
    last_update = {}
    dropped = 0
    last_dropped_log = 0.0
    while True:
        mono = time.monotonic()
        try:
            fresh = source.read()
        except Exception as e:
            logging.warning("source.read() failed, carrying forward: %s", e)
            fresh = {}
        for k, v in fresh.items():
            last_value[k] = v
            last_update[k] = mono
        if not last_value:
            time.sleep(period or 0.1)
            continue
        # Please note: lines sent to Unreal should ALWAYS be complete payloads
        payload = dict(last_value)
        payload["t"] = time.time()
        oldest = min(last_update.get(k,mono) for k in last_value)
        payload["age"] = round(mono - oldest, 3)
        payload["stale"] = payload["age"] > STALE_AFTER_S
        line = (json.dumps(payload) + "\n").encode("utf-8")
        # Latest-value-wins; drop sample if client isn't draining fast enough.
        writable, _, _ = select.select([], [conn], [], 0)
        if writable:
            conn.sendall(line)
        else:
            dropped += 1
            if mono - last_dropped_log > 1.0:
                logging.warning("client not draining fast enough, dropped %d samples", dropped)
                last_dropped_log = mono
        if period > 0:
            time.sleep(period)


def main() -> None:
    parser = argparse.ArgumentParser(description="GeminiStarPlatinum live feed bridge")
    parser.add_argument("--source", choices=["mock", "epics"], default="mock")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9100)
    parser.add_argument("--rate", type=float, default=20.0, help="samples per second")
    parser.add_argument("--ca_addr", default=None, help="EPICS_CA_ADDR for epics source")
    parser.add_argument("--name_servers", default=None,
                        help="EPICS_CA_NAME_SERVERS for epics source, e.g. "
                             "'10.26.70.200:5064 10.26.70.200:5065' (tunnel/gateway mode)")
    args = parser.parse_args()

    # --ca_addr is UDP broadcast search; --name_servers is TCP name resolution (tunnel).
    # They are mutually exclusive: --ca_addr re-enables the broadcast list that name-server
    # mode has to suppress, so a silent winner would look like "PVs just don't resolve".
    if args.ca_addr and args.name_servers:
        parser.error("--ca_addr and --name_servers select mutually exclusive CA search "
                     "modes; pass only one")

    if args.rate <= 0:
        parser.error("--rate must be positive and non-zero")

    # Must be set before libca initializes -- see the lazy `import epics` in EpicsSource.
    if args.ca_addr:
        os.environ["EPICS_CA_ADDR_LIST"] = args.ca_addr
        os.environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"
    if args.name_servers:
        os.environ["EPICS_CA_NAME_SERVERS"] = args.name_servers
        os.environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"
        os.environ["EPICS_CA_ADDR_LIST"] = ""

    source = build_source(args)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((args.host, args.port))
        server.listen(1)
        server.settimeout(1.0)
        print(f"[feed_bridge] {args.source} source listening on {args.host}:{args.port} "
              f"at {args.rate} Hz")

        while True:
            try:
                conn, addr = server.accept()
            except socket.timeout:
                # Lets the loop return to the bytecode-eval loop periodically so a
                # pending Ctrl+C is actually delivered while idle (Windows won't
                # interrupt a blocking accept() otherwise).
                continue
            except KeyboardInterrupt:
                print("\n[feed_bridge] shutting down")
                break

            print(f"[feed_bridge] client connected: {addr}")
            try:
                with conn:
                    serve_client(conn, source, args.rate)
            except (ConnectionResetError, BrokenPipeError, ConnectionAbortedError):
                print("[feed_bridge] client disconnected, awaiting new connection")
            except KeyboardInterrupt:
                print("\n[feed_bridge] shutting down")
                break


if __name__ == "__main__":
    main()

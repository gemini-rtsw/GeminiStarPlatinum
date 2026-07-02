"""TCP bridge server: streams positional samples to the Unreal sim as JSON lines.

Usage:
    python server.py --source mock
    python server.py --source epics            # uses DEFAULT_PV_MAP from pv_map.py
    python server.py --source mock --host 0.0.0.0 --port 9100 --rate 20

Wire format: one JSON object per line, terminated by '\n'. The Unreal ULiveDataFeed
connects as a client, drains the socket each tick, and applies each complete line.

This is a prototype: it serves one client at a time and re-accepts when that client
disconnects (e.g. when the sim toggles Manual -> Live again).
"""

from __future__ import annotations

import argparse
import json
import socket
import time

from data_source import DataSource, MockSource, EpicsSource


def build_source(args: argparse.Namespace) -> DataSource:
    if args.source == "mock":
        return MockSource()
    if args.source == "epics":
        from pv_map import DEFAULT_PV_MAP
        return EpicsSource(DEFAULT_PV_MAP)
    raise ValueError(f"unknown source: {args.source}")


def serve_client(conn: socket.socket, source: DataSource, rate: float) -> None:
    period = 1.0 / rate if rate > 0 else 0.0
    while True:
        payload = source.read()
        line = json.dumps(payload) + "\n"
        conn.sendall(line.encode("utf-8"))
        if period:
            time.sleep(period)


def main() -> None:
    parser = argparse.ArgumentParser(description="GeminiStarPlatinum live feed bridge")
    parser.add_argument("--source", choices=["mock", "epics"], default="mock")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9100)
    parser.add_argument("--rate", type=float, default=20.0, help="samples per second")
    args = parser.parse_args()

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

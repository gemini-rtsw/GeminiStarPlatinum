# EPICS Tunnel Connection via `EPICS_CA_NAME_SERVERS` — Bridge Plan

**Status: Steps 1–5 implemented (2026-07-24). Step 6 (pre-flight validation against the real
TCS) is outstanding — it needs the live tunnel.**

Goal: let `tools/feed_bridge/server.py` reach the observatory TCS through the workstation
tunnel (`10.26.70.200`), the same way `tools/test_listen.py` does — i.e. TCP name-server
mode instead of UDP broadcast search. Bridge-only changes; no C++ / wire-schema changes
(the JSON payload contract with `ULiveDataFeed::ApplyLine` is untouched).

## Background (verified against current code)

- The tunnel connection requires **`EPICS_CA_NAME_SERVERS`** (CA name resolution over TCP),
  with `EPICS_CA_AUTO_ADDR_LIST=NO` and `EPICS_CA_ADDR_LIST=""` to suppress UDP broadcast.
  `tools/test_listen.py` is the working reference (formerly `epics_tunnel_test.py`; renamed
  and moved into `tools/` — its module docstring still names the old file).
- `server.py` only supports `--ca_addr`, which sets `EPICS_CA_ADDR_LIST` (broadcast mode) —
  it cannot express name-server mode today.
- The env vars must be set **before libca initializes**, i.e. before the first `import epics`
  in the process. This currently holds only by accident: `EpicsSource.__init__`
  (`data_source.py`) imports pyepics lazily, after `main()` has processed args/env.
  `test_listen.py` makes the same constraint explicit with an inline comment.

## Step 1 — `--name_servers` flag in `server.py`

Mirror the existing `--ca_addr` handling (the arg + env block at the top of `main()`):

```python
parser.add_argument("--name_servers", default=None,
                    help="EPICS_CA_NAME_SERVERS for epics source, e.g. "
                         "'10.26.70.200:5064 10.26.70.200:5065' (tunnel/gateway mode)")
...
if args.ca_addr and args.name_servers:
    parser.error("--ca_addr and --name_servers select mutually exclusive CA search "
                 "modes; pass only one")
if args.name_servers:
    os.environ["EPICS_CA_NAME_SERVERS"] = args.name_servers
    os.environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"
    os.environ["EPICS_CA_ADDR_LIST"] = ""
```

Usage:

```powershell
python tools\feed_bridge\server.py --source epics --name_servers "10.26.70.200:5064 10.26.70.200:5065"
```

Precedence is **decided**: passing both is an error. They select mutually exclusive search
modes, and a silent winner would be painful to debug from the sim side.

## Step 2 — Connection wait + per-PV report in `EpicsSource.__init__`

Over the tunnel, initial channel connection can take seconds (`test_listen.py` allows 8 s).
Today `read()` guards with `pv.connected` / `pv.get(timeout=0.2)`, so a dead tunnel or wrong
PV name just streams empty payloads silently. Add to the constructor, after creating the PVs:

- `wait_for_connection(timeout=8.0)` per PV, printing `[ OK ] <pv> = <value>` / `[FAIL] <pv>`
  exactly like `test_listen.py` (same format so output is comparable between the two).
- Failures are non-fatal — the server still runs, and `serve_client` carries the last known
  value forward — but the operator sees immediately which channels are down.

## Step 3 — Document the import-order constraint

Add a comment next to the lazy `import epics` in `EpicsSource.__init__` (and/or the README):
CA env vars are read at libca context creation, so **no module may import `epics` at top
level** — the lazy import is load-bearing for `--name_servers` / `--ca_addr` to take effect.
`server.py` imports `data_source` at top level, so moving that import up would silently break
both flags.

## Step 4 — Document the elevation frame (code already landed)

The real TCS reports elevation as **altitude: 0° = horizon, +90° = zenith**; the sim's
internal frame is **0° = zenith, −90° = horizon** (`ElevTwistMin/Max = -90/0`; see
`ObservatoryCoordinator.cpp` `MapAltToElevTarget`).

**The conversion is already implemented** — `EpicsSource.read()` emits
`payload["elev"] = raw["elev"] - 90.0`, and the `live-data-feed` skill already describes it.
Only the docs are outstanding:

- `data_source.py` module docstring: `elev` currently reads "telescope elevation, degrees".
  Change to "degrees in the sim frame: 0 = zenith, −90 = horizon (= TCS altitude − 90)".
- `LiveDataFeed.h`, the example JSON line in the `ULiveDataFeed` class comment: the value
  shown (`-60.0`) is already sim-frame, but the frame is never stated. Add the same one-line
  note (comment-only, no C++ logic change).
- `MockSource` already emits the sim frame — no change.
- **Resolved:** the bridge conversion does **not** include `ElevZeroOffset`. That property has
  been repurposed as the operator's user-controllable manual pointing offset for the new
  pointing feature — it is an adjustment applied to *commanded* targets, not a frame
  calibration, so applying it to measured telemetry would corrupt the readback. Recorded in
  the comments at both the `data_source.py` conversion site and in `LiveDataFeed.h`.

## Step 5 — Update the operator docs once the flag lands

- `README.md` currently instructs the user to configure `EPICS_CA_NAME_SERVERS` /
  `EPICS_CA_ADDR_LIST` "separately from running server.py" — that becomes wrong as soon as
  Step 1 exists. Replace with the `--name_servers` invocation.
- The `live-data-feed` skill (`.claude/skills/live-data-feed/SKILL.md`) documents the bridge
  but not its flags; add `--source` / `--ca_addr` / `--name_servers` to the startup section.

## Step 6 — Pre-flight validation (manual, before relying on the bridge)

1. Run `python tools\test_listen.py`. It already hardcodes the same 8 PVs as
   `DEFAULT_PV_MAP` (`tcs:currentAz`, `tcs:currentEl`, `cr:crCurrentPos`, `ec:domePos`,
   `ec:topShtrPos`, `ec:botShtrPos`, `ec:westVentGatePos`, `ec:eastVentGatePos`) and reports
   per-PV connect + value, which confirms they resolve on the real TCS.
   *Cleanup while you're there:* have it import `DEFAULT_PV_MAP` from `pv_map.py` instead of
   duplicating the list, and fix the stale `epics_tunnel_test.py` name in its docstring.
2. Check the raw values against what `EpicsSource` assumes:
   - **Shutters** map TCS-frame degrees → sim degrees via the calibration anchors at the top
     of `data_source.py`. The CLOSED readings are measured; the **OPEN readings are
     provisional** (closed ± assumed 1:1 hinge travel). Take real readings with the shutters
     fully open and replace `TOP_SHUTTER_DEG_OPEN` / `BOT_SHUTTER_DEG_OPEN`.
   - **Vents** are assumed 0–100 % open and averaged into one value. Confirm the `ec:` vent
     PVs really report percent, not degrees/mm.
   - **Elevation**: confirm `tcs:currentEl` really reports 0–90 altitude, which the Step 4
     conversion assumes.

## Known gap (out of scope here)

If no PV ever connects, `serve_client` never sends a line at all (`last_value` stays empty and
the loop `continue`s), so the sim sits in `Connecting` indefinitely with no diagnostic. Step 2
surfaces this on the *bridge console* only. Making the sim show a distinct "connected to bridge,
no data" state is a separate change to the feed-status path.

## Interim workaround (no code changes)

Until Step 1 lands, the tunnel works by exporting the CA env vars in the shell **before**
launching the bridge — same three vars, same order-before-import constraint as `test_listen.py`:

```powershell
$env:EPICS_CA_AUTO_ADDR_LIST = "NO"
$env:EPICS_CA_ADDR_LIST = ""
$env:EPICS_CA_NAME_SERVERS = "10.26.70.200:5064 10.26.70.200:5065"
python tools\feed_bridge\server.py --source epics
```

Notes:

- `$env:` assignments last only for the current PowerShell session — a new terminal needs them
  re-exported, and a bridge started from a terminal without them will silently fall back to
  UDP broadcast and find nothing.
- Do **not** also pass `--ca_addr` in this mode; it sets `EPICS_CA_ADDR_LIST` and re-enables
  broadcast search, defeating the exported vars.
- `python tools\test_listen.py` sets all three itself, so it is the faster way to confirm the
  tunnel is up before starting the bridge.

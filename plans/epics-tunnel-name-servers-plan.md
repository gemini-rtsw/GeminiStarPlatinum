# EPICS Tunnel Connection via `EPICS_CA_NAME_SERVERS` — Bridge Plan

Goal: let `tools/feed_bridge/server.py` reach the observatory TCS through the workstation
tunnel (`10.26.70.200`), the same way the standalone `epics_tunnel_test.py` script does —
i.e. TCP name-server mode instead of UDP broadcast search. Bridge-only changes; no C++ /
wire-schema changes (the JSON payload contract with `ULiveDataFeed::ApplyLine` is untouched).

## Background (verified against current code)

- The tunnel connection requires **`EPICS_CA_NAME_SERVERS`** (CA name resolution over TCP),
  with `EPICS_CA_AUTO_ADDR_LIST=NO` and `EPICS_CA_ADDR_LIST=""` to suppress UDP broadcast.
- `server.py` only supports `--ca_addr`, which sets `EPICS_CA_ADDR_LIST` (broadcast mode) —
  it cannot express name-server mode today.
- The env vars must be set **before libca initializes**, i.e. before the first `import epics`
  in the process. This currently holds only by accident: `EpicsSource.__init__`
  (`data_source.py:79`) imports pyepics lazily, after `main()` has processed args/env.

## Step 1 — `--name_servers` flag in `server.py`

Mirror the existing `--ca_addr` handling (`server.py:75-80`):

```python
parser.add_argument("--name_servers", default=None,
                    help="EPICS_CA_NAME_SERVERS for epics source, e.g. "
                         "'10.26.70.200:5064 10.26.70.200:5065' (tunnel/gateway mode)")
...
if args.name_servers:
    os.environ["EPICS_CA_NAME_SERVERS"] = args.name_servers
    os.environ["EPICS_CA_AUTO_ADDR_LIST"] = "NO"
    os.environ["EPICS_CA_ADDR_LIST"] = ""
```

Usage:

```powershell
python tools\feed_bridge\server.py --source epics --name_servers "10.26.70.200:5064 10.26.70.200:5065"
```

Decide precedence if both `--ca_addr` and `--name_servers` are given (suggest: error out —
they select mutually exclusive CA search modes).

## Step 2 — Connection wait + per-PV report in `EpicsSource.__init__`

Over the tunnel, initial channel connection can take seconds (the test script allows 8 s).
Today `read()` guards with `pv.connected` / `pv.get(timeout=0.2)`, so a dead tunnel or wrong
PV name just streams empty payloads silently. Add to the constructor, after creating the PVs:

- `wait_for_connection(timeout=8.0)` per PV, printing `[ OK ] <pv> = <value>` / `[FAIL] <pv>`
  like `epics_tunnel_test.py`.
- Failures are non-fatal (server still runs; `read()` already carries forward), but the
  operator sees immediately which channels are down.

## Step 3 — Document the import-order constraint

Add a comment near the lazy `import epics` in `data_source.py` (and/or the README): CA env
vars are read at libca context creation, so **no module may import `epics` at top level** —
the lazy import inside `EpicsSource.__init__` is load-bearing for `--name_servers`/`--ca_addr`
to take effect.

## Step 4 — Elevation frame conversion in `EpicsSource`

The real TCS reports elevation as **altitude: 0° = horizon, +90° = zenith**, but the sim's
internal frame is **0° = zenith, −90° = horizon** (`ElevTwistMin/Max = -90/0`; documented in
`ObservatoryCoordinator.cpp:66`). The feed path currently passes `tcs:currentEl` through raw
(`data_source.py:97-99` → `ULiveDataFeed::ApplyLine` → `SetTargets`), so a real +60° would
clamp to 0 (zenith) — wrong by 90°. The click-to-point path already does the correct mapping
(`UObservatoryCoordinator::MapAltToElevTarget` = `Alt - 90 + ElevZeroOffset`); the bridge
must match it.

- In `EpicsSource.read()`, emit `payload["elev"] = raw["elev"] - 90.0` (conversion lives in
  the bridge, like the shutter/vent pct conversions).
- Update the `data_source.py` module docstring: `elev` is "degrees in the sim frame:
  0 = zenith, −90 = horizon (= TCS altitude − 90)". This is the shared wire contract — also
  update the example-line comment in `LiveDataFeed.h:30` (comment-only, no C++ logic change).
- `MockSource` already emits the sim frame — no change.
- Open question: `ElevZeroOffset` (in-viz calibration on the coordinator) applies only to
  click-to-point, not the feed. Decide whether the bridge conversion should include the same
  calibration once real data is flowing.

## Step 5 — Pre-flight validation (manual, before relying on the bridge)

1. Run the existing tunnel test but with the 8 names from `pv_map.py` (`tcs:currentAz`,
   `tcs:currentEl`, `cr:crCurrentPos`, `ec:domePos`, `ec:topShtrPos`, `ec:botShtrPos`,
   `ec:westVentGatePos`, `ec:eastVentGatePos`) to confirm they resolve on the real TCS.
2. Check the raw values: `EpicsSource` assumes shutters/vents are 0–100 % open and converts
   to Unreal ranges (`data_source.py:104-112`). If the real `ec:` PVs report other units
   (degrees/mm), adjust `pct_to_range` conversions accordingly. Likewise confirm
   `tcs:currentEl` really reports 0–90 altitude, which the Step 4 conversion assumes.

## Interim workaround (no code changes)

Until Step 1 lands, the tunnel works by exporting the env vars before launch:

```powershell
$env:EPICS_CA_AUTO_ADDR_LIST = "NO"
$env:EPICS_CA_ADDR_LIST = ""
$env:EPICS_CA_NAME_SERVERS = "10.26.70.200:5064 10.26.70.200:5065"
python tools\feed_bridge\server.py --source epics
```

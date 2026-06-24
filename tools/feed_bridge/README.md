# Feed Bridge (prototype)

A standalone Python process that retrieves telescope/dome positional data and streams
it to the Unreal sim over a TCP socket. The Unreal side (`ULiveDataFeed`) connects as a
client when the sim is switched to **Live** control mode.

## Why a separate process

Unreal never has to link an EPICS Channel Access library: Python owns the data source and
can run/restart independently of the editor. Swapping mock data for a real TCS feed is a
one-flag change with no Unreal rebuild.

## Runtime flow (how the whole pipeline works)

There are **two independent OS processes** that talk over TCP `localhost:9100`. Unreal is the
**client**; this bridge is the **server**. The bridge is launched **manually** — Unreal does
not start or manage it.

1. **Start the bridge first** (this process): `python server.py --source mock` binds the port
   and blocks on `accept()`. Nothing streams yet — there is no client.
2. **Start Unreal.** On `GameInstance` startup, `UObservatoryCoordinator::Initialize()` creates
   the `ULiveDataFeed` and gives it the `GameInstance`. The feed sits idle (not connected, not
   ticking).
3. **Switch the sim to Live** (Manual/Live toggle → `SetControlMode(Live)`). The feed opens a
   non-blocking TCP socket and connects; this server's `accept()` returns.
4. **Streaming.** This server loops at `--rate` Hz: `source.read()` → `json.dumps(payload)+"\n"`
   → `sendall`. The feed drains the socket every frame, splits on `\n`, parses each line, and
   calls the model setters (`SetAzimTarget`, `SetElevTarget`, `SetCassTarget`,
   `SetDomeTwistTarget`, `SetOpen`). Those models broadcast `OnStateChanged`, and the telescope/
   dome actors drive their physics constraints toward the new targets each Tick.
5. **Back to Manual / shutdown.** The feed disconnects; this server gets a broken pipe, prints
   "client disconnected," and loops back to `accept()` for the next session. If this process
   dies mid-session, the feed auto-reconnects every `ReconnectInterval` seconds.

So the correct startup order is: **bridge → editor → toggle Live.** Either process can be
restarted independently.

## Payload schema

One JSON object per line, terminated by `\n`. This schema is the single source of truth,
mirrored in `ULiveDataFeed::ApplyLine` on the C++ side. Missing keys are ignored by the
sim; extra keys are harmless.

```json
{"azim": 180.0, "elev": -60.0, "cass": 120.0, "dome_twist": 0.0, "top_shutter": 45.0, "bot_shutter": -9.0, "vent": 300.0}
```

| Key            | Type  | Meaning                          |
|----------------|-------|----------------------------------|
| `azim`         | float | Telescope azimuth, degrees       |
| `elev`         | float | Telescope elevation, degrees     |
| `cass`         | float | Cassegrain rotator, degrees      |
| `dome_twist`   | float | Dome azimuth/twist, degrees      |
| `top_shutter`  | float | Top shutter swing, degrees       |
| `bot_shutter`  | float | Bottom shutter swing, degrees    |
| `vent`         | float | Vent slide, world units          |

## Running

```bash
# Mock data (no hardware needed) — recommended for testing the pipeline:
python server.py --source mock

# Real TCS feed (requires pyepics + network access to the observatory):
python server.py --source epics
```

Options: `--host` (default `127.0.0.1`), `--port` (default `9100`), `--rate` (Hz, default 20).

Inspect the stream without Unreal:

```bash
# Windows (PowerShell): use Test-NetConnection to confirm the port, or a small nc build.
# Any platform with netcat:
nc 127.0.0.1 9100
```

## Adding a new data source

Subclass `DataSource` in `data_source.py` and implement `read() -> dict`, then wire it into
`build_source()` in `server.py`. EPICS PV names live in `pv_map.py` (placeholders — replace
with real Gemini TCS PVs).

## Defaults must match the C++ side

`ULiveDataFeed` defaults to `127.0.0.1:9100` (`Host`/`Port`, editable as UPROPERTYs). Keep
these in sync, or override the UPROPERTYs in the editor.

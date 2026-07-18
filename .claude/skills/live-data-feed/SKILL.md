---
name: live-data-feed
description: How to run and reason about the live TCS data feed pipeline — the TestIOC EPICS soft IOC, the tools/feed_bridge CA→JSON translator, and ULiveDataFeed in-engine. Use when working on the feed, the bridge, TestIOC, PV names, unit conversion, the JSON wire schema, or when the Manual/Live control toggle misbehaves.
---

# Live data feed & TestIOC

The live feed simulates the eventual Gemini TCS connection. It is a three-process pipeline; the two Python processes are **launched manually** (Unreal does not start them) and live outside the UE module:

```
TestIOC/test_ioc.py        tools/feed_bridge/server.py            ULiveDataFeed (in-engine)
EPICS CA server      -->    CA client (pyepics) + TCP server  -->  TCP client -> model setters -> actors
(publishes PVs)            (translates CA -> JSON)                (parses JSON in ApplyLine)
       \________ EPICS Channel Access ________/   \____ TCP/JSON-lines on 127.0.0.1:9100 ____/
       (UDP 5064/5065 name search + TCP)
```

- **`TestIOC/`** — a pure-Python **EPICS Channel Access soft IOC** (built on `caproto`; `pip install caproto`). `python test_ioc.py --simulate` publishes 8 writable PVs and sweeps them at 1 Hz: `tcs:currentAz`, `tcs:currentEl` (degrees; elevation is real-frame altitude, 0 = horizon / 90 = zenith), `cr:crCurrentPos`, `ec:domePos`, `ec:topShtrPos`, `ec:botShtrPos` (degrees in the real TCS frame — closed reads 11.5 top / 12.1 bottom), and `ec:eastVentGatePos`, `ec:westVentGatePos` (0–100 %). It stands in for real TCS hardware.
- **`tools/feed_bridge/`** — the CA→sim translator (`pip install pyepics`). `server.py --source epics` reads the IOC PVs via `EpicsSource` (PV names in `pv_map.py`), and `--source mock` uses synthetic sines (no hardware). `data_source.py` converts TCS units into the **sim's engineering units**: elevation altitude → the sim's [-90, 0] frame, shutter TCS-frame degrees → sim swing degrees (via calibration anchors; open readings provisional), vent **percentages** → slide world-units (the two vent gates are averaged into one `vent` value) — then streams one JSON object per line over TCP 9100. **Unit conversion lives here, in the bridge; Unreal never links a CA library.**
- **`ULiveDataFeed::ApplyLine`** — parses each line and calls the model setters: `azim/elev/cass` → `UTelescopeModel`, `dome_twist/top_shutter/bot_shutter/vent` → `UDomeModel`. The JSON wire schema is the **contract** shared between `data_source.py` and `ApplyLine` — change both together. Missing/extra keys are tolerated; malformed JSON is skipped.

**Startup order:** IOC → bridge → editor → toggle control to **Live** (the Manual/Live UI calls `UObservatoryCoordinator::SetControlMode`). Either Python process can be restarted independently; the feed auto-reconnects every `ReconnectInterval` seconds. The feed's `Host`/`Port` default to `127.0.0.1:9100` (editable UPROPERTYs) and must match `server.py`.

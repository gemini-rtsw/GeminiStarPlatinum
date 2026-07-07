# Code Review — Feed Bridge & LiveDataFeed Pipeline

**Date:** 2026-07-02
**Scope:** `tools/feed_bridge/` (`server.py`, `data_source.py`, `pv_map.py`), `TestIOC/test_ioc.py`, `ULiveDataFeed` (`Source/GeminiStarPlatinum/{Public,Private}/LiveDataFeed.*`), and the model setters the feed drives (`TelescopeModel.cpp`, `DomeModel.cpp`) plus `ObservatoryCoordinator.cpp` as the owner.

**Overall assessment:** The pipeline is well-shaped for a prototype. The separation (IOC → bridge → TCP/JSON → feed → models) is the right architecture, the "Unreal never links CA" decision is sound and well documented, and the non-blocking socket handling in `ULiveDataFeed` is unusually careful (the recv-return-value comments are excellent). The issues below are mostly about what happens when this graduates from a 20 Hz localhost prototype to a real CA-backed feed: event-driven vs. polled data, burst/backpressure behavior, unvalidated wire values reaching fragile physics, and schema/constant duplication across the Python/C++ boundary.

Severity legend: 🔴 fix before relying on the live path · 🟡 should fix · 🔵 improvement / architectural note

---

## 1. Correctness & robustness

### 🔴 1.1 Wire values reach physics targets unvalidated (NaN/Inf/out-of-range) [FINISHED]
`ULiveDataFeed::ApplyLine` (`LiveDataFeed.cpp:225-251`) casts each JSON number to `float` and calls the setters directly. The telescope setters clamp (`TelescopeModel.cpp:6-22`), but `FMath::Clamp` of a NaN returns NaN, so a NaN on the wire lands in `AzimTarget` and then in the constraint drives. Worse, three of four dome setters have **no clamping at all** — `SetTopShutterTarget` / `SetBotShutterTarget` / `SetVentTarget` (`DomeModel.cpp:12-28`) assign raw values. A single malformed-but-parseable sample (`{"vent": 1e18}`) goes straight into physics constraint targets, and per CLAUDE.md the physics tuning is deliberately fragile. Given the eventual source is a live network feed, treat the wire as untrusted:

- In `ApplyLine`, reject non-finite values (`FMath::IsFinite`).
- Add min/max clamps to the dome shutter/vent setters, mirroring the twist clamp (the header at `DomeModel.h:27` already notes limits should come from a data asset — the shutter/vent limits belong there too).

### 🔴 1.2 An exception from `source.read()` kills the whole bridge
`serve_client` (`server.py:34-41`) is wrapped only by handlers for connection errors and `KeyboardInterrupt` (`server.py:75-82`). Any exception raised by `source.read()` — and pyepics *can* raise on CA disconnects, context teardown, or type conversion — propagates out of `main()` and terminates the process. The Unreal side then cycles Reconnecting→Failed with nothing to reconnect to. Catch `Exception` around the read (log and skip the sample, or send the last-known payload), reserving the connection-error handlers for the socket path.

### 🟡 1.3 Blocking `sendall` with no backpressure policy → stale-burst flood
`conn.sendall(...)` (`server.py:39`) blocks when the client stalls (editor hitch, breakpoint, PIE pause with heavy load). TCP buffers fill; when the client resumes, it receives the entire backlog and `PollSocket` dutifully parses and applies **every** buffered line (`LiveDataFeed.cpp:193-203`). For a positional feed, only the newest sample matters. Two-sided fix:

- **Bridge:** set a send timeout or use a non-blocking socket and drop samples when the buffer is full ("latest value wins" — this is also how CA monitors behave).
- **Feed:** after splitting lines, apply only the *last* complete line per tick (or per key). This also caps per-frame JSON parsing cost after reconnects.

### 🟡 1.4 Stale data is indistinguishable from live data
When a PV disconnects, `EpicsSource.read()` silently omits the key (`data_source.py:88-91`), the sim keeps its last target, and the UI still says **Live** (status is "bytes are arriving", `LiveDataFeed.cpp:154-156`). An operator-facing display that shows frozen positions as live is the kind of thing observatory staff notice immediately. Recommend adding to the wire schema:

- a bridge-side timestamp (and later, the CA server timestamp per PV), and
- either per-key presence tracking or an explicit `stale`/quality flag when `pv.connected` is false.

The feed can then surface "Live (data stale 4.2 s)" — this also directly serves the project's engineer-facing drill-down goal.

### 🟡 1.5 Disconnected PVs can stall the sample loop (verify)
`pv.get()` in `EpicsSource.read()` (`data_source.py:89`) waits on connection with pyepics' internal timeout when a PV is unconnected. With several missing PVs (common against a partially-up TCS), one `read()` can take multiple seconds, stalling the entire stream — including the keys that *are* healthy. Guard with `if pv.connected:` before `get()`, or pass a short `timeout=`. (Behavior depends on pyepics version — verify against yours.)

### 🟡 1.6 `FIPv4Address::Parse` cannot resolve hostnames
`OpenSocket` (`LiveDataFeed.cpp:67`) parses `Host` as a dotted-quad only; setting the UPROPERTY to `localhost` or `tcs-bridge.gemini.edu` fails with a warning. Once the bridge runs on another machine (the realistic CA deployment), you'll want DNS. Use `ISocketSubsystem::GetAddressInfo`, or document "IPv4 literal only" on the UPROPERTY.

### 🟡 1.7 No socket cleanup if the feed is destroyed without `Disconnect()`
`UObservatoryCoordinator::Deinitialize` calls `Disconnect()` (`ObservatoryCoordinator.cpp:25-33`), which covers normal teardown, but `ULiveDataFeed` itself has no `BeginDestroy()` safety net. If the object is ever GC'd while connected (future refactor, PIE edge case), the raw `FSocket*` leaks. Add a `BeginDestroy` override that mirrors `Disconnect()`.

### 🔵 1.8 Unbounded `RxBuffer` growth
If a peer sends bytes with no `\n` (garbled stream, wrong service on the port), `RxBuffer` grows without bound (`LiveDataFeed.cpp:160`). A cap (e.g. 64 KB → treat as protocol error, `HandleDisconnect()`) makes the failure loud and bounded.

### 🔵 1.9 `--rate 0` busy-spins
`serve_client` treats `period == 0` as "no sleep" (`server.py:35-41`), which floods the client and pins a core. Either reject `--rate <= 0` in argparse or document it as "as fast as possible" intentionally.

---

## 2. Performance

### 🟡 2.1 Up to 7 `OnStateChanged` broadcasts per sample, 20× per second [FINISHED]
Each model setter broadcasts individually (`TelescopeModel.cpp:6-22`, `DomeModel.cpp:6-28`), so one JSON line triggers up to 7 delegate broadcasts — at 20 Hz, ~140 broadcasts/s to every listener, each presumably refreshing UI readouts. `UTelescopeModel::SetTarget` even carries a TODO about the triple broadcast (`TelescopeModel.cpp:24-25`). Options, in increasing order of effort:

1. Batch in the feed: give each model a `SetTargets(...)` /begin-end-update scope so one line → one broadcast per model.
2. Dirty-flag: setters mark dirty; the model broadcasts at most once per frame.

This matters more as the UI grows (per the dual-audience visualization goal, listener count will only increase).

### 🟡 2.2 Polling loop vs. CA's event-driven model
The bridge polls `pv.get()` at a fixed `--rate` (`server.py:36-41`, `data_source.py:86-91`). Channel Access is publish/subscribe: the idiomatic client subscribes once (`camonitor` / `PV(..., callback=)`) and receives updates only on change, with server timestamps. Polling at 20 Hz against 1 Hz PVs does 20× redundant work and *adds up to one poll period of latency*; against fast PVs it undersamples. Recommended restructure (see §4.1): CA callbacks update a shared latest-values dict; the TCP loop streams that dict (on change, or at a capped rate). This keeps the wire format identical while making the CA side correct.

### 🔵 2.3 No `TCP_NODELAY` on the bridge socket
Small (~120 byte) JSON lines at 20 Hz are exactly what Nagle's algorithm coalesces; on some stacks this adds tens of ms of latency and makes samples arrive in clumps. Set `conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)` after `accept()`. (The Unreal side only reads, so it doesn't need it.)

### 🔵 2.4 `time.sleep(period)` ignores processing time
The effective rate is `1/(period + read_time + send_time)`, so the stream drifts below the requested rate, and unevenly when CA reads are slow. If fixed cadence matters, sleep until the next absolute deadline (`t_next += period; sleep(t_next - now)`). Becomes moot if §2.2's event-driven shape is adopted.

### 🔵 2.5 Per-line costs in `PollSocket` are fine at 20 Hz — just don't scale them linearly
`RightChopInline` per line is O(remaining buffer) and each line allocates a JSON DOM (`LiveDataFeed.cpp:194-203`, `227-229`). Harmless now; the "apply only the newest line" change from §1.3 fixes the burst case, which is the only place this shows up. If the feed ever carries hundreds of PVs at high rates, move recv+parse to a worker thread and marshal only the merged latest state to the game thread.

---

## 3. Maintainability

### 🟡 3.1 Engineering-unit constants duplicated across Python and C++
The shutter/vent conversion ranges in `EpicsSource.read()` (`data_source.py:100-109`: top `-7…83`, bottom `-3.5…-13`, vent `0…500`) are the same magic numbers hard-coded in `UDomeModel::SetOpen` (`DomeModel.cpp:36-44`) — where a comment admits their provenance is already lost ("I don't remember how I got them"). Two copies in two languages **will** drift. Pick one owner:

- Preferred: name them as constants in *one* place per side, generated/checked from a small shared config (even a JSON file both sides read/embed), and cite the SolidWorks/Logan source in that one place.
- Minimum: name the constants (`TOP_SHUTTER_CLOSED_DEG = -7.0` …) and cross-reference the twin location in a comment on both sides.

This also ties into §1.1 — the same numbers are the natural clamp limits for the dome setters.

### 🟡 3.2 The wire schema "contract" is enforced nowhere
`PAYLOAD_KEYS` (`data_source.py:30`) says it exists "so a source can be validated… against one list," but nothing validates against it; `MockSource` and `EpicsSource` build dicts by hand, and `ApplyLine` hard-codes the key strings again. Cheap wins:

- Bridge: assert `set(payload) <= set(PAYLOAD_KEYS)` in `serve_client` (one line, catches typos in any future source).
- C++: replace the seven if-chains in `ApplyLine` with a static table of `{key → setter}` entries (see §4.2) so adding a data point is a one-line change on each side.

Also note `pv_map.py` mixes two concepts: `vent_west`/`vent_east` are *PV inputs*, not payload keys, but live in the same map that otherwise mirrors the payload schema. A comment distinguishing "payload key" from "raw input key" (or a separate `RAW_INPUTS` map) would prevent someone "fixing" the mismatch.

### 🟡 3.3 `pct_to_range(p, lo, hi)` with `lo > hi` and no types
`pct_to_range` (`data_source.py:32-33`) is called with `lo=-3.5, hi=-13.0` for the bottom shutter (`data_source.py:104`) — mathematically fine for a lerp, but the parameter names lie. Rename to `lerp(p, closed, open_)` or `pct_to_range(p, at_0pct, at_100pct)`, and add type annotations (`p: float, …`) like the rest of the file.

### 🟡 3.4 `__pycache__/*.pyc` files are committed
`git ls-files` shows `tools/feed_bridge/__pycache__/*.cpython-314.pyc` tracked. `TestIOC/` has a `.gitignore`; `tools/` does not. Add `__pycache__/` to a root or `tools/` `.gitignore` and `git rm --cached` the three files.

### 🔵 3.5 Doxygen comment syntax in `LiveDataFeed.h` won't parse [FINISHED]
The `EFeedStatus` member comments use `/*<` (`LiveDataFeed.h:17-21`); Doxygen's trailing-comment markers are `/**<` or `///<`, so these are invisible to doc generation — which CLAUDE.md says is the point of the comment style. Same file also mixes `///<summary>` XML style and plain `/** */`; harmless, but worth normalizing while touching the file.

### 🔵 3.6 Small paper cuts
- `server.py` uses bare `print`; the `logging` module gives timestamps for free, which matter when correlating "client disconnected" against editor logs. (`test_ioc.py` similarly.)
- `pv_map.py:3` has a truncated docstring sentence ("…throughout the python").
- `GetTimeUntilReconnect()` / `GetReconnectAttempts()` (`LiveDataFeed.h:58-59`) should be `const`.
- `README.md:29` (feed_bridge) says the feed calls `SetOpen` — it doesn't (it calls the four per-axis setters); `SetOpen` is the *manual*-mode path. Stale doc.
- `EpicsSource.read()` builds a `set` literal `{"azim", …}` each call (`data_source.py:94`) — hoist to a module constant next to `PAYLOAD_KEYS`.

---

## 4. Architecture — preparing for real EPICS CA

### 4.1 Restructure the bridge around CA monitors (biggest win)
Today's shape — synchronous `read()` polled at a fixed rate inside the client-serving loop — inverts CA's model and couples "how fast we sample hardware" to "how fast we feed one TCP client". Suggested target shape, which keeps the wire contract and `ULiveDataFeed` untouched:

```
CA monitors (pyepics callbacks)        TCP server loop (per client)
  on_change(key, value, ca_timestamp)    every 1/rate s (or on change):
    → convert units                        snapshot latest dict → JSON line → send
    → latest[key] = value  ──shared──▶
```

Benefits: no redundant polling; CA disconnects can't stall the send loop (fixes §1.5 structurally); CA timestamps are available to forward (§1.4); and multiple TCP clients (a second editor instance, a debug `nc`) become trivial since they all read the same `latest` snapshot. `asyncio` or a thread-per-client with a lock both work at this scale; caproto also offers an async *client* if you want one library for IOC and bridge.

### 4.2 Make the schema extensible before the data points multiply
The project goals call for adding more data points and drill-down panels. Today each new value costs edits in `pv_map.py`, `data_source.py` (conversion), `data_source.py` docstring, `ApplyLine`, a model header, a model setter, and the README. Reduce the per-point cost:

- **C++:** table-drive `ApplyLine` — e.g. `static const TMap<FString, TFunction<void(UTelescopeModel*, UDomeModel*, float)>>` or an array of `{Key, TargetModel, Setter}` structs. New key = new row.
- **Python:** attach the unit conversion to the map entry (`pv_map` entry = PV name + optional transform), so `EpicsSource.read()` becomes a generic loop instead of a hand-written if-chain per key.
- **Wire:** consider adding a `"t"` (timestamp) and later `"seq"` field now, while only one consumer exists — it's the cheapest moment to extend the contract, and both directly serve the engineer-facing readouts (latency display, stale detection).

### 4.3 Keep the CA boundary where it is
The decision that Unreal never links a CA library is correct — CA has awkward threading/licensing/build implications inside UE, and the bridge gives you a place to convert units, average the vent gates, and absorb TCS quirks without editor rebuilds. When pointing at the real TCS, the deltas are: real PV names in `pv_map.py` (already noted as reflective of real Gemini keys), `EPICS_CA_ADDR_LIST` configuration (worth a `--ca-addr` flag or documented env var in the README), and gateway/firewall traversal for UDP 5064/5065 name resolution. None of that touches Unreal.

### 4.4 Reconnect/status model is close — two refinements
The Connecting/Live/Reconnecting/Failed state machine and the first-byte `ConnectTimeout` workaround are solid and well commented. Two suggestions:

- **`Failed` still retries forever** (enum comment admits it, `LiveDataFeed.cpp:222`). Either make `Failed` terminal until the user re-toggles Live (clear operator semantics) or rename the state (`Degraded`?) so the UI doesn't tell an operator something is "Failed" while it quietly recovers.
- **Consider exponential backoff** once this points at a real network: fixed 2 s retries against an unreachable gateway are noisy in logs; 2 s → 4 s → 8 s capped at ~30 s is standard and still feels responsive.

### 4.5 TestIOC — fit for purpose, one nit
`test_ioc.py` is clean and does its job. The simulation loop keeps its own `t += 1.0` decoupled from wall time (drift is irrelevant for a test rig), and hanging all axes off `dome_pos.startup` is fine at this scale. One nit: the sweep writes shutters over the full 0–100 % range each cycle, which via the bridge conversion exercises the dome's full mechanical envelope continuously — handy for stress testing, but consider a `--speed` flag or gentler amplitudes if you ever demo with it, since per CLAUDE.md the physics dislikes aggressive targets.

---

## Suggested priority order

1. §1.1 NaN/range validation + dome setter clamps (protects the fragile physics from the network).
2. §1.2 bridge exception handling + §3.4 gitignore (cheap, prevents whole-process failure / repo noise).
3. §1.3 latest-line-wins in `PollSocket` + bridge send policy (fixes the burst-after-stall failure mode).
4. §2.1 batched model broadcasts (UI scalability).
5. §4.1/§4.2 monitor-based bridge + table-driven `ApplyLine` + timestamped schema (do together — this is the "real CA readiness" milestone).
6. Everything else opportunistically.

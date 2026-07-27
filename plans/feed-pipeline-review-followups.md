# Feed Pipeline — Review Follow-Ups

Actionable remainder of `code-review/feed-pipeline-review-2026-07-02.md`, re-verified against
the code on 2026-07-24. The review's own `[FINISHED]` tags are accurate; this plan carries
forward only what is still open, plus items that changed shape since the review was written.

## Verification summary

**Genuinely done** (spot-checked, no action): 1.1 NaN/range validation (`ApplyLine` checks
`FMath::IsFinite`; all four dome setters now clamp against `MotionLimitSettings`) · 1.2 bridge
`source.read()` exception handling · 1.4 stale/quality flags (`t`/`age`/`stale` on the wire,
`bBridgeReportedStale` + `LocalStaleTimeout` in the feed) · 2.1 batched broadcasts (`SetTargets`
+ `bDirty` on both models) · 2.3 `TCP_NODELAY` · 3.5 Doxygen `///<` markers.

**Fixed since the review, untagged** (no action):

- **1.5** — `read()` now guards with `if pv.connected` *and* `timeout=0.2`. Resolved.
- **3.3** — the inverted `pct_to_range(p, -3.5, -13.0)` call is gone; shutters use `lin_map`
  with named CLOSED/OPEN anchors. Only the missing type annotations remain (folded into 3.6).
- **4.3** — the review's "worth a `--ca-addr` flag" is done, plus `--name_servers` for tunnel
  mode. Its note about "gateway/firewall traversal for UDP 5064/5065" is superseded: the
  tunnel path uses TCP name resolution, not UDP search. See
  `plans/epics-tunnel-name-servers-plan.md`.
- **4.2 (wire portion)** — `t` was added. `seq` still absent, but `age`/`stale` cover what it
  was wanted for; drop unless a gap appears.

**Regressed / worse than described:** 3.1 — see Tier 2 below. What the review found as two
copies of the shutter/vent numbers is now **four**.

---

## Tier 1 — Burst/backpressure (§1.3, §2.5) [DONE]

The one open item the review ranked as a real failure mode, and it is still fully open on both
sides. `conn.sendall` blocks when the client stalls (editor hitch, breakpoint, PIE pause); on
resume, `PollSocket` parses and applies **every** buffered line, walking the telescope through
a minute of backlog at frame rate before it reaches the present.

Confirmed safe to fix by latest-wins: `serve_client` sends a full snapshot every line
(`payload = dict(last_value)` — always all keys), so a dropped line never carries a unique
value. This is the enabling fact; note it in a comment so a future partial-payload source
doesn't silently break the assumption.

- **Feed** (`LiveDataFeed::PollSocket`): keep splitting lines, but apply only the last complete
  line per tick. Discard the rest without parsing — this also removes the per-line JSON DOM
  allocation that §2.5 flags, since the burst case is the only place it shows up.
- **Bridge** (`serve_client`): set a send timeout (or non-blocking socket) and drop the sample
  when the buffer is full, rather than blocking. "Latest value wins", matching CA monitor
  semantics.

## Tier 2 — Shutter/vent constants now have four owners (§3.1) [DONE]

The review found the open/closed numbers (top `-7`/`83`, bottom `-3.5`/`-13`, vent `0`/`500`)
duplicated in Python and C++. There are now **four** copies:

1. `data_source.py` — `TOP_SHUTTER_SIM_CLOSED/OPEN`, `BOT_SHUTTER_SIM_CLOSED/OPEN`, and the
   `0.0, 500.0` literals in the vent `pct_to_range` call
2. `MotionLimitSettings.h` — `TopShutterSwingMin/Max`, `BotShutterSwingMin/Max`, `VentSlideMin/Max`
3. `DomeModel.h` — the same six values again as member defaults, overwritten at `Initialize`
   by copy 2
4. `DomeModel::SetOpen` — the same six as bare literals, next to the comment "I don't remember
   how I got them"

Copy 3 is dead weight: `UDomeModel::Initialize` always overwrites it from
`MotionLimitSettings`, so the defaults only apply if the settings object fails to load — in
which case silently different limits are worse than an obvious failure. Copy 4 is the actual
hazard: `SetOpen`'s literals are the *same* numbers as the clamp limits, so "open" means "the
max limit" — but nothing ties them together, and editing the limits in config silently makes
`SetOpen` stop reaching the endpoint.

Steps:

1. Rewrite `SetOpen` to use the limit members rather than literals: open → `TopShutterSwingMax`,
   `BotShutterSwingMin`, `VentSlideMax`; closed → `TopShutterSwingMin`, `BotShutterSwingMax`,
   `VentSlideMin`. Note the axis inversions in a comment — the bottom shutter opens toward its
   *min* and closes toward its *max*, which is exactly the sort of thing that looks like a typo
   later. This deletes copy 4 and makes SetOpen track config automatically.
2. Delete the initializers on `DomeModel.h:40-47` (copy 3), or comment them as
   fallback-if-config-missing. Prefer deleting.
3. Cross-reference copy 1 ↔ copy 2 in comments on both sides, since they cannot share a
   definition across the language boundary. `data_source.py`'s anchors are the bridge's view of
   the same physical endpoints `MotionLimitSettings` clamps to; say so in both files.
4. While in `SetOpen`: record what is known about the provenance of the numbers (Logan's
   figures) rather than leaving "I don't remember how I got them" as the only note.

Deferred: the review's preferred "one shared config file both sides read". Not worth it at six
constants — revisit if a third consumer appears.

## Tier 3 — Cheap correctness/robustness (§1.7, §1.8, §1.9, §2.4)

Each is small and independent.

- **1.7 socket leak on GC** — `ULiveDataFeed` still has no `BeginDestroy()`. Normal teardown
  goes through `UObservatoryCoordinator::Deinitialize` → `Disconnect()`, so this is a safety
  net for a future refactor or PIE edge case; add the override mirroring `Disconnect()`.
- **1.8 unbounded `RxBuffer`** — a peer that never sends `\n` grows it without limit. Cap
  (~64 KB) and treat overflow as a protocol error → `HandleDisconnect()`. [DONE]
- **1.9 `--rate 0` busy-spins** — pins a core and floods the client. Reject `rate <= 0` in
  argparse (simplest), or document it as deliberate "as fast as possible". [DONE]
- **2.4 sleep ignores processing time** — effective rate is `1/(period + read + send)`, so the
  stream drifts under the requested rate, unevenly when CA reads are slow. Sleep to an absolute
  deadline instead. Skip this if Tier 5 lands, which restructures the loop anyway.

## Tier 4 — Schema contract enforcement (§3.2, §4.2)

Adding one data point currently costs edits in `pv_map.py`, `data_source.py` (conversion +
docstring), `ApplyLine`, a model header, a model setter, and the README. Directly against the
project's extensibility goal.

- **Bridge:** assert `set(payload) <= set(PAYLOAD_KEYS)` in `serve_client`. One line, and it
  makes `PAYLOAD_KEYS` do the job its comment already claims.
- **Bridge:** attach the unit conversion to the `pv_map` entry (PV name + optional transform) so
  `EpicsSource.read()` becomes a generic loop instead of a per-key if-chain.
- **C++:** table-drive `ApplyLine` — an array of `{Key, TargetModel, Setter}` — so a new key is
  one row instead of a new `TryGetNumberField` block. Keep the batch-then-`SetTargets` shape so
  §2.1's single-broadcast property survives the refactor.
- **`pv_map.py`:** `vent_west`/`vent_east` are raw *PV inputs*, not payload keys, sitting in a
  map that otherwise mirrors the payload schema. Comment the distinction (or split a
  `RAW_INPUTS` map) so nobody "fixes" the mismatch.

## Tier 5 — CA monitors (§2.2, §4.1)

The remaining architectural item, and the largest. The bridge polls `pv.get()` at a fixed rate
inside the client-serving loop, which inverts CA's publish/subscribe model: 20× redundant work
against 1 Hz PVs, up to one poll period of added latency, and undersampling of fast PVs.

Target shape (wire contract and `ULiveDataFeed` unchanged): CA callbacks
(`PV(..., callback=)`) convert units and write into a shared `latest` dict; the TCP loop
snapshots that dict on its own cadence. Benefits: no redundant polling, CA disconnects can't
stall the send loop, per-PV CA server timestamps become available to forward (the part of §1.4
that the bridge-stamped `age` only approximates), and multiple TCP clients become trivial.

Sequencing: do this **after** Tier 4's per-entry transforms, which give the callbacks a natural
place to live. Do it **before** or together with any per-PV timestamp work.

## Tier 6 — Paper cuts (§3.4, §3.6, §4.4, §4.5)

- **3.4** — `tools/feed_bridge/__pycache__/*.pyc` are still tracked (3 files, confirmed via
  `git ls-files`). `TestIOC/` has a `.gitignore`; `tools/` does not. Add `__pycache__/` and
  `git rm --cached` the three.
- **4.4 `Failed` still retries forever** — `Tick`'s reconnect countdown runs regardless of
  status, so the UI reports `Failed` while the feed quietly keeps trying. Either make `Failed`
  terminal until the operator re-toggles Live, or rename it (`Degraded`). This is operator-facing
  wording, so pick deliberately rather than by implementation convenience. [DONE]
- **4.4 backoff** — fixed 2 s retries against an unreachable gateway are log noise now that a
  real network is in play; 2→4→8 s capped ~30 s.
- **`tools/feed_bridge/README.md:28-29`** — says the feed calls `SetAzimTarget`/…/`SetOpen`. It
  calls `SetTargets` on both models; `SetOpen` is the manual-mode path only. Stale in two ways.
- **`GetTimeUntilReconnect()` / `GetReconnectAttempts()`** (`LiveDataFeed.h:64-65`) — should be
  `const`.
- **`pv_map.py:3`** — docstring sentence still truncated ("…throughout the python").
- **`EpicsSource.read()`** — hoists nothing: the `{"azim", "elev", "cass", "dome_twist"}` set
  literal is rebuilt every call. Move next to `PAYLOAD_KEYS`. Likely disappears in Tier 4.
- **`server.py`** — mixes bare `print` with `logging.warning`; pick one. Timestamps matter when
  correlating bridge output against editor logs.
- **`lin_map`/`pct_to_range`** — add type annotations to match the rest of the file (§3.3
  remainder).
- **4.5 TestIOC sweep** — amplitudes are now correct real-frame values, but still traverse the
  full mechanical envelope every cycle. Fine as a stress rig; add a `--speed`/amplitude flag only
  if it gets used for demos, since the physics dislikes aggressive targets.

---

## Suggested order

1. **Tier 1** — the only open item that produces visibly wrong telescope behavior.
2. **Tier 2** — cheap, and prevents a silent config/`SetOpen` divergence.
3. **Tier 3 + Tier 6** — batch opportunistically; mostly one-liners.
4. **Tier 4**, then **Tier 5** — the "real CA readiness" milestone, in that order.

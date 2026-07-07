# Plan — Feed bridge exception safety + stale-data detection (review §1.2 + §1.4)

**Source review:** `code-review/feed-pipeline-review-2026-07-02.md` items §1.2 and §1.4
**Files:** `tools/feed_bridge/server.py`, `tools/feed_bridge/data_source.py`,
`Source/GeminiStarPlatinum/{Public,Private}/LiveDataFeed.{h,cpp}`, plus docs.

## Core idea

§1.2 (whole `source.read()` throws) and §1.4 (individual PVs silently omitted) are the
**same failure** at different granularities — "we don't have fresh values for some or all
keys this cycle." Handle both with **one** carry-forward-with-age merge in the bridge, and
**one** staleness surface in the feed. Staleness is *data quality*, kept separate from the
*connection* state machine (`EFeedStatus`) — a feed can be `Live` **and** stale.

Decision: **carry-forward** (keep emitting last-known values, flagged stale) rather than
going quiet on CA failure, so a CA-side hiccup never makes the TCP side look disconnected.

---

## 1. Wire schema — add three fields (contract change)

Bridge-stamped in `server.py` (NOT source-supplied), so both sources get them for free:

```jsonc
{ "azim": 180.0, ..., "vent": 300.0,
  "t":     1751....,   // float, bridge wall-clock time.time() at send
  "age":   0.0,        // float, seconds since the STALEST included key was last fresh
  "stale": false }     // bool, age > STALE_AFTER_S
```

`age`/`stale` drive the "Live (data stale 4.2 s)" readout; `t` is added now (cheapest moment,
review §4.2) to enable later end-to-end latency display.

---

## 2. Bridge — `server.py`

The merge loop is where §1.2 and §1.4 unify. `source.read()` returns whatever keys it could
get this cycle; `serve_client` keeps last-known values + per-key freshness timestamps and
carries missing keys forward.

```python
import logging
from data_source import PAYLOAD_KEYS

STALE_AFTER_S = 2.0   # keys older than this mark the sample stale

def serve_client(conn, source, rate):
    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)   # review §2.3, free win
    period = 1.0 / rate if rate > 0 else 0.0
    last_value  = {}   # key -> last known value
    last_update = {}   # key -> monotonic time that value was last fresh
    while True:
        mono = time.monotonic()
        try:
            fresh = source.read()                 # §1.2: was unguarded, could kill the process
        except Exception as e:                    # NOT bare except — let KeyboardInterrupt propagate
            logging.warning("source.read() failed, carrying forward: %s", e)
            fresh = {}                             # zero fresh keys -> everything carries forward
        for k, v in fresh.items():                # §1.4: only keys actually present refresh
            last_value[k] = v
            last_update[k] = mono
        if not last_value:                         # no good read ever yet -> nothing to send
            time.sleep(period or 0.1)              # ...and don't busy-spin
            continue
        payload = dict(last_value)
        payload["t"] = time.time()
        oldest = min(last_update.get(k, mono) for k in last_value)
        payload["age"]   = round(mono - oldest, 3)
        payload["stale"] = payload["age"] > STALE_AFTER_S
        conn.sendall((json.dumps(payload) + "\n").encode("utf-8"))
        if period:
            time.sleep(period)
```

Why this is "do both" and not two patches:
- **§1.2 falls out for free** — raised `read()` -> `fresh = {}` -> merge carries every key
  forward and `age` climbs from the last good read. No special "resend last payload" branch.
- **§1.4 is the same path at key granularity** — one disconnected PV (omitted by
  `EpicsSource.read`, `data_source.py:90`) stops refreshing -> `age` reflects it while healthy
  keys stay live.
- **`age` = worst-case key age** — the conservative operator-facing number.

Scope discipline (per review): the `try` wraps **only** `read()`. `conn.sendall` stays
outside it, so a client disconnect still propagates to `main()`'s
`ConnectionResetError/BrokenPipeError` handler (`server.py:78`) and re-accepts — not swallowed
as "skip a sample."

---

## 3. Bridge — `data_source.py`

- Docstring schema block (lines 7-15): document `t`/`age`/`stale` as **bridge-stamped**, not
  source-supplied. No logic change to `MockSource`/`EpicsSource`.
- Optional (review §1.5, feeds §1.4): guard the per-PV read so one dead PV can't stall the loop:

```python
value = pv.get(timeout=0.2) if pv.connected else None   # data_source.py:89
```

  An unconnected PV cleanly omits its key -> carries forward -> ages out.

---

## 4. Feed — `LiveDataFeed.h`

Staleness is data quality, NOT a new `EFeedStatus`. Add:

```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFeedDataQualityChanged, bool /*bStale*/, float /*AgeSeconds*/);

public:
    FOnFeedDataQualityChanged OnDataQualityChanged;   // fires when the stale flag flips
    bool  IsDataStale()       const { return bDataStale; }
    float GetDataAgeSeconds() const { return DataAgeSeconds; }   // UI polls this for the readout

    /** No line applied for this long => treat data as stale even if the bridge never says so. */
    UPROPERTY(EditAnywhere, Category = "LiveFeed") float LocalStaleTimeout = 2.f;

private:
    void  SetDataQuality(bool bNewStale, float NewAge);
    float BridgeReportedAge    = 0.f;   // "age" field from the last line
    bool  bBridgeReportedStale = false;
    float TimeSinceLastSample  = 0.f;   // local: resets to 0 in ApplyLine
    float DataAgeSeconds       = 0.f;   // max(bridge age, local silence)
    bool  bDataStale           = false;
```

`LocalStaleTimeout` covers what the bridge can't: if the whole bridge process freezes, `age`
freezes with it. Tracking "time since I last applied a line" client-side catches that.
Effective staleness = bridge-reported OR local silence.

---

## 5. Feed — `LiveDataFeed.cpp`

**`ApplyLine`** (after existing model pushes, ~line 265) — read new fields, reset silence timer:

```cpp
double AgeVal = 0.0;
if (Json->TryGetNumberField(TEXT("age"), AgeVal) && FMath::IsFinite(AgeVal))
    BridgeReportedAge = static_cast<float>(AgeVal);
bool bStaleVal = false;
if (Json->TryGetBoolField(TEXT("stale"), bStaleVal))
    bBridgeReportedStale = bStaleVal;
TimeSinceLastSample = 0.f;   // a line arrived this frame
```

**`Tick`** (after `PollSocket()`, ~line 117) — re-evaluate every frame so local silence trips
even when no line arrives:

```cpp
if (bEstablished)
{
    TimeSinceLastSample += DeltaTime;
    const bool bStale = bBridgeReportedStale || TimeSinceLastSample >= LocalStaleTimeout;
    SetDataQuality(bStale, FMath::Max(BridgeReportedAge, TimeSinceLastSample));
}
```

**`SetDataQuality`** — mirror `SetStatus`: keep `DataAgeSeconds` current every frame (readout
wants a smooth number), broadcast only on the bool transition to avoid a per-frame delegate storm:

```cpp
void ULiveDataFeed::SetDataQuality(bool bNewStale, float NewAge)
{
    DataAgeSeconds = NewAge;
    if (bDataStale != bNewStale)
    {
        bDataStale = bNewStale;
        OnDataQualityChanged.Broadcast(bDataStale, DataAgeSeconds);
    }
}
```

**`Disconnect` / `HandleDisconnect`** — reset quality state so a fresh connection doesn't
inherit a stale flag:

```cpp
TimeSinceLastSample = 0.f;
bBridgeReportedStale = false;
SetDataQuality(false, 0.f);
```

**UI wiring:** keep the existing `OnStatusChanged` handler for the Manual/Live/Reconnecting
label; add an `OnDataQualityChanged` handler that appends "(stale N.Ns)" using
`GetDataAgeSeconds()` each refresh.

---

## 6. Doc touch-ups

- `data_source.py` docstring schema block — add `t`/`age`/`stale`.
- `CLAUDE.md` "Live data feed & TestIOC" section — note the three new bridge-stamped fields
  in the wire contract.
- `LiveDataFeed.h` class comment JSON example (line 29) — add the fields.

---

## Deliberate scope cuts

- **Per-key ages** — `age` is a single worst-case number. A `"ages":{...}` map for the
  engineer drill-down is a later extension; `last_update` per key already exists, so it's small.
- **NaN/range clamps (§1.1)** — separate task. `ApplyLine` already guards `IsFinite`, but the
  dome setters still don't clamp. Not in scope here.

## Suggested apply order

1. `server.py` + `data_source.py` docstring — independently testable with `--source mock`.
2. `LiveDataFeed.h/.cpp` — header change means full editor restart + rebuild (not Live Coding).
3. Docs.

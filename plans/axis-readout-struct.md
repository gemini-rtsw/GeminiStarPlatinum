# Plan: Per-Axis Readout Struct (`FAxisReadout`)

**Status:** Proposed / deferred. Not currently needed — see "Realistic take" below.
**Context:** Operator-style UI that shows both **target** and **current** values for each
moving axis (telescope Az/El/Cass, dome twist/shutters/vent), mirroring real TCS operator
screens (`target | current | Δerror | on-target lamp`).

Current state (the baseline this plan would replace/augment):
- Targets are `BlueprintReadOnly` UPROPERTYs on the models (`UTelescopeModel::AzimTarget`
  /`ElevTarget`/`CassTarget`; dome targets on `UDomeModel`).
- Current values are `BlueprintCallable` getters on the actors
  (`AMovingTelescope::GetAzimTwist/GetElevSwing/GetCassTwist`, `AMovingDome::GetDomeTwist`),
  each reading the live physics angle off its `UPhysicsConstraintComponent`.
- The chosen near-term path is **UMG-only**: the widget reads model targets + actor current
  getters and computes the delta itself. This plan is the documented upgrade from that.

---

## Scope

Introduce a Blueprint-exposed struct that bundles everything one UI row needs for a single
axis, plus one getter per axis that fills it. The widget then binds to a single call per row
and only renders fields — no arithmetic or convention fixups in the widget graph.

In scope:
- A `USTRUCT` `FAxisReadout { Target, Current, Error, bOnTarget }`.
- One getter per axis on the actors (3 telescope, 4 dome) returning a filled `FAxisReadout`.
- Per-axis on-target tolerance as an editable UPROPERTY.
- Centralizing the elevation sign/offset reconciliation inside the elevation getter.

Out of scope (explicitly):
- Refresh/timer mechanism — unchanged; widget still polls (~20 Hz) because current values
  move every physics tick. The struct changes *what each call returns*, not *when* you call.
- The UMG widget assets themselves (built in-editor).
- Moving the readout ownership to the models (see "Alternative ownership" below).

---

## Pros

- **Single source for delta/on-target logic.** `Error` and `bOnTarget` are computed once in
  C++ instead of being re-derived in every widget. A second view (e.g. an at-a-glance HUD
  overlay *and* a detailed drill-down menu — both called for in the project's dual-audience
  goal) consumes the same getter and cannot disagree about what "on target" means.
- **Centralizes convention fixups.** The elevation reconciliation (measured `Swing2` is
  negated, and the constraint carries an `AngularRotationOffset` of `ElevAngularOffset`)
  lives inside `GetElevReadout()`. The widget receives a `Current` already in the same frame
  as `Target`, so `Error` is automatically meaningful. UMG-only exposes that negation/offset
  per widget, where it is easy to get subtly wrong.
- **Resolves the model/actor straddle.** Target comes from the model subsystem, current from
  the actor. The struct getter is the one place that touches both, so the widget needs only
  an actor reference and gets a complete answer.
- **Tolerance becomes a tunable**, editable per axis in the editor, instead of a magic
  number copy-pasted into widget graphs (azimuth may tolerate different slop than Cassegrain).

## Cons

- **More C++ surface + header churn.** A `USTRUCT` plus 7 getters. Per the build notes,
  adding `USTRUCT`/`UPROPERTY`/`UFUNCTION` signatures requires a **full editor restart +
  rebuild**, not just Live Coding (`Ctrl+Alt+F11`).
- **Doesn't change the refresh story.** Still polled on a timer; the struct returns more per
  call, not less often.
- **Indirection.** Debugging the readout means opening the actor getter rather than seeing
  the arithmetic inline in the widget — a mild cost for a single-panel prototype.

---

## Realistic take — how needed is this?

**Not needed yet.** For a single operator panel, UMG-only is the lighter, correct call and
the delta math is trivial. The struct is an investment that only pays off when the
"add panels/data points without rework" goal actually kicks in — i.e. **multiple views of
the same axis**, or many more axes.

**Upgrade trigger:** the moment you find yourself copy-pasting the delta/tolerance logic into
a *second* widget, or you hit a bug where two panels disagree on the elevation convention.
At that point the struct stops being speculative and starts removing duplication that exists.
Until then, this doc is the parking spot.

---

## Technical implementation details

### 1. The struct — where it lives

Create a dedicated public header so both actors (and, if ever needed, the models) can include
it without circular dependencies:

- **New file:** `Source/GeminiStarPlatinum/Public/AxisReadout.h`
  - Needs a generated header: `#include "AxisReadout.generated.h"`.
  - No matching `.cpp` required (POD struct, no method bodies).

Sketch (shape only):

```cpp
// AxisReadout.h
#pragma once
#include "CoreMinimal.h"
#include "AxisReadout.generated.h"

/** Snapshot of one rotational/linear axis for operator display. Units: degrees
 *  (telescope axes, dome twist/shutter swing) or world-units (vent slide). */
USTRUCT(BlueprintType)
struct FAxisReadout
{
    GENERATED_BODY()

    /** Commanded value from the model (source of truth). */
    UPROPERTY(BlueprintReadOnly) float Target = 0.f;

    /** Measured live physics value from the constraint, in Target's frame. */
    UPROPERTY(BlueprintReadOnly) float Current = 0.f;

    /** Target - Current. Sign indicates slew direction. */
    UPROPERTY(BlueprintReadOnly) float Error = 0.f;

    /** |Error| < tolerance for this axis. */
    UPROPERTY(BlueprintReadOnly) bool bOnTarget = false;
};
```

### 2. Per-axis tolerance UPROPERTYs

Add to each actor's header (`MovingTelescope.h`, `MovingDome.h`), grouped near the existing
threshold UPROPERTYs:

```cpp
UPROPERTY(EditAnywhere, Category="Readout") float AzimOnTargetTol = 0.1f; // deg
// ...ElevOnTargetTol, CassOnTargetTol; dome: twist/topShtr/botShtr/vent tolerances
```

(Consider eventually sourcing these from a DataAsset alongside the rotational limits the
TODO in `TelescopeModel.h` already mentions — keep it inline for now.)

### 3. The getters — where they live and what they do

Add to the actors, alongside the existing current getters. Declare in headers (UFUNCTION),
implement in `.cpp`.

- **`MovingTelescope.h` / `.cpp`:** `GetAzimReadout()`, `GetElevReadout()`, `GetCassReadout()`
- **`MovingDome.h` / `.cpp`:** `GetDomeTwistReadout()` (+ shutter/vent readouts as those
  getters are added)

```cpp
UFUNCTION(BlueprintCallable, Category="Readout")
FAxisReadout GetAzimReadout();
```

Implementation pattern (azimuth, the simple twist case):

```cpp
FAxisReadout AMovingTelescope::GetAzimReadout()
{
    FAxisReadout R;
    if (auto* M = GetGameInstance()->GetSubsystem<UTelescopeModel>())
        R.Target = M->AzimTarget;
    R.Current   = GetAzimTwist();                 // reuse existing getter
    R.Error     = R.Target - R.Current;
    R.bOnTarget = FMath::Abs(R.Error) < AzimOnTargetTol;
    return R;
}
```

Elevation is the one with the convention work — it is the reason to centralize:

```cpp
FAxisReadout AMovingTelescope::GetElevReadout()
{
    FAxisReadout R;
    if (auto* M = GetGameInstance()->GetSubsystem<UTelescopeModel>())
        R.Target = M->ElevTarget;
    R.Current   = GetElevSwing();   // already returns -(GetCurrentSwing2());
    // NOTE: verify GetElevSwing() is in the SAME frame as ElevTarget before trusting Error.
    //       If ElevAngularOffset shifts the measured zero relative to the target's
    //       convention, apply that correction HERE so every consumer sees a clean Error.
    R.Error     = R.Target - R.Current;
    R.bOnTarget = FMath::Abs(R.Error) < ElevOnTargetTol;
    return R;
}
```

### 4. Includes / wiring

- Both actors `#include "AxisReadout.h"` and the relevant model header
  (`TelescopeModel.h` / `DomeModel.h`) in their `.cpp` (already needed for the existing
  target reads).
- No `Build.cs` change — no new module dependency.
- After adding the struct/getters: **regenerate VS project files** (right-click `.uproject`)
  and do a **full editor restart + rebuild**, since these are header signature changes.

### 5. Widget side (unchanged in mechanism)

The UMG row binds one node — `GetAzimReadout()` — and renders `Target`/`Current`/`Error`
plus an indicator driven by `bOnTarget`. The actor reference is still acquired once
(e.g. `UGameplayStatics::GetActorOfClass`) and cached. Refresh still on a ~20 Hz timer.

---

## Alternative ownership (noted, not chosen)

The more MVC-pure home for `FAxisReadout` is the **models**, since the model is the source of
truth. That would mean the actor writes its measured `Current` back into the model each tick,
and the widget reads readouts entirely from the subsystem (no actor reference needed). Same
struct, different owner; more wiring (the tick write-back) in exchange for a widget that never
touches an actor. Revisit if/when the actor reference in the widget becomes a pain point.

---

## File summary

| Action | File | Notes |
|--------|------|-------|
| Create | `Source/GeminiStarPlatinum/Public/AxisReadout.h` | `USTRUCT FAxisReadout`; needs `.generated.h` |
| Edit   | `Source/GeminiStarPlatinum/MovingTelescope.h/.cpp` | 3 getters + 3 tolerance UPROPERTYs |
| Edit   | `Source/GeminiStarPlatinum/MovingDome.h/.cpp` | readout getters + tolerances as shutter/vent getters land |
| None   | `GeminiStarPlatinum.Build.cs` | no new dependency |
| Manual | regen VS project files; full editor restart + rebuild | header signature changes |

# Plan: Star "GoTo" — construct a telescope target from a celestial object

**Status:** Proposed. Not built. Plan-only.
**Context:** Celestial Vault already answers the *forward* question — "given a look
direction, which star is there?" (`ACelestialVaultDaySequenceActor::GetClosestStarInfo`).
This plan adds the *reverse*: given a known star (or its RA/Dec), command the telescope
to point at it — a classic observatory "GoTo". It serves the project's *immersive operator
experience* goal (operators pick a target; the mount slews to it) and reuses the existing
MVC seam (`model = source of truth`, coordinator = control source, actor = view).

Prerequisite reading already done (findings baked into this plan):
- Celestial Vault ships **C++ source** at
  `C:\Program Files\Epic Games\UE_5.6\Engine\Plugins\Experimental\CelestialVault\` — it is
  **not** content-only, contrary to the stale comment in `EnvironmentView.h:12-13` ("Celestial
  Vault is a content-only Blueprint plugin, so that last hop cannot be done in C++").
  `UCelestialMaths` is a `UBlueprintFunctionLibrary`; the sky is an
  `ACelestialVaultDaySequenceActor`. Fix that comment as part of this work.
- **There is no `RA/Dec → Alt/Az` node.** The math library stops at sidereal time
  (`JulianDateToGreenwichMeanSiderealTime`, `LocalSideralTime`) and an *equatorial* Cartesian
  conversion (`RADECToXYZ_RH`). Horizontal (azimuth/altitude) coordinates are never computed.
- **But** every visible star is placed as a **world-space ISM instance**, and the plugin
  exposes each star's `ISMInstanceIndex` (`FStarInfo`) plus the instanced component
  (`StarsComponent`). That makes the reverse lookup the exact mirror of the forward query.

---

## Scope

Add a pointing helper that turns a chosen celestial target into
`UTelescopeModel::SetTargets(Azim, Elev, Cass)` calls.

In scope:
- A new control path (a method on `UObservatoryCoordinator`, or a small dedicated
  `UPointingSolver` it owns) that:
  1. resolves a target star to a **world-space direction**, then
  2. converts that direction into the telescope model's Azim/Elev conventions, then
  3. writes it via the existing model setters.
- Target selection by identity (name / Hipparcos / Henry Draper / Yale Bright Star ID) against
  the vault's `StarsInfo` array, **or** by a raw `FStarInfo`/`(RA,Dec)` pair.
- A one-shot "slew to" and an optional "track" mode that re-solves as sim time advances.

Out of scope (explicitly):
- The target-picker UI (built in UMG/Blueprint later; this plan exposes the callable).
- Dome co-pointing (keeping the shutter aligned with azimuth) — separate concern, its own plan.
- Precession/nutation/refraction refinements — Route A inherits whatever fidelity the vault
  renders; we do not add our own astrometry.
- Any change to the physics drives — this only writes model targets; the actor slews as it
  already does for Manual/Live.

---

## Two routes (decision: Route A)

### Route A — read the star's world transform (chosen)

`GetClosestStarInfo` (`CelestialVaultDaySequenceActor.cpp:292`) already resolves a star to a
world position:

```cpp
StarsComponent->GetInstanceTransform(StarsInfo[i].ISMInstanceIndex, ISMInstanceTransform, /*bWorldSpace=*/true);
FVector DirectionToInstance = ISMInstanceTransform.GetLocation() - ObserverLocation; // normalize, dot vs LookupDirection
```

The reverse uses the **same call**, but we already know the star and want the direction:

```
Dir = (StarInstanceWorldLocation - TelescopeWorldLocation).GetSafeNormal();
```

Why chosen: the instance world location **already bakes in** observer lat/lon *and* the
time-of-day sidereal rotation (the vault is oriented by `GetPlanetCenterTransform` + the
DaySequence time of day; stars are placed via `RADECToXYZ_RH(...) * FVector(100,-100,100)` in
`InitStars`, `CelestialVaultDaySequenceActor.cpp:578`). Pointing at that vector aims the
telescope exactly where the star is *rendered*, eliminating every convention trap at once:
equatorial-frame handedness, the `-100` Y-flip into UE's left-handed frame, the azimuth-zero
reference, and the sidereal-time rotation. We stay entirely in the UE world frame the sky uses.

### Route B — analytic from RA/Dec (fallback only)

All time primitives exist: `UTCDateTimeToJulianDate` → `JulianDateToGreenwichMeanSiderealTime`
→ `LocalSideralTime(Longitude, GMST)` gives LST; then `HourAngle = LST − RA` and the standard
Alt/Az trig, which **we would have to write** (the library has no such node). `RADECToXYZ_RH`
yields only the *equatorial* unit vector, not horizontal, so Route B still owes the entire
observer-frame rotation — precisely the work Route A gets for free.

**Keep B in reserve for one case:** the vault only instances stars brighter than
`MaxVisibleMagnitude` (default 6.0), so a sub-visible catalog target has **no ISM instance** and
Route A cannot see it. B can still point at it from raw RA/Dec. Not worth building until such a
target is actually requested.

---

## Hard prerequisites / gotchas (these decide correctness)

- **`bKeepStarsInfo` must be `true`** on the `ACelestialVaultDaySequenceActor` (default is
  `false`). Otherwise `StarsInfo` is empty and `GetInstanceTransform` has no index to use.
  This is an editor-set property on the sky actor — document it, and fail loudly if unset.
- **RA is stored in *hours*** in `FStarInfo`/`FStarInputData` (Dec in degrees). `InitStars`
  multiplies RA by 15 to get degrees. Matters for Route B and any RA display.
- **Magnitude gate** (`MaxVisibleMagnitude`) — see Route B note above.
- **Model conventions still apply.** The world direction is unambiguous, but mapping it to
  `AzimTarget`/`ElevTarget` must match how the actor *measures* those axes today:
  - Elevation is measured as `Atan2(FwdInAzim.Z, FwdInAzim.X)` in the azimuth frame, with a
    `-3°` empirical `CalibrationOffset` and a constraint `ElevAngularOffset = -45°`
    (`MovingTelescope.cpp:249`, `:21`). The model's `ElevTarget` lives in `[-90, 0]`
    (`SetElevTarget` clamps there). Our solver must produce Elev **in the same frame as
    `ElevTarget`** — i.e. invert the same offset/sign the readout path documents.
  - Azimuth is a continuous multi-turn twist (`GetAzimTwist` returns `AzimTwistState.Continuous`)
    canonicalized into `[-180, 360]` by `UnwrapGeminiAz`. Our computed azimuth must be
    referenced to the **same zero and sign** as that twist. `SetAzimTarget` already wraps/clamps,
    so we hand it a world-referenced azimuth and let it canonicalize.
  - **This calibration is the one genuine unknown** and must be verified in-viz (point at a
    known star, compare rendered vs. commanded). Expect a small constant offset to fall out,
    exactly like `GetElevSwing`'s `-3°`.
- **Cassegrain:** for a realistic alt-az mount the Cass rotator tracks the **parallactic
  angle** to hold field orientation. v1 may leave `CassTarget` under manual control (don't
  write it). Computing parallactic angle from H/Dec/lat is a clean follow-up once Az/Elev is
  trusted. Flag, don't build.
- **Reachability:** the model setters clamp to mount limits, so an out-of-envelope star
  silently clamps to the nearest limit. **Elevation is the only axis that can actually be
  unreachable** — a below-horizon star maps outside `[ElevTwistMin, ElevTwistMax]` (defaults
  `[-90, 0]`; `Config/DefaultGame.ini` currently overrides only `AzimTwistMax`). Azimuth's
  `[-180, 360]` span covers the full circle, so `UnwrapGeminiAz` always finds a valid wrap.
  The solver should pre-check the computed Elev against the model's limit fields (read them
  off the model — they're config-driven via `UMotionLimitSettings`, don't hardcode) and return
  a "not reachable" result instead of commanding a bogus target.
- **Staleness (track mode):** a star's world direction drifts as the rendered time of day
  advances. A one-shot target goes stale within seconds of sim time. Track mode must
  re-solve — see below. Note the drift is driven by the **DaySequence/Blueprint side** of the
  day cycle, not by `UEnvironmentModel::CurrentTime` — nothing in C++ advances `CurrentTime`
  while `bTimeProgresses` is true, so the model's clock (and its `OnStateChanged`) cannot be
  the re-solve trigger. This is also a quiet point *for* Route A: the instance world transform
  always reflects what is actually rendered, even when the model clock has gone stale.

---

## Technical implementation details

### 1. Module dependency

`UObservatoryCoordinator` (or the new solver) will call into `ACelestialVaultDaySequenceActor`
and touch `FStarInfo` in C++, so the plugin module must be in the build:

- **Already done (uncommitted):** `Source/GeminiStarPlatinum/GeminiStarPlatinum.Build.cs`
  already lists `"CelestialVault"` in `PrivateDependencyModuleNames`.
- The plugin is already enabled in `GeminiStarPlatinum.uproject`, so no `.uproject` change.
- Header signature changes here ⇒ **regenerate VS project files + full editor restart/rebuild**,
  not Live Coding.

### 2. Getting the vault actor and telescope location

The solver needs two world references each solve:

- **Sky actor:** `UGameplayStatics::GetActorOfClass(World, ACelestialVaultDaySequenceActor::StaticClass())`,
  cached with a `TWeakObjectPtr`. Guard for null (level without a sky) and for
  `bKeepStarsInfo == false`. `World` here is `GetGameInstance()->GetWorld()` — the coordinator
  is a `UGameInstanceSubsystem`, so it reaches the current world through its outer game
  instance. No world exists yet at subsystem `Initialize()` time, so both actor lookups happen
  **lazily at first solve** (re-resolved via the weak pointer if the level changed), matching
  the project's existing lazy `GetSubsystem<>` fetch pattern.
- **Telescope actor:** the coordinator is a `UGameInstanceSubsystem` — it has no transform of
  its own, so *both* world references below must come through a cached
  `TWeakObjectPtr<AMovingTelescope>` (found via `UGameplayStatics::GetActorOfClass`, same
  pattern as the sky actor; guard for null). It provides:
  - **Pivot location** — the world location to measure direction *from*. Use the azimuth pivot
    (the `Azim` component's world location), not the actor origin, so the direction matches
    the axis the mount actually rotates about. Expose a small getter on the actor (e.g.
    `FVector GetAzimPivotWorldLocation()`), or read `Azim->GetComponentLocation()`. For a sky
    at ~400,000 km the parallax between actor origin and pivot is negligible, but using the
    pivot keeps it principled.
  - **Base frame** — the upright, non-rotating frame the direction→angle trig (§4) decomposes
    in: `Telescope->GetActorTransform()`. See §4 for why neither `Ground` (pitched +90°) nor
    `Azim` (spins with the mount) can serve as this frame.

### 3. Target resolution → `FStarInfo`

Two entry points:

```cpp
// PRIVATE helper — search the vault's StarsInfo (populated only if bKeepStarsInfo).
// bool + out-param mirrors the plugin's own GetClosestStarInfo signature.
bool FindStarByName(const FString& Name, FStarInfo& Out) const;
// Also by catalog ID: FStarInfo has int32 HipparcosID / HenryDraperID / YaleBrightStarID
// (0 = absent from that catalog — skip zero matches).
// By value — caller already has the record or a raw (RA,Dec).
```

Identity search is a linear scan of `TArray<FStarInfo> StarsInfo` (small; visible-star count).
No new catalog needed — reuse the vault's.

**Public surface stays minimal:** only `SlewToStar`/`TrackStar` (taking the target identity
directly) are `UFUNCTION(BlueprintCallable)`; `FindStarByName` and the direction→angle math
are private implementation details of resolving that identity. The future UMG picker does
not need the resolver either — `ACelestialVaultDaySequenceActor::StarsInfo` is already
`BlueprintReadOnly`, so the picker enumerates the vault's array directly to build its list,
then hands the chosen identity to `SlewToStar`.

### 4. Direction → model Az/Elev (the core)

Mirror the actor's *measurement* math (`GetElevSwing`/`GetAzimTwist`) in reverse so the result
lands in the model's frame:

```cpp
// Dir: unit world vector telescope→star.
// Express in the NON-ROTATING, UPRIGHT base frame: the actor root
// (RootSceneComponent / GetActorTransform()). Two frames that look tempting are wrong:
//  - Azim's frame spins with the mount → azimuth would come out relative to current
//    pointing instead of absolute. (GetElevSwing uses it only because it measures
//    elevation *relative to* the azimuth structure.)
//  - Ground's frame is the locked anchor, but it is pitched +90° off the root
//    (MovingTelescope.cpp:46, to align the azimuth constraint's twist axis with world
//    vertical), so its local Z is not "up" and the trig below would read the wrong axes.
// This code runs on the coordinator (a subsystem with no transform of its own), so the
// transform comes through the cached telescope actor pointer from §2.
const FVector DirInBase = Telescope->GetActorTransform().InverseTransformVectorNoScale(Dir);

float AzimDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Y, DirInBase.X)); // referenced to azim zero
float ElevDeg = FMath::RadiansToDegrees(FMath::Atan2(DirInBase.Z,
                    FMath::Sqrt(DirInBase.X*DirInBase.X + DirInBase.Y*DirInBase.Y)));

// Reconcile to model conventions (constants to be CALIBRATED in-viz, cf. GetElevSwing's -3):
AzimDeg = AzimDeg + AzimZeroOffset;             // align azimuth zero + sign to AzimTwistState
ElevDeg = MapAltToElevTarget(ElevDeg);          // e.g. account for ElevAngularOffset(-45) + [-90,0] frame
```

Then:

```cpp
if (auto* M = GetGameInstance()->GetSubsystem<UTelescopeModel>())
{
    // Optional: pre-clamp check for reachability before committing.
    M->SetTargets(AzimDeg, ElevDeg, M->CassTarget); // leave Cass untouched in v1
}
```

`SetTargets` handles `UnwrapGeminiAz`, `UnwindDegrees`, limit clamping, and the single
broadcast — no need to duplicate any of it.

> **Calibration is the one step that must be measured, not derived.** Point at a couple of
> known bright stars, compare the rendered star position to the commanded axis, and bake the
> resulting constant offsets into `AzimZeroOffset` / `MapAltToElevTarget`. Budget a short in-viz
> session for this; it is the analogue of the `-3°` already sitting in `GetElevSwing`.

### 5. One-shot vs. track mode

- **One-shot (`SlewToStar`):** resolve once, write once. Simplest; target drifts with sim time.
- **Track (`TrackStar`):** cache the resolved `FStarInfo` + a `bTracking` flag on the
  coordinator. Re-solve and re-write on a timer via
  `GetGameInstance()->GetTimerManager()` (~1–4 Hz is plenty; the mount can't slew faster than
  sim-time drift anyway). **Do not hook `UEnvironmentModel::OnStateChanged`:** it fires only
  on discrete `SetCurrentTime` calls — nothing broadcasts (or even updates `CurrentTime`) in
  C++ while the day cycle plays, so it would never re-solve during exactly the drift it's
  meant to chase. The coordinator is a `UGameInstanceSubsystem` (not tickable), so a timer is
  also the natural mechanism. Re-solving is cheap: one `GetInstanceTransform` + the trig above.
  Also bind `SetCurrentTime`'s `OnStateChanged` for immediate re-solve on discrete time jumps.
  GetTimerManager() refers to Unreal's Gameplay Timers system, which allows a developer to set 
  a function to execute according to a global timer.
- **Interaction with control mode:** GoTo is a Manual-mode action. If `EControlMode::Live` is
  active the feed owns the targets; either refuse GoTo in Live, or treat engaging GoTo as a
  switch back to Manual. Decide with the user before building — mirrors the existing
  Manual/Live brokering in `UObservatoryCoordinator::SetControlMode`.

### 6. Where the code lives (MVC fit)

Two options, both keeping the model as source of truth:

- **On `UObservatoryCoordinator`** (recommended): it already brokers control sources and holds
  the environment/time context conceptually. Add `SlewToStar`/`TrackStar` alongside
  `SetControlMode`. Minimal new surface.
- **Dedicated `UPointingSolver`** owned by the coordinator: better if GoTo grows (planets,
  parallactic Cass, dome co-point). Start on the coordinator; extract later if it bloats.

The actor (`AMovingTelescope`) is unchanged except possibly the pivot-location getter — it keeps
reading `AzimTarget`/`ElevTarget`/`CassTarget` off the model exactly as now.

---

## Realistic take

The core (Route A one-shot) is small: a module dependency, an actor lookup, ~15 lines of
direction→angle trig, and a calibration pass. The only real risk is the Az/Elev convention
reconciliation, and that risk is bounded — the actor's own `GetElevSwing`/`GetAzimTwist` show
exactly which frame and offsets to invert, and any residual error is a single measured constant.
Track mode and parallactic-Cass are natural follow-ups, not v1. Route B is a documented escape
hatch for sub-visible targets, not something to build up front.

---

## File summary

| Action | File | Notes |
|--------|------|-------|
| Done   | `Source/GeminiStarPlatinum/GeminiStarPlatinum.Build.cs` | `"CelestialVault"` already in `PrivateDependencyModuleNames` (uncommitted) |
| Edit   | `Source/GeminiStarPlatinum/Public/ObservatoryCoordinator.h` + `.cpp` | `SlewToStar`/`TrackStar`, star resolution, direction→Az/Elev, reachability check |
| Edit   | `Source/GeminiStarPlatinum/MovingTelescope.h` + `.cpp` | (optional) `GetAzimPivotWorldLocation()` getter |
| Edit   | `Source/GeminiStarPlatinum/Public/EnvironmentView.h` | fix stale "content-only Blueprint plugin" comment |
| None   | `GeminiStarPlatinum.uproject` | CelestialVault already enabled |
| Manual | set `bKeepStarsInfo = true` on the sky actor | required for Route A queries |
| Manual | regen VS project files; full editor restart + rebuild | Build.cs / header changes, not Live Coding |
| Manual | in-viz calibration of `AzimZeroOffset` / elev mapping | measure against known stars, bake constants |

---

## Open questions for the user (before any build)

1. **GoTo vs. Live:** should engaging GoTo force Manual mode, or be refused while Live?
2. **Cassegrain:** leave manual in v1, or track parallactic angle from the start?
3. **Track mode:** needed in v1, or is one-shot slew enough to prove the path?

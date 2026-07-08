# Investigation: the `GetElevSwing()` fudge factor

**Subject:** `AMovingTelescope::GetElevSwing()` (`Source/GeminiStarPlatinum/MovingTelescope.cpp:245`)

```cpp
float AMovingTelescope::GetElevSwing()
{
    return -(ElevConstraint->GetCurrentSwing2() - ElevAngularOffset) + 3.f; // TODO: fudge factor
}
```

## TL;DR

The elevation readout is measured in **constraint space**, whose zero reference is not a
surveyed physical reference — it is *whatever pose the meshes happened to be in when the
constraint frames were snapshotted at physics init*, further rotated by
`AngularRotationOffset`. The "actual elevation angle" the fudge was tuned against is a
**world-space / visual** reference (debug rotation strings, the forward-vector laser line).
These two references are only aligned if the SolidWorks-exported elevation mesh is authored
*exactly* at the assumed zero pose relative to the trunnion axis. Any authoring misalignment
shows up as a **constant bias on every reading** — that constant is what the hand-tuned
`+3.f` is papering over. The residual error remains because (a) the true bias is not exactly
3.0°, and (b) several smaller pose-dependent effects are folded into a single constant.

A secondary structural problem makes this hard to reason about: `ElevAngularOffset` is
bookkept in **three places at once** (baked into the constraint frame, re-applied in
`SwingComponent`'s error/drive math, and subtracted again in `GetElevSwing()`), so the zero
reference is implicit and fragile.

## How the reading is actually produced (verified in engine source, UE 5.6)

1. **Frame snapshot at init** — `UPhysicsConstraintComponent::UpdateConstraintFrames()`
   (`Engine/Private/PhysicsEngine/PhysicsConstraintComponent.cpp:535`) captures both
   constraint reference frames from the **component transforms at constraint
   initialization**:
   - Frame1 (child = `Elev` body): the constraint component's world X/Y axes expressed in
     the child body's local space.
   - Frame2 (parent = `Azim` body): the *same* world axes, but **pre-rotated by
     `ConstraintInstance.AngularRotationOffset`** (lines 552–555). Our
     `CreateConstraintComponent` sets that offset to `FRotator(SwingAngle, 0, 0)` =
     pitch −45° for the elevation constraint (`MovingThing.cpp:98`), which recenters the
     ±45° cone limit.

2. **The reading** — `FChaosEngineInterface::GetCurrentSwing2()`
   (`Runtime/PhysicsCore/Private/ChaosEngineInterface.cpp:1667`) computes the relative
   transform of Frame1 w.r.t. Frame2 and returns
   `GetRotation().GetTwistAngle(Swing2Axis)` — the swing-twist decomposition's twist
   component about the local Y axis, in radians (converted to degrees by the component
   wrapper). Two consequences:
   - The −45° `AngularRotationOffset` is **baked into the reading** (it rotated Frame2).
     `GetElevSwing()` subtracts `ElevAngularOffset` to undo it. Self-consistent, but it
     means the readout depends on the offset value matching what was passed at
     construction.
   - The reported angle is **relative to the init-time pose of the meshes**, not to the
     horizon or to any authored "elevation = 0" datum. At spawn the reading is exactly the
     baked offset; every subsequent reading is "rotation since spawn, plus offset".

3. **Algebra of the current code.** With `CurrentSwing = -GetCurrentSwing2()` (the same
   negation `SwingComponent` uses at `MovingThing.cpp:183`):

   ```
   GetElevSwing() = CurrentSwing + ElevAngularOffset + 3
                  = ElevTarget + (control error) + 3      // after SwingComponent converges
   ```

   `SwingComponent`'s snap phase drives the constraint to the *exact* orientation target
   (`SwingTarget − SwingOffset`, spring 5e19 vs. gravity torque ~1e9–1e10 in these units, so
   steady-state droop is negligible; the fudge is far too large to be spring droop). In
   constraint space, therefore, the un-fudged readout would equal `ElevTarget` at rest.
   **The +3° was added because the *visually measured* elevation (mesh pose / laser line)
   disagreed with constraint space by ~3° — i.e. the discrepancy lives between the two
   reference frames, not in the control loop.**

## Root cause (primary): the zero reference is the authored spawn pose

The frame chain is: `Ground` pitched +90° under root → `Azim` (identity) → `Elev` rolled
180° → static mesh `elevation_vsp` in whatever orientation SolidWorks exported it. The
constraint component sits on `Azim` with identity relative rotation. When
`UpdateConstraintFrames` snapshots this at init, it *defines* the spawn pose as
"swing = 0 (before offset)". The readout math then assumes that pose corresponds to a
specific real elevation angle.

If the exported mesh's optical/tube axis is ~3° off from the assembly frame's X axis at
its authored pose (pivot placement, export orientation — the same import pipeline whose COM
data had to be hand-corrected from SolidWorks mass properties), then **every reading is
biased by that constant**, which is exactly the signature the fudge factor compensates.
The value 3.0 is "arbitrary" because it was eyeballed from debug output rather than
measured, and the eyeball reference itself is suspect (see below).

## Contributing causes (why the fudge still leaves a margin of error)

Ranked by likely magnitude:

1. **The calibration reference was itself unreliable.** The tuning was presumably done
   against `DrawDebugString(... Elev->GetComponentRotation().ToString() ...)`
   (`MovingTelescope.cpp:198`). A world-space `FRotator` of a body that is rolled 180° under
   a parent pitched 90° does **not** display the elevation angle directly in its Pitch
   field — Euler decomposition near ±90° pitch redistributes angle between pitch/yaw/roll.
   Matching the constraint reading to that string can bake in an error of a few degrees
   that varies with pose.

2. **Swing–twist decomposition cross-talk.** `GetTwistAngle(Swing2Axis)` equals the pitch
   angle only when the relative rotation is a *pure* swing-2 rotation. Twist and swing-1
   are locked but solved iteratively with projection tolerances
   (`SetProjectionParams(1, 1, 0.1, 0.01)`, `MovingThing.cpp:112`); small residual rotation
   about the other axes leaks into the decomposition. Pose- and load-dependent, sub-degree,
   but not constant — no single fudge constant can absorb it.

3. **Init-time settling.** The frames are snapshotted from component transforms during
   physics setup. Gravity is enabled on `Elev`/`Cass`; if the bodies move even slightly
   before/while the constraint initializes (or across PIE restarts / Live Coding reloads),
   the zero reference shifts by that amount. This would make the required fudge *vary
   between runs*, which matches "still leaves a margin of error".

4. **Dynamic lag during motion.** While `|error| > ElevAngularThreshold` (3°), only the
   velocity drive is active (spring = 0), so during a slew the readout legitimately trails
   the target by up to the threshold. Note the suspicious coincidence: **the fudge (3.0)
   equals `ElevAngularThreshold` (3.0)**. If the tuning comparison was made while (or right
   after) the axis was still in the velocity phase, the observed "constant" 3° gap may
   partly have been the threshold-sized control error at the moment of observation, not a
   frame bias at all.

## Experiments to disambiguate (in-editor, no code risk)

1. **Read the spawn value.** Log `ElevConstraint->GetCurrentSwing2()` on the first tick
   after physics init. Expected: exactly ±45° (the baked offset). Any deviation is the
   init-settling term (cause 3).
2. **Build a geometric ground truth.** Compute elevation directly from geometry each tick:
   project `Elev->GetForwardVector()` into `Azim`'s local frame and take
   `atan2(vertical, horizontal)`. This is the *actual* elevation by definition. Plot
   `(geometric − constraint-space)` across the full range (0°, −30°, −60°, −90°) at rest:
   - Constant gap → authoring/frame bias (cause: primary). The constant *is* the correct
     fudge — measured, not guessed.
   - Gap varies with pose → decomposition/Euler effects (causes 1–2).
3. **Change `ElevAngularThreshold` to 1°** and re-check the settled gap. If the needed
   fudge shrinks, cause 4 contaminated the original calibration.

## Recommended fix

Replace the constraint-space readout with the **direct kinematic measurement** from
experiment 2 (forward vector of `Elev` expressed in `Azim`'s frame). Benefits:

- It reads the geometry the user actually sees, so no fudge factor by construction.
- Immune to constraint-frame snapshot timing, `AngularRotationOffset` bookkeeping, and
  swing-twist decomposition leakage.
- Same pattern as the already-working `MeasureSlide()` (world-space measurement rather
  than constraint introspection), except for the remaining `RestOffset`-style constant:
  if the mesh's authored forward axis is itself tilted, calibrate **once, numerically**
  (log the geometric reading at a surveyed pose) instead of hand-tuning.

If staying with `GetCurrentSwing2()` instead, at minimum:

- Capture `S2_0 = GetCurrentSwing2()` at `BeginPlay` and report relative to it plus the
  known spawn elevation, removing both the offset double-bookkeeping and the init bias in
  one measured constant.
- Document that `ElevAngularOffset` is load-bearing in three places (constraint frame
  offset at construction, `SwingComponent` error/drive math, readout) and must stay
  consistent.

This also resolves the open note in `plans/axis-readout-struct.md:173` ("verify
GetElevSwing() is in the SAME frame as ElevTarget"): with the current code the frames match
*only up to the fudge*, i.e. `Error` computed as `Current − Target` would carry a built-in
−3° bias.

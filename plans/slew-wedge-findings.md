# Findings: telescope stops accepting targets after repeated SlewToStar

**Status:** Diagnosis only. No fixes applied.
**Symptom:** Slew to Polaris works; slew to Castor settles a few degrees *above* the star;
after that the telescope ignores all new targets — from `SlewToStar` **and** from the UI
controls — for the rest of the session.

**Key structural fact that scopes the search:** the actor re-reads
`Model->AzimTarget/ElevTarget/CassTarget` every `Tick` (`MovingTelescope.cpp:203-205`) and the
model setters guard NaN and clamp inputs (`TelescopeModel.cpp:46-98`). So a wedge cannot live in
the model's broadcast/dirty logic — it must be either (a) **another writer overwriting the model
targets**, or (b) **the physics island going dead** so drives no longer act.

Findings are ranked by likelihood × ease of confirmation. Work them top to bottom.

---

## Finding 1 — Tracking timer keeps overwriting targets and cannot be stopped

**Confidence: high (deterministic, matches symptom exactly).**

`TrackStar` starts a 0.5 s repeating timer driving `SolveTrackingMovement`
(`ObservatoryCoordinator.cpp:133`), which rewrites all three model targets every fire
(`ObservatoryCoordinator.cpp:160`). Once it is running:

- Any `SlewToStar` or UI target is silently overwritten within 0.5 s → "won't receive any
  new targets" while the mount holds on the tracked star. This is the symptom verbatim.
- **`ToggleTracking(false)` does not stop it** — it flips `bTracking` but never calls
  `ClearTimer(TrackingTimer)` (`ObservatoryCoordinator.cpp:164-169`), and
  `SolveTrackingMovement` never checks `bTracking`. The timer runs forever.
- `ToggleTracking(true)` conversely does **not** start the timer, so the toggle is
  half-wired in both directions.
- `TrackStar` on the **same** star early-returns (`:126`) — fine — but `TrackStar` on a new
  star while already tracking works only because it clears first (`:129-130`). `SlewToStar`
  does **not** stop tracking, so a one-shot slew during tracking is a no-op after 0.5 s.
- `SolveTrackingMovement` dereferences `CelestialVault`/`Telescope` weak pointers without
  `IsValid()` (`:139`, `:141`) — a level change while tracking will crash.

**Action:**
1. `ToggleTracking(false)` (and any "stop" path) must `ClearTimer(TrackingTimer)`.
2. `SolveTrackingMovement` should early-out (or self-clear) when `!bTracking` as a belt-and-braces guard.
3. Decide the arbitration rule: `SlewToStar` should cancel tracking (or be refused while
   tracking) — same brokering question as Manual/Live.
4. Guard the weak pointers in `SolveTrackingMovement`.
5. Also reset `TrackedStar` when tracking stops, otherwise re-tracking the same star is
   refused by the `:126` early-return.

**Confirm:** reproduce the wedge, then check whether `bTracking`/the timer is active
(log in `SolveTrackingMovement`, or watch `Stat UObject`-free: just add a temporary
`UE_LOG` there). If lines print while "wedged", this is it.

---

## Finding 2 — `SlewToStar`/`TrackStar` ignore `EControlMode` (Live feed clobbers everything)

**Confidence: medium; trivial to rule out.**

Neither `SlewToStar` (`ObservatoryCoordinator.cpp:103-106`) nor the tracking path checks
`Mode`. If the session is in **Live**, `ULiveDataFeed::ApplyLine` rewrites all telescope
targets on every sample (`LiveDataFeed.cpp:277`) — UI and GoTo both appear dead. This is
open question #1 in `plans/star-goto-pointing.md` (§5), never decided.

**Action:** decide and implement one of: refuse GoTo/track while Live, or have GoTo force
`SetControlMode(Manual)`. Symmetrically, `SetControlMode(Live)` should stop tracking.

**Confirm:** when wedged, read `Mode` and `FeedStatus` off the coordinator (both are
`BlueprintReadOnly`, visible from the UI/console).

---

## Finding 3 — Force-mode override on Elev/Cass drives is clobbered every tick

**Confidence: medium-high that it's wrong; medium that it caused *this* wedge.**

Constructor deliberately puts the gravity-enabled Elev/Cass constraints in **force mode**
(`MovingTelescope.cpp:119`, `:132` — "Set to false for physics sake"), with gains tuned for
force units (`5e19` strength / `2e19` damping). But `TwistComponent` and `SwingComponent`
end **every tick** with `SetAngularDriveAccelerationMode(bAccelerationMode)`
(`MovingThing.cpp:170`, `:197`) and the base default is `true` — so from the first tick the
Elev/Cass drives actually run in **acceleration mode with force-tuned gains**, ~15 orders of
magnitude too stiff. CLAUDE.md's description of the override is a dead letter after one frame.

A solver fed accelerations that large works "most of the time" and then produces a
NaN / blown constraint island under an edge condition — after which **every** drive on the
telescope island is inert: meshes freeze at the last valid pose and no target moves anything.
That also matches the symptom.

**Action:** make the per-tick mode reapplication respect per-constraint intent — either
remove the per-tick call, or pass the desired mode per constraint (e.g. a parameter on
`TwistComponent`/`SwingComponent`) — then re-verify Elev/Cass gains in whichever mode they
actually run in. Treat gains as retune-required if the effective mode changes.

**Confirm:** when wedged, look at the on-screen debug strings the actor already draws
(`Elev->GetComponentRotation()` and the elevation constraint force,
`MovingTelescope.cpp:198-200`) — `NaN` or absurd magnitudes = dead island. Also check the
Output Log for Chaos warnings at the moment it stopped, and compare `Model->ElevTarget`
vs `GetElevSwing()` (model accepted the target; body ignored it).

---

## Finding 4 — Bang-bang threshold + hard limit grind can destabilize the solver

**Confidence: medium; amplifier for Finding 3 rather than standalone.**

`SwingComponent` switches between a pure velocity drive (damping `2e19`) outside
`AngularThreshold` (3°) and a `5e19` orientation snap inside it
(`MovingThing.cpp:186-196`). When the error hovers right at ~3° — where the mount visibly
stopped — the controller can flap between the two gigantic-gain regimes every frame, with
gravity torque on the Elev body. That flap zone is the natural trigger for the impulse spike
in Finding 3.

Separately: a commanded elevation within 3° of (or past) the hard swing limit (±45° about
the −45° offset ⇒ physically [−90, 0]) can never enter the snap zone, so the velocity drive
grinds against the locked limit **forever** — no stall detection exists. The new
reachability check in `SlewToStar`/`SolveTrackingMovement` (`:101`, `:151`) rejects
out-of-range targets but not near-limit ones.

**Action:** after Finding 3 is fixed, consider hysteresis on the threshold (e.g. enter snap
at 3°, leave at 5°) and a stall guard when pinned at a limit. Don't retune before fixing the
mode clobber — the current behavior is measured in the wrong mode.

---

## Finding 5 — "Settles a bit above the star": constraint frame is ~3° off the true frame

**Confidence: high for the mechanism; the −3° default now set on `ElevZeroOffset` needs
in-viz confirmation.**

`GetElevSwing` needs `CalibrationOffset = -3.f` (`MovingTelescope.cpp:251`) to make the
constraint-frame reading match reality, and `SwingComponent` converges purely in constraint
space — so a healthy, converged slew points ~3° away from the rendered star. This explains
"a bit above Castor" independently of the wedge. `ElevZeroOffset` now defaults to `-3.f`
(`ObservatoryCoordinator.h:45`), which should cancel it.

**Action:** once Findings 1-3 are resolved, slew to 2-3 known bright stars and verify the
laser/debug line lands on the rendered star; adjust `ElevZeroOffset` (and `AzimZeroOffset`,
still 0) from the residual. This is the calibration pass from
`plans/star-goto-pointing.md` §4.

---

## Suggested order of work

1. **Rule out the writers first (cheap):** check `Mode`/`FeedStatus` (Finding 2) and add a
   temporary log in `SolveTrackingMovement` (Finding 1) — reproduce the wedge and see which
   one is live.
2. **Fix Finding 1** (timer lifecycle + arbitration with `SlewToStar`) — it's a bug
   regardless of whether it caused this specific session.
3. **Fix Finding 2's arbitration rule** (one `if` plus a decision).
4. **Fix Finding 3** (per-constraint drive mode), then watch for Chaos log noise near limits
   (Finding 4).
5. **Calibrate** (Finding 5).

Note: fixes touching `MovingThing.h`/signatures require full editor restart + rebuild, not
Live Coding.

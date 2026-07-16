# Fixed Camera Views — Blueprint-Only Implementation Plan

Goal: several fixed camera vantage points of the telescope/dome, switchable with a keyboard
shortcut, with the current view name + shortcut shown in the UI. Everything in Blueprints/editor —
no C++ changes.

## Current state (verified)

- No GameMode override in `Config/DefaultEngine.ini` or in MainWorld's World Settings → the
  engine-default GameMode spawns the engine `DefaultPawn` (free-fly camera) at PlayerStart.
- `Content/Blueprints/StarPlatinumPlayerController.uasset` exists but is **empty and unused** —
  nothing references it. It is the natural home for the camera-switching logic.
- `WB_Controls` is created and added to viewport by the **MainWorld Level Blueprint**, which also
  sets `bShowMouseCursor` on the player controller. Keep that wiring as-is.
- Enhanced Input is the configured input system (`DefaultPlayerInputClass=EnhancedPlayerInput` in
  `Config/DefaultInput.ini`), but no Input Action / Mapping Context assets exist yet.

## Design summary

- A small `BP_ViewCamera` actor (CameraComponent + metadata) placed N times in MainWorld defines
  the fixed views. Adding a new view later = drop another instance in the level, no logic changes.
- `StarPlatinumPlayerController` gathers those actors at BeginPlay, sorted by an index variable,
  and switches between them (plus the free-fly pawn as view 0) with `Set View Target with Blend`.
- Input: Enhanced Input — one "cycle" action (**C**) plus optional direct-select actions (**1–4**,
  with **0** or cycling past the end returning to free-fly).
- UI: the controller fires an `OnViewChanged` dispatcher; a new small widget (or a new panel in
  `WB_Controls`) subscribes and displays "View: Dome Exterior (C to cycle)".

## Step 1 — `BP_ViewCamera` actor (Content/Blueprints/)

Blueprint class, parent **Actor**:

- Components: `CameraComponent` (root or child of root). Optionally set a wider/narrower FOV per
  instance.
- Instance-editable variables:
  - `DisplayName` (Text) — e.g. "Dome Exterior", "Telescope Close-Up".
  - `SortIndex` (Integer) — deterministic ordering in the cycle (Get All Actors Of Class returns
    arbitrary order, so an explicit index is required).
- No tick, no logic. It's pure placement + metadata.

Place 3–4 instances in MainWorld, e.g.:

1. **Overview** — high, outside, whole dome + horizon.
2. **Dome Exterior** — closer, sees shutter/vent motion.
3. **Telescope Close-Up** — inside the dome, framing the mount so azimuth/elevation/Cass motion is
   readable.
4. **Operator View** — near floor level inside, approximating a person standing in the dome.

Use the editor's **pilot actor** feature (right-click actor → Pilot) to frame each shot precisely.

## Step 2 — Enhanced Input assets (Content/Input/ — new folder)

- `IA_CycleView` — Input Action, Value Type: Digital (bool).
- `IA_SelectView` — Input Action, Value Type: **Axis1D (float)** so one action can carry which
  number key was pressed (see mapping below). Alternative: four separate digital actions
  `IA_View1..4` — simpler to read, more assets; either is fine, the plan assumes the scalar trick
  is skipped and **separate `IA_View0..IA_View4` digital actions** are used for clarity.
- `IMC_CameraViews` — Input Mapping Context:
  - `C` → `IA_CycleView`
  - `1`–`4` → `IA_View1..IA_View4`
  - `0` → `IA_View0` (return to free-fly)

Note: the engine `DefaultPawn` uses legacy input bindings internally; adding an Enhanced Input
mapping context does not interfere with its WASD/mouse flying.

## Step 3 — Wire up `StarPlatinumPlayerController`

The blueprint already exists; it finally gets used.

**BeginPlay:**

1. Get Enhanced Input Local Player Subsystem → **Add Mapping Context** `IMC_CameraViews`
   (priority 0).
2. `Get All Actors Of Class (BP_ViewCamera)` → sort by `SortIndex` (simple selection-sort loop, or
   insert into a Map keyed by index) → store as `ViewCameras` (array of `BP_ViewCamera` refs).
3. `CurrentViewIndex` (Integer) = 0, where **0 means free-fly pawn** and 1..N are the fixed
   cameras.
4. Fire the dispatcher once (see below) so the UI shows the initial state.

**Function `SetViewIndex(int NewIndex)`** — single choke point for all switching:

- Wrap/clamp `NewIndex` to `[0, Num(ViewCameras)]`.
- If 0: `Set View Target with Blend` → target = `Get Controlled Pawn`, blend ~0.5 s. Re-enable
  movement: `Set Ignore Move Input (false)` / `Set Ignore Look Input (false)`.
- Else: view target = `ViewCameras[NewIndex-1]`, same blend. Call
  `Set Ignore Move Input (true)` and `Set Ignore Look Input (true)` so the invisible pawn doesn't
  fly away underneath the fixed view (otherwise returning to free-fly puts you somewhere
  unexpected).
- Store `CurrentViewIndex`, then call dispatcher `OnViewChanged(DisplayName, ShortcutHint)`:
  - Index 0 → "Free Cam" / hint "C — next view, 1–4 — jump, 0 — free cam".
  - Else → `ViewCameras[i-1].DisplayName`.

**Input events:**

- `IA_CycleView (Triggered)` → `SetViewIndex(CurrentViewIndex + 1)` (wraps past N back to 0).
- `IA_View1..4 (Triggered)` → `SetViewIndex(1..4)` (ignore if index > Num(ViewCameras)).
- `IA_View0 (Triggered)` → `SetViewIndex(0)`.

**Event dispatcher:** `OnViewChanged(ViewName: Text, ShortcutHint: Text)`.

## Step 4 — Activate the controller via a GameMode

Blueprints-only, two small assets:

- `BP_StarPlatinumGameMode` (parent **GameModeBase**):
  - Player Controller Class = `StarPlatinumPlayerController`.
  - Default Pawn Class = `DefaultPawn` (unchanged — free-fly stays view 0).
- Set it in **Project Settings → Maps & Modes → Default GameMode** (writes
  `GlobalDefaultGameMode` to `Config/DefaultEngine.ini` — commit that file), or as the World
  Settings override on MainWorld. Project Settings is preferred so future maps inherit it.

## Step 5 — UI indicator

Two options; **Option A recommended** to avoid re-plumbing `WB_Controls`' creation flow:

- **Option A — new widget `WB_CameraIndicator`**: a small HUD-corner widget (view name + shortcut
  hint text). Created and added to viewport by `StarPlatinumPlayerController` itself at BeginPlay
  (after the camera array is built), then bound to its own `OnViewChanged` dispatcher. Keeps the
  camera feature self-contained in the controller; the Level Blueprint / `WB_Controls` wiring is
  untouched.
- **Option B — panel inside `WB_Controls`**: on Construct, `Get Player Controller` → cast to
  `StarPlatinumPlayerController` → bind to `OnViewChanged`. Integrates with the existing panel
  but couples `WB_Controls` to the controller class and risks a race if the widget constructs
  before the controller's BeginPlay fires its initial dispatch (mitigate by also pulling
  `CurrentViewIndex` directly on Construct).

Either way: bind via the dispatcher (event-driven), **not** property bindings evaluated every
frame — consistent with the project's push-based `FOnStateChanged` style.

## Step 6 — Input-focus gotcha

The Level Blueprint shows the mouse cursor for the UMG controls. If keyboard input ever stops
reaching the controller after clicking UI:

- Ensure input mode is **Game and UI** (not UI Only), with "Flush Input" unchecked.
- Set `Is Focusable = false` on `WB_Controls`' buttons or the root widget so a click doesn't leave
  keyboard focus on the widget.

## Verification (in-editor, no test suite)

1. PIE in MainWorld: confirm free-fly still works at start and `WB_Controls` still appears.
2. Press **C** repeatedly: cycles Free Cam → each fixed view in `SortIndex` order → back to Free
   Cam, with a smooth blend; indicator text updates each press.
3. Press **1–4 / 0**: jumps directly; out-of-range keys do nothing.
4. While on a fixed view, hold WASD, then return to free-fly: pawn should not have moved
   (ignore-move-input working).
5. Click a `WB_Controls` button, then press **C**: switching still works (focus gotcha).
6. Toggle Manual/Live mode and slew the telescope: fixed views frame the motion as intended.

## Asset checklist

| Asset | Location | New/Existing |
|---|---|---|
| `BP_ViewCamera` | `Content/Blueprints/` | New |
| `IA_CycleView`, `IA_View0..4` | `Content/Input/` | New |
| `IMC_CameraViews` | `Content/Input/` | New |
| `BP_StarPlatinumGameMode` | `Content/Blueprints/` | New |
| `WB_CameraIndicator` | `Content/` (next to other widgets) | New |
| `StarPlatinumPlayerController` | `Content/Blueprints/` | Existing (empty) — gains all logic |
| MainWorld | `Content/Maps` root | Edited — place camera actors |
| `Config/DefaultEngine.ini` | — | Edited — default GameMode |

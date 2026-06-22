# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

GeminiStarPlatinum is an Unreal Engine 5.6 real-time **physics simulation of the Gemini telescope and its observatory dome**. Movements are driven by physics constraints (not animation), and the longer-term intent is to mirror a live TCS (Telescope Control System) EPICS data feed. Single C++ Runtime module: `GeminiStarPlatinum`.

## Project Goals

These are the guiding objectives for the project. Weigh new work against them.

- **Immersive operator experience** — Recreate a simplified version of what telescope operators do in the command center. Users should be able to observe and interact with the live behavior of the telescope and dome, with access to the important positional data points (e.g. azimuth/elevation/Cassegrain angles, dome twist/shutter/vent state) as they move.
- **Streamlined, dual-audience data visualization** — Build a data interface that serves both laymen and engineers/operators. Support both at-a-glance world-space/camera-space figures (labels and readouts attached to the moving geometry) and detailed drill-down menus for precise numeric state. Keep the default view approachable while making the deeper data available on demand.
- **Maintainability and extensibility** — Contribute so future work doesn't require extensive re-learning. Favor clear MVC boundaries (models as source of truth, actors as views, coordinator/feed as the control source), document non-obvious physics and wiring decisions inline, and design new UI/data features so additional data points or panels can be added without rework.

## Build & Run

Workflow is **Editor + Live Coding** with a default Epic Games Launcher engine install (`C:\Program Files\Epic Games\UE_5.6`).

- **Run / open project**: open `GeminiStarPlatinum.uproject` (double-click or via Epic Launcher). If C++ binaries are stale it prompts to rebuild on launch. Default startup map is `/Game/MainWorld`.
- **Rebuild C++ while the editor is open**: **Live Coding** — `Ctrl+Alt+F11`. Prefer this for iterating on `.cpp` changes. Header/`UCLASS`/`UPROPERTY` signature changes generally require a full editor restart + rebuild.
- **Regenerate Visual Studio project files** (after adding/removing source files): right-click `GeminiStarPlatinum.uproject` → "Generate Visual Studio project files".
- **Full CLI build** (editor closed):
  ```
  "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" GeminiStarPlatinumEditor Win64 Development -Project="<abs path>\GeminiStarPlatinum.uproject" -WaitMutex
  ```
  Targets are defined in `Source/GeminiStarPlatinum.Target.cs` (Game) and `Source/GeminiStarPlatinumEditor.Target.cs` (Editor).

There is **no automated test suite** (no Automation specs, no test target). Don't go looking for one; verify changes by running the simulation in the editor.

## Architecture — MVC (migration in progress)

The codebase is mid-refactor toward an MVC split (noted explicitly in `AssemblyModel.h`). Understanding it requires reading the model subsystems and the actors together:

- **Models** — `UAssemblyModel` (abstract) → `UTelescopeModel`, `UDomeModel`. These are `UGameInstanceSubsystem` singletons that hold target state (e.g. `AzimTarget`/`ElevTarget`/`CassTarget`, dome twist/shutter/vent targets), clamp inputs to per-axis limits, and broadcast `FOnStateChanged`. They are the source of truth.
- **Actors / Views** — `AMovingThing` (base) → `AMovingTelescope`, `AMovingDome`. Physics actors that build their component + constraint hierarchy in the constructor and, each `Tick`, read the model's targets and drive `UPhysicsConstraintComponent`s toward them. Models are fetched lazily, e.g. `GetGameInstance()->GetSubsystem<UTelescopeModel>()`.
- **Coordinator / Feed** — `UObservatoryCoordinator` (holds `EControlMode { Manual, Live }`, meant to switch control source and broker the TCS EPICS API) and `ULiveDataFeed` (meant to pull live data). **Both are stubs right now.**

**Wiring status (important):** the telescope actor IS wired to `UTelescopeModel` and reads targets from it. The **dome actor is NOT yet wired** to `UDomeModel` — `AMovingDome` still drives from its own local fields, and the open/closed target values are duplicated between `UDomeModel::SetOpen` and `AMovingDome::Tick`. Keep both in sync until the dome is migrated.

## Physics conventions & gotchas

The physics tuning here is deliberate and fragile. Before changing physics behavior, understand these:

- **FluxCapacitor pattern** — both actors include a zero-mass helper body (`ParticleCube`) spun forever by a velocity drive. Its only purpose is to prevent UE's physics sleep system from halting slow/precise rotations. **Do not remove it.**
- **Velocity-then-snap drives** — `TwistComponent`/`SwingComponent`/`SlideComponent` (in `MovingThing.cpp` / `MovingDome.cpp`) apply a *velocity* drive when the error exceeds `AngularThreshold` / `LinearThreshold`, then switch to an *orientation/position spring* (snap) once within it. Angular targets are `FMath::UnwindDegrees`-clamped to [-180, 180], which intentionally limits rotation paths to mimic real mount limits.
- **Acceleration vs force mode** — `bAccelerationMode` (base default `true`, re-applied every tick) makes drives ignore mass and use acceleration units. `AMovingTelescope` overrides its Elev/Cass constraints to force mode (`SetAngularDriveAccelerationMode(false)`) because those bodies have gravity enabled.
- **Mass sensitivity** — there is a documented BUG: setting the elevation mass too high fails to stabilize. Treat masses as tuned values. Physics is configured globally in `Config/DefaultEngine.ini` under `[/Script/Engine.PhysicsSettings]` (substepping on, 16 substeps, high solver iteration counts, `bEnableEnhancedDeterminism`) and per-body in `AMovingThing::CreateMeshComponent` (sleep disabled, 50/30/100 position/velocity/projection iterations). These settings are load-bearing.
- **COM offsets** — telescope component center-of-mass offsets come from SolidWorks mass-property data (converted mm → cm). The inline comments record the source numbers.
- **Anchors** — `Base`/`Ground` bodies use mass ~`1e29` with all 6 DOF locked via `SetBaseLocked()` to act as the immovable inertial reference.

## Known incomplete / stubs

Don't assume these work — they're placeholders:

- `UObservatoryCoordinator::SetControlMode` — no-op (`return;`).
- `UAssemblyModel::ClampAndStore` — declared, not implemented in the `.cpp`, never called.
- `ULiveDataFeed`, `UMyGameInstanceSubsystem` — empty placeholder classes.
- `AMovingThing::CalculateCOMOffset` — marked `FIXME`, not functional.

## Source layout

- Newer MVC model/coordinator classes live in `Source/GeminiStarPlatinum/Public` + `Private` (`AssemblyModel`, `TelescopeModel`, `DomeModel`, `ObservatoryCoordinator`, `LiveDataFeed`, `MyGameInstanceSubsystem`).
- Older actor classes (`MovingThing`, `MovingDome`, `MovingTelescope`) sit flat in `Source/GeminiStarPlatinum/`.
- Module dependencies (`GeminiStarPlatinum.Build.cs`): `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`. Input uses **EnhancedInput**.
- Enabled plugins: `ModelingToolsEditorMode`, `VisualStudioTools` (Win64).
- Static mesh assets are loaded by path in actor constructors, e.g. `/Game/TelescopeModels/...` and `/Game/DomeModels/...`.

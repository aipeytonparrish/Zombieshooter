# Editor Setup — Zombieshooter

Step-by-step editor instructions for everything that can't be written as code. Each milestone adds a
section. Do them in order. Engine: **Unreal Engine 5.8** (installed at `C:\Program Files\Epic Games\UE_5.8`).

---

## 0. One-time machine setup (needed before anything compiles)

Unreal C++ needs Microsoft's C++ compiler. Install **one** of these (free):

- **Visual Studio 2022 Community 17.14 or newer**, or **Visual Studio 2026 Community** — https://visualstudio.microsoft.com/
  In the installer, tick these workloads:
  - **Game development with C++** (then on the right, also tick *Unreal Engine installer* and *IDE support for Unreal Engine* if shown)
  - **Desktop development with C++**
  - **.NET desktop development**
  Under *Individual components* make sure these are ticked:
  - *MSVC v143 – VS 2022 C++ x64/x86 build tools (Latest)* (VS 2026: *MSVC v145 …*)
  - *Windows 11 SDK (10.0.22621.0)* or newer
  - *.NET Framework 4.8 targeting pack* (or 4.6.2+)
  - *C++ Clang tools for Windows* (optional, used by some tooling)

Restart the PC after installing. Then tell Claude "VS is installed" and it will compile the project.

Git LFS is already set up for this repo (`git lfs install --local` was run). On another PC, run
`git lfs install` once after cloning.

---

## Milestone 1 — Project setup

### Opening the project
1. In File Explorer go to `C:\Users\Owner\Documents\Unreal Projects\Zombieshooter`.
2. Right-click **Zombieshooter.uproject** → **Generate Visual Studio project files** (optional; only needed if you
   want to browse the code in Visual Studio).
3. Double-click **Zombieshooter.uproject**. If asked *"The following modules are missing or built with a
   different engine version… Would you like to rebuild them now?"* click **Yes** (Claude normally builds it for you first).
4. The editor opens on an empty default map. That's expected — no maps exist yet.

### What's already configured (no action needed)
- Plugins enabled: **Enhanced Input, CommonUI, Online Subsystem + Online Subsystem Steam, StateTree, Modeling Tools**.
- Steam dev App ID **480** (Valve's test app). Steam only initialises when the Steam client is running; otherwise the
  game falls back to offline mode and still works. Change `SteamDevAppId` in `Config/DefaultEngine.ini` once you have a real App ID.
- Input actions and controller/keyboard bindings are **built in C++** (`UZSInputSet`) — there are no input assets to make.
- Lumen, Virtual Shadow Maps, TSR anti-aliasing, DX12 / SM6.
- Default game mode for every level: `ZSGameMode_Sandbox` (spawns the ZS player).

### Check the plugins (optional sanity check)
**Edit → Plugins**, search each of: `Enhanced Input`, `Common UI`, `Online Subsystem Steam`. Each should show as enabled.

### Content folder layout
Create these folders in the Content Browser as you start adding assets (right-click → New Folder).
`Docs/AssetRequests.md` lists what goes where.
```
Content/ZS/
  Maps/            Characters/Player/    Characters/Zombies/    Weapons/
  Bunker/          City/                 UI/                    Audio/
  VFX/             Materials/            Data/ (DataTables/CurveTables)
  Blueprints/      Test/
```

---

## Milestone 2 — Movement test map

### Build the test map (about 2 minutes)
1. **File → New Level… → Basic** → **Create**.
2. In the **Outliner**, select the actor named **Floor** and press **Delete** (the course builds its own floor).
3. In the **Place Actors** panel (or the Content Browser with **Settings → Show C++ Classes** turned on),
   go to **C++ Classes → Zombieshooter → Public → World** and drag **ZSMovementCourse** into the viewport.
4. With it selected, in the **Details** panel set **Location** to `X 0, Y 0, Z 0`.
5. Select the **PlayerStart** in the Outliner and set its Location to `X -800, Y 0, Z 100`, Rotation `Z (Yaw) 0`.
   (It must face +X, toward the course.)
6. **File → Save Current Level As…** → create folder `Content/ZS/Maps/Test/` → name it **`L_MovementTest`** → **Save**.
7. Optional, so the editor opens this map every time: **Edit → Project Settings → Maps & Modes** →
   **Editor Startup Map** = `L_MovementTest` and **Game Default Map** = `L_MovementTest`.

The world settings game mode is left blank on purpose — the project default (`ZSGameMode_Sandbox`) is used.

### Quick test without making a map (full screen)
Close the editor first, then run this in PowerShell. It opens the engine's basic template map, spawns the course
at runtime and starts the game full screen. Press Alt+F4 to quit.
```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\Owner\Documents\Unreal Projects\Zombieshooter\Zombieshooter.uproject" "/Engine/Maps/Templates/Template_Default?Course" -game -fullscreen
```

### Test it
1. Plug in the controller **before** pressing Play.
2. Press **Play** (Alt+P). Click into the viewport once so it has focus.
3. A green debug line at the top-left shows the movement state, speed, jumps left and dash cooldown.
   Toggle it with **D-pad Down** (or **F3**).

| Action | Controller | Keyboard / mouse |
|---|---|---|
| Move / look | Left stick / right stick | WASD / mouse |
| Sprint (tap to toggle, stops when you stop) | L3 | Hold Left Shift |
| Jump, double jump, mantle | A | Space |
| Crouch / **slide** (tap while sprinting) | B | Left Ctrl or C |
| Dash (1 in air, refills on landing / wall-run) | LB | Left Alt or Mouse Thumb 2 |
| Movement debug overlay | D-pad Down | F3 |

Lanes, left to right as you face the course:
1. **Dash gaps** – climb the step, sprint, jump, dash across 7 m; 10 m needs slide-hop + jump + double jump + dash.
2. **Mantle** – boxes 0.6 / 1.0 / 1.5 / 1.9 / 2.3 m (mantle), 3.0 m (too high from the ground — jump then press A
   again near the top), and a 2 m wall with a low gap above (mantles into a crouch).
3. **Slide** – sprint then tap B to slide under the 1.15 m tunnel; then go up the ramp and slide down it to gain speed.
   Tap A during a slide to slide-hop (keeps momentum).
4. **Wall-run** – from the platform, sprint and jump toward the tall wall on your right while holding forward. You run
   along it; press A to wall-jump off.
5. **Wall-jump corridor** – chain wall-runs/jumps between the two walls; mantle onto the 4 m ledge at the end.
6. **Steps** – 2.5 m steps: jump + double jump, or jump and mantle the edge.

### Tuning from the console (press ` or ~ on the keyboard during Play)
| Command | Effect |
|---|---|
| `ZSFov 95` | Field of view (70–110, default 80) |
| `ZSSens 1.2 0.8` | Controller look sensitivity, mouse sensitivity |
| `ZSDeadZone 0.1 0.12 0.96` | Right-stick inner, left-stick inner, outer dead zone |
| `ZSCurve 2` | Controller look response curve (1 = linear … 4 = very precise near centre) |
| `ZSHeadBob 0` / `1` | Head bob off / on |
| `ZSInvertY 1` | Invert look Y (controller + mouse) |
| `ZSToggleSprint 0` | L3 must be held instead of toggled |
| `ZSToggleCrouch 0` | Crouch button must be held |
| `ZSMoveDebug` | Toggle the debug overlay |

These save to `Saved/Config/Windows/GameUserSettings.ini` and are the same values the settings menu (Milestone 14) will edit.

### Tuning movement values
All movement numbers (speeds, slide, wall-run, mantle, dash, double jump) are properties on
**ZSMovementComponent** under the **ZS|Movement** categories. To tune them without code:
1. Content Browser → `Content/ZS/Blueprints/` → right-click → **Blueprint Class** → **All Classes** → search
   `ZSPlayerCharacter` → name it **`BP_ZSPlayerCharacter`**.
2. Open it, select **CharacterMovement (ZSMovementComponent)** in Components, edit values in Details, Compile, Save.
3. To make the game use it: create a Blueprint child of `ZSGameMode_Sandbox` named **`BP_ZSGameMode_Sandbox`**, set its
   **Default Pawn Class** to `BP_ZSPlayerCharacter`, then in **Project Settings → Maps & Modes** set **Default GameMode**
   to `BP_ZSGameMode_Sandbox`.
   Camera-feel values (head bob, tilt, landing dip, FOV kicks) are on the **FirstPersonCamera** component in the same Blueprint.

Tell Claude what feels off (e.g. "slide too short", "wall-run drops too fast") and it will change the defaults in code.

### Known placeholders
- No arms or weapon are visible yet (arms mesh slot exists: `FirstPersonArms`; see `Docs/AssetRequests.md`).
- Weapon sway / inertia: done in Milestone 3.
- `Escape` stops Play-In-Editor; it becomes the pause menu in a packaged build / later milestone. Use **Shift+F1** to free the mouse in PIE.

---

## Milestone 3 — Gunplay

### Before the first Play (one time per PC)
The rifle is built from the owner's RD-704 pack, which has to be converted into Unreal assets once:
1. Close the editor.
2. Run in PowerShell (from the project folder):
   `powershell -ExecutionPolicy Bypass -File Tools\Import\run_imports.ps1`
   (~1 minute; it creates `Content/ZS_Imported/`, which is git-ignored.)
3. If you skip this the game still runs — the rifle just shows as grey placeholder boxes.

The first-person arms come from the **AAA FPS Gloves** pack (`Content/AAA_FPS_Gloves/`). If that folder is missing
(fresh clone), the guns still work but no arms are shown — restore the packs (`Docs/ThirdPartyAssets.md`).

### Test it
Use the quick-test command from Milestone 2 (or your `L_MovementTest` map), then type `ZSRange` in the console
(` key) to jump to the **shooting range** (lane 7, far right of the course).

| Action | Controller | Keyboard / mouse |
|---|---|---|
| Fire | RT | Left mouse |
| Aim down sights | LT (hold) | Right mouse (hold) |
| Reload | X | R |
| Switch weapon (rifle / pistol) | Y | Mouse wheel |
| Fire mode (auto / burst / semi) | D-pad Up | B |

The range has targets at 10 / 20 / 35 / 50 m, one strafing target (aim-assist test), a 20 cm wall the rifle
shoots through and a 60 cm wall it can't. Targets show their health, flash when hit, fall over at 0 and stand up
again after 3 s. Headshots (the ball) do double damage.

What you should feel:
- **Recoil** climbs as you hold the trigger; pull down on the stick/mouse to fight it. When you let go, the view
  settles most of the way back (not past where you aimed).
- **Hip-fire spread** grows while moving / jumping / firing — the crosshair opens to show it. Aiming tightens it.
- **Aim assist (controller only):** the stick slows down over a target, aiming in near a target snaps toward it,
  and it gently follows a moving target while you move or look. It never moves your aim when you're idle.
- **Reloads:** tactical (rounds left, faster, keeps one in the chamber: 31/30) vs empty (slower, racks the gun).
  Firing cancels a tactical reload that hasn't inserted the mag yet.
- Sprinting lowers the gun; pulling the trigger or aiming ends the sprint. Rumble on every shot, stronger on kills.

### Console commands
| Command | Effect |
|---|---|
| `ZSRange` | Teleport to the shooting range |
| `ZSGiveAmmo` | Refill all weapons |
| `ZSAimAssist 1` | Aim-assist strength (0 = off, 1 = default, 1.5 = strong) |
| `ZSDamageNumbers 0` / `1` | Floating damage numbers off / on |
| `zs.ShowImpacts 0` / `1` / `2` | Impact dots off / on / dots + tracers |

### Known placeholders
- No first-person animations exist yet, so the arms are **posed by code** (IK onto the grips) and reloads are a
  procedural tilt + magazine drop. Real animations replace this when an FPS animation pack arrives.
- The pistol is grey boxes (no pistol mesh supplied yet). No weapon sounds, muzzle-flash effect or shell casings yet
  — only a muzzle light. See `Docs/AssetRequests.md`.
- The RD-704's front sight post shows inside the scope (it's part of the receiver mesh).

---

## Milestone 4 — Zombies v1 (Walker + Runner)

### One-time
- Run `Tools\Import\run_imports.ps1` again (editor closed) — it now also creates the blood decal material
  (`Content/ZS/Materials/M_ZS_BloodDecal`, committed to git, so on this PC it's already there).
- Zombies use the owner's **ZombieCity01** and **ZombieDoctor** packs with their own animations — no setup needed.

### NavMesh for your test map (so zombies path around obstacles)
In `L_MovementTest` (or any map): **Place Actors → search "Nav Mesh Bounds Volume"** → drag it in → in Details set
**Brush Settings X 12000, Y 12000, Z 2000** and Location `X 3000, Y 2000, Z 0` so it covers the whole course →
press **P** in the viewport to see the green walkable area → Save. Without a NavMesh (e.g. the quick-test command)
zombies walk straight at you and can get stuck on walls — that's expected.

### Test it
In Play, open the console (`) and type:

| Command | Effect |
|---|---|
| `ZSZombies 8` | Spawn 8 zombies 12–25 m ahead (mix of walkers and runners) |
| `ZSZombies 6 0` / `ZSZombies 6 1` | Walkers only / runners only (third number = spread in degrees, default 55) |
| `ZSKillZombies` | Remove all zombies |
| `ZSGod` | Toggle invulnerability |

- **Walkers** shamble (95–140 cm/s), 110 HP. **Runners** often roar first, then sprint (390–470 cm/s), 75 HP.
- Random character (hoodie or doctor) and skin (3 colour variants each).
- They attack in melee (20 damage); your health regenerates after 4 s without damage. In this sandbox you respawn 3 s
  after dying (the round system in Milestone 5 turns that into game over).
- Shooting: headshots (×2) hit the real head bone; they flinch when hit (always on headshots); on death they go
  **ragdoll** and get shoved by the killing shot. Blood droplets spray on every hit, splats land on walls / floor behind
  them, and a pool forms under bodies. Bodies are removed after 25 s.

### Known placeholders
- Basic blood only (code droplets + decal). Niagara blood spray, screen blood and dismemberment come in Milestone 11.
- No zombie sounds yet (none supplied). The new Mocap_Zombie / Hitreact packs add variety in Milestone 11.

---

## Gunplay upgrade — real first-person animations (after Milestone 4)

The guns now use the owner's Fab packs when they are installed:
- **M4 Tactical (slot 1)** — InfimaGames *Tactical FPS Animations*: idle / walk / sprint, iron-sight aim pose, semi + aimed
  fire, tactical + empty reloads with the pack's own reload sounds, quick equip / holster, AR gunshot + tail, muzzle flash.
- **P9 Sidearm (slot 2)** — ProFPS *Pistol V2*: idle / walk / sprint, ADS, fire, tactical + empty reloads (slide lock),
  draw / holster; RealWeaponSoundFX pistol shots (5 variations) with timed mag-out / mag-in / slide sounds.
- **RD-704 (slot 3)** — the owner's modular AK, still code-posed (no animation pack for it).

Y / mouse wheel cycles the three. Your gloved arms (AAA FPS Gloves) copy the animated pose, so outfits still swap arms.
If a pack is missing on a PC, that gun falls back to grey placeholder boxes with code-posed arms — the game still runs.

Known: the InfimaGames muzzle smoke renders slightly pink (effect colour parameter not set yet); weapon sprint / jump
transition clips are not used yet (loops only).

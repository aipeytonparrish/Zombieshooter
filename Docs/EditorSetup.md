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

---

## Milestone 5 — Rounds, crates, pickups, game over

### The test map (no setup needed)
`Content/ZS/Maps/Test/L_ZSTest` is generated by `Tools/Import/create_test_map.py` and is now the **default map** for
the editor and the game. It has the movement course, a **NavMesh** (built at runtime), 18 zombie spawn points around
the course, 3 unlimited ammo crates and the **Survival** game mode (World Settings → GameMode Override). Just press
**Play** in the editor, or launch the game normally. To rebuild it: `Tools\Import\run_imports.ps1` (editor closed).

### How a run plays
- 8 s **GET READY**, then **ROUND 1**. Each round has a zombie budget; zombies spawn out of your sight (behind walls
  or outside your view) and walk in. Kill them all → **ROUND N SURVIVED** + 12 s break → next round.
- Rounds scale from `Data/ZSRounds.csv` (→ CurveTable `CT_ZSRounds`): round 1 = 9 zombies, round 10 = 44,
  round 20 = 106; health +10 % per round, a little faster each round, runners from round 3 up to 60 %, faster
  spawns, up to 32 alive at once. Edit the CSV and re-run the import script to retune.
- **Ammo crates** (your ammo-box meshes): look at one and press **X** (controller) / **E** (keyboard) → full reserve
  for every gun, any number of times. X still reloads when you're not looking at a crate.
- **Pickups:** ~7 % of kills drop a spinning **green box (+50 health)** or **blue plate (+1 armor plate)**. They blink
  and vanish after 30 s. Armor holds up to 3 plates (shown as 3 blue segments above the health bar) and soaks 70 %
  of incoming damage until it runs out. Health still regenerates after 4 s without damage.
- **Game over** when you die: round reached, kills, headshots, time; the run restarts after 8 s.
  (Milestone 6 adds XP / cash / losing your deployed gear and a proper results screen.)

### Console commands (survival)
| Command | Effect |
|---|---|
| `ZSRound 10` | Jump to round 10 |
| `ZSZombieInfo` | Log every zombie's distance / state / pathing to the Output Log |
| `ZSKillZombies` | Kill all zombies (they still count toward the round) |
| `ZSGod` | Invulnerable |

The old quick-test command still works and is a **sandbox** (no rounds); add `?game=Survival` to get rounds there too:
`"/Engine/Maps/Templates/Template_Default?Course?game=Survival"`.

### Placing survival things in your own levels
Place Actors → search **ZSSpawnPoint** (red capsule = zombie size), **ZSAmmoCrate**, **ZSPickup** (set Type and
Lifetime 0 for a permanent one), plus a **Nav Mesh Bounds Volume** covering the playable area, and set the level's
**World Settings → GameMode Override** to `ZSGameMode_Survival`.

### Known placeholders
- Round start / end stingers use explosion sounds from GunSoundsPackVol1 until proper music stingers exist.
- Pickup visuals are simple glowing shapes; lethal / tactical refills arrive with grenades, power-ups in Milestone 11.
- The round-20 boss is Milestone 12 — rounds currently continue past 20.

---

## Milestone 6 — Profile, economy, risk

### What's saved
`Saved/SaveGames/Profile0.sav` (UZSProfileSubsystem): level + XP (55 levels, curve in `Data/ZSLevels.csv` →
`CT_ZSLevels`), cash, owned weapons (each a unique instance, ready for per-gun attachments), the deployed loadout and
lifetime stats (runs, wins, kills, headshots, best round, play time, weapons lost). A new profile starts with the
starter guns (M4 Tactical + P9 Sidearm). Saved at deploy and at the end of every run.

### Earning
| Event | XP | Cash |
|---|---|---|
| Kill (walker) | 10 | $10 |
| Runner | +5 | +$5 |
| Headshot | +5 | +$5 |
| Multi-kill (each extra kill within 1.2 s) | +10 | — |
| Round survived | 100 + 25 × round | $50 × round |
| **Win** (clear round 20 until the boss exists) | +2,500 | +$1,000 |

Run earnings show top-right, marked **AT RISK**; popups appear next to the crosshair.

### The risk rules
- **Win:** keep your loadout, bank all run XP / cash plus the win bonus.
- **Fail** (death, *Abandon Run*, quitting, or force-quitting the game): the deployed guns are **removed from your
  inventory** and the run's XP / cash are **not banked**. Force-quit is caught on the next launch (the deploy writes an
  "active run" flag to the save) and the get-ready screen tells you what was lost.
- At deploy the get-ready screen lists what is **AT RISK**.
- Until the market exists (Milestone 8), missing starter guns are re-issued for free at deploy so a loss can't leave
  you unarmed. Stopping Play-In-Editor mid-run counts as a force quit too (intended).

### Results screen
After every run: outcome, round, kills, headshots, time, XP / cash (kept with the win bonus, or LOST), gear lost,
level bar, level-ups and profile cash. **A / Enter** continues (restarts the run; the bunker replaces this in M7).

### Pause menu
**Start** (controller) / **Esc** (packaged game) → Resume, **Abandon Run** (asks twice, counts as a fail), Quit Game
(also a fail mid-run). D-pad / arrows to move, A / Enter to select, B / Backspace back.

### Console commands
| Command | Effect |
|---|---|
| `ZSProfile` | Log level, XP, cash, weapons, stats |
| `ZSGiveCash 5000` / `ZSGiveXP 10000` | Debug grants |
| `ZSResetProfile` | Wipe the save |
| `ZSWinRun` | End the current run as a win |

## Milestone 7 — The bunker

The game now **starts in the bunker** (`/Game/ZS/Maps/L_Bunker`, also the editor startup map). It's a placeholder
concrete room (`AZSBunkerLayout` builds it from code, so the map just stores the actor) with five stations. Walk up to
one and press **Interact** (X on controller / E):

| Station | What it does |
|---|---|
| **MAP TABLE** | Pick a level (Training Yard now; Midtown Block and Lexington Line show as coming soon / locked by level), see what's **AT RISK**, **DEPLOY** |
| **ARMORY** | 5 loadout presets, pick primary / sidearm, attachments, sell |
| **MARKET** | Today's guns for sale |
| **LOCKER** | Character creator (Milestone 9) |
| **MISSION BOARD** | Missions (Milestone 10) |

No weapons in the bunker. Top-left shows level, XP bar, cash and the active loadout. After a run, **Continue** on the
results screen brings you back here. If the previous run was force-quit, the bunker says what was lost.

**To rebuild the bunker map:** close the editor, then run `Tools/Import/run_imports.ps1` (or delete
`Content/ZS/Maps/L_Bunker.umap` and run `create_bunker_map.py` with the editor's `-run=pythonscript`).
To play survival directly in the editor, open `L_ZSTest` and press Play as before.

## Milestone 8 — Loadouts, attachments, market

- **5 loadout presets** (Armory → LOADOUT 1–5). Choose *Deploy with this loadout* to make one active. Only the active
  one is taken on a run (and risked).
- **Attachments are bought for one specific gun** (Armory → pick a gun). Up to 5 per gun, one per slot (optic, muzzle,
  underbarrel, magazine, stock). They change the gun's stats (shown in the panel) and are lost with the gun. Removing
  one scraps it. On the RD-704 the suppressor, muzzle brake and vertical grip also show on the model; the M4 uses its
  animated pack mesh, so its attachments are stats-only for now.
- **Market:** starters (P9, M4) are always stocked; other guns rotate daily (UTC midnight, same stock for everyone on a
  given day) and one item is the **deal of the day** at 25 % off. Guns have unlock levels (RD-704: level 4).
- **Sell** any gun at the armory for 40 % of what it and its attachments cost.
- The free starter re-issue from Milestone 6 is gone: losing a gun is real now. If you own **no** weapons at all, the
  quartermaster lends you a P9 at deploy so you can never be stuck.

| Attachment | Slot | Price | Level | Effect |
|---|---|---|---|---|
| Reflex Sight | Optic | $450 | 2 | −12 % ADS time, a bit more zoom |
| Suppressor | Muzzle | $700 | 3 | −10 % recoil, −15 % damage range |
| Muzzle Brake | Muzzle | $550 | 2 | −18 % vertical recoil |
| Vertical Grip | Underbarrel | $500 | 2 | −12 % recoil, −10 % hip spread |
| Extended Mag | Magazine | $650 | 5 | +50 % mag, +10 % reload time |
| Light Stock | Stock | $400 | 6 | −15 % ADS time and moving spread, +5 % recoil |

Prices, levels and stats live in `Source/Zombieshooter/Private/Weapons/ZSWeaponCatalog.cpp` (DataTables later).

## Milestone 9 — Character creator

Bunker → **LOCKER**. The camera swings to your survivor standing next to the bench.

- **Survivor:** 5 bodies from the Survivors Mega Pack (Gas-Mask Scout, Street Runner, Salvager, Hazmat Worker, Night
  Watch). Some unlock with level (3, 6, 10).
- **Match gloves to outfit:** ON picks gloves + tint that suit the body. Turn it OFF to choose your own:
- **Gloves:** the 11 AAA FPS Gloves arm sets (more unlock at levels 2, 5, 8).
- **Glove tint:** Factory, Blackout, Olive, Desert Tan, Navy, Blood Red, Arctic, Urban Grey.

Your choice is saved in the profile and is **never lost** on a failed run. In a run your first-person arms use the
chosen gloves and tint, and the survivor body casts your shadow.

**Over-the-shoulder camera (experiment):** type `ZSThirdPerson` in the console to toggle a third-person view of your
survivor (idle / run animations only, no aiming poses yet). First person stays the real game camera.

### Bunker art (Bunker Fab pack)
The bunker now uses the **Bunker** pack you supplied (`Content/Bunker`, UE 4.25 content, gitignored like the other
packs — the backup is `Desktop/Enviorments Assets/Bunker.rar`). `create_bunker_map.py` streams the pack's
`/Game/Bunker/Maps/Demo` level into `L_Bunker` (always loaded) and places the five stations along its main pipe
corridor; you spawn at the pack's own start. Without the pack the script falls back to the code-built placeholder
room. **Never save the Demo map itself** — L_Bunker only references it.

The other archives in that folder: Shelter (split .rar, UE content), Underground_Tunnel (UE), V2Bunker 4.26 (UE),
cold-war-underground-bunker (UE 5.0) are good candidates for later levels; Bunker_pack is a Unity package and the
.blend files need a Blender export before Unreal can use them.

## Milestone 10 — Missions and timed tasks

**Mission board** (bunker): 3 **dailies** (new set every day at 00:00 UTC), 2 **weeklies** (every Monday UTC) and the
**story chain** (7 chapters, one at a time, ending with "win a run"). Rewards (XP + cash) are paid automatically.
**Progress only counts when you win the run** — die or abandon and that run's mission progress is lost (the results
screen says so). During a run, up to 3 unfinished missions are tracked on the left with live progress.

**Timed tasks** in a run (every 3rd round, a few seconds in; types rotate):
| Task | Goal |
|---|---|
| Kill Rush | Kill 8 + round zombies in ~40 s |
| Hold the Zone | Stand inside the orange ring for 20 s (total) within 60 s |
| Supply Beacon | Reach the blue beacon within 30 s |

A diamond with the distance points to the ring / beacon (at the screen edge when it's behind you). Completing a task
pays 150 + 25 × round XP and $100 + 10 × round (run earnings, at risk) and counts toward missions.

Console: `ZSTask 1|2|3` starts a task now.

## Milestone 11 — Gore, special zombies, power-ups

### Special zombies (join from round 4; chances grow with the round)
| Zombie | From round | What it does | Tell |
|---|---|---|---|
| **Crawler** | 4 | Legless, low to the ground, slow, bites | crawling |
| **Bloater** | 5 | Rushes you, swells and bursts at close range (or when killed): big area damage that also kills other zombies | orange glow |
| **Spitter** | 7 | Lobs acid from range; the glob leaves a burning pool | green glow |
| **Screamer** | 9 | Shrieks: nearby zombies get 45 % faster for 7 s | purple glow |
| **Brute** | 10 | Huge (900 HP), never flinches, charges, knocks you back (max 2 alive) | dark red glow, 1.4× size |

Any walker or runner that loses a leg drops to the ground and becomes a crawler. Crawling uses the mannequin crawl
clips from *Zombie_deaths_crawls* (the zombie skeletons are marked compatible at runtime — same bone names).
Specials pay +20 XP/cash (Brute +150).

### Gore
- Arms (forearm / upper arm) and legs (calf / thigh) come off after enough damage to that limb or from a heavy killing
  shot; heads pop on big headshot kills. Severed stumps pump blood for a few seconds; flesh chunks fly.
- Blood decals build up for the whole run (up to 256 on Full).
- **Screen blood** when gore happens right in front of you (Bloater bursts especially).
- **Gore setting:** `ZSGore 0` off (no blood at all), `ZSGore 1` reduced (blood, no dismemberment), `ZSGore 2` full
  (default). Saved in GameUserSettings; the options menu gets it in Milestone 14.

### Power-ups (2.2 % per kill, one on the ground at a time)
| Power-up | Effect |
|---|---|
| **OVERKILL** | Double damage, 20 s |
| **ONE SHOT** | Every hit kills, 15 s |
| **PAYDAY** | Double run XP and cash, 30 s |
| **SCORCHED EARTH** | Kills every zombie on the map (you get the kills) |

They float as glowing orbs with their name; active ones show as timer bars bottom-centre.

Console: `ZSZombies <count> <type>` (3 crawler, 4 bloater, 5 spitter, 6 brute, 7 screamer, 8 one of each),
`ZSPowerUp 1-4`, `ZSHurtBone <bone> <damage>` (gore testing).

## Milestone 12 — The boss and the win

**Round 20 is the boss round.** *The Abomination* (16,000 HP, 2.3× size) arrives with a small escort; the round
ends only when it dies. A health bar with phase marks sits at the top of the screen and callouts tell you what it's
doing.

| Phase | Health | New moves |
|---|---|---|
| 1 | 100–66 % | Swipes (knockback) and the **Ground Slam**: a red ring appears around it for ~1.3 s — jump or get out. After a slam it's **stunned for 3 s: weak points (head, chest) take triple damage** (bar turns gold) |
| 2 | 66–33 % | Roars in adds (armoured while roaring), **hurls debris** at an orange ring where you're standing |
| 3 | < 33 % | Enraged: faster, slams more often, **charges** |

Killing it: slow motion, "THE ABOMINATION IS DEAD", every remaining zombie drops, +2,000 XP / $1,000, then the
results screen as a **win** (loadout kept, everything banked, win bonus, mission progress banked).
One Shot is capped against the boss; Scorched Earth doesn't touch it.

### Run length (target 40–75 min)
Rounds 1–19 now total ~1,130 zombies (counts ×1.25) and breaks are 20 s. At ~1.5 s per zombie (strong player) that
is ~38 min plus breaks and the boss ≈ 45 min; at ~2.5 s per zombie ≈ 60 min. Tune in `Data/ZSRounds.csv` (re-run
`create_core_materials.py` to rebuild `CT_ZSRounds`) and `BreakDuration` on the round manager. The boss model is a
scaled zombie until a mutant mesh arrives (see AssetRequests).

Console: `ZSBoss` jumps to the boss round.

## Milestone 13 — Prestige and long progression

**Levels 1–55.** A won run banks roughly 35–40k XP (kills, rounds, tasks, boss, win bonus, missions), and the curve
(`Data/ZSLevels.csv` → `CT_ZSLevels`) needs ~1.05M XP for level 55, so the first climb is ~28 wins (25–35 hours of
winning play, more with losses, since failed runs bank nothing). Levels unlock market guns, attachments, survivors,
gloves and maps; the results screen lists each unlock as it happens.

**Prestige** (mission board, top entry). At level 55 the entry says READY:
- Your level goes back to 1 and your prestige rank goes up (1–25).
- **Nothing is taken away:** weapons, attachments, cash, outfits and every unlock are kept (unlocks stay open once
  you have prestiged at least once).
- Climbing back to 55 takes 60% of the normal XP (~14 hours per rank).

| Each rank gives | |
|---|---|
| Permanent bonus | +2% XP and cash on every won run (max +40% at rank 20) — shown as a line on the results screen |
| Payout | $2,500 × rank |
| Emblem | Bronze (1–4), Silver (5–9), Gold (10–14), Platinum (15–19), Obsidian (20–24), Master (25), shown in the bunker panel and the menu headers |
| Exclusive glove tints | Bronze Rank (1), Hazard Orange (3), Silver Rank (5), Toxic (8), Gold Rank (10), Bone (12), Platinum Rank (15), Ember (18), Obsidian Rank (20), Master Crimson (25) — Locker → GLOVE TINT |

The bunker panel shows "PRESTIGE READY - MISSION BOARD" at level 55. Selecting PRESTIGE shows the next rank's
rewards; confirm on the second page.

Console: `ZSGiveXP 2000000` (to level 55), `ZSPrestige` (prestige now), `ZSPrestige 7` (set the rank directly).

## New York level — Riverside Ruins (from your post-apocalyptic NY scene)

Map table → **New York - Riverside Ruins** (level 1). A ruined street by the river on a grey, stormy afternoon:
brownstones, rusted cars, debris piles, dead trees and street lamps along the riverbank, Megascans NYC buildings
along the east side and the north end. Play area ~42 × 140 m along the street (invisible walls at the river and
the far facades), 3 ammo crates.

**The compound (classic round-based layout):** you start in the start room at the north end of the street; behind it
is a 40 x 60 m compound built from your industrial modular kit (`Tools/Import/nyc_compound.py` has the floor plan):

| Area | How you get in | Cost |
|---|---|---|
| Start room | — | — |
| Hallway + Offices + Storage | HALLWAY door (start room, north wall) | $750 |
| Locker room | LOCKER ROOM door (offices or hallway) | $1000 |
| Generator room | GENERATOR ROOM door (storage or hallway) | $1000 |
| Warehouse (6 m high) | WAREHOUSE door (locker room, hallway or generator room) | $1250 |
| Courtyard — outdoor fight area 2 | LOADING BAY (warehouse, north wall) | $1500 |
| The street — outdoor fight area 1 | STREET DOOR (start room, south wall) | $350 |

- **Zombies come from outside**, like the classic mode: each room has boarded windows (`AZSBarricade`, 19 in all) with
  a spawn point outside. Zombies walk up, rip the boards off one by one and climb in. Stand at a window and press
  **Interact** to nail a board back (+$10 each). The street and courtyard also have ground spawns.
- Ammo crates: offices, warehouse, courtyard, start room (plus the three on the street).
- Reusable pieces: `AZSBuyDoor`, `AZSBarricade` (opening size, sill height, boards, repair reward),
  `AZSSpawnPoint` **Zone** / **Min Player Distance** / **Barricade**, and `AZSKitInstances` (draws thousands of kit
  pieces as a handful of instanced meshes).

**Intensity (all maps):** denser rounds from round 1 (14 alive at once, quicker spawns, sprinters from round 2,
faster zombies later); **horde surges** from round 3 — about 9 s into a round and then every 30 s a burst of
4 + round/3 zombies pours in with a "HORDE INCOMING" warning, a quarter more of them sprinters; zombies spawn
closer (9–35 m). Zombies that make no progress for 8 s (stuck on debris, lost behind a wall) are moved to a fresh
spawn point out of view.

Weather (all in `L_NYC`, rebuild with `Tools/Import/create_nyc_map.py`):
- **Clouds:** storm-coverage volumetric clouds (`MI_ZS_OvercastClouds`), weak soft sun, real-time sky light.
- **Fog:** light height fog with volumetric fog, cool grey grade, slight negative exposure.
- **Rain:** `AZSRain` (our C++ actor): 4,000 streaks simulated around the camera in world space. Tune it on the
  `ZSRain` actor (count, box size, speed, streak size, opacity, colour, wind). **Shelter Boxes** keep it out of
  the safehouse.
- **Lightning:** `AZSRain` also flashes the sun and sky light every 14–35 s, with thunder a moment later
  (a big explosion sound pitched way down until there's a real thunder sound).

Rebuilding the art (only needed on a new PC or after changing the scene) is in `Docs/ThirdPartyAssets.md`
("New York level"). Layout helper: console `ZSHeightMap -7000 5000 -9000 9000 250` writes a floor-height grid to
`Saved/ZSHeightMap.txt` (used to place the play area, spawns and crates).

Known: the art was a render scene. The rubble piles are low-poly by design (the scene calls them
"debrispilelowres"); they need a rubble pack to replace (Docs/AssetRequests.md). The river surface is solid
(the walls keep you off it).

Test helpers: `ZSRunCash 400` (run cash for this run only), `ZSUseDoors` (uses every buy door, i.e. opens the
whole compound). Launches with
`-ZSAutoShot=` never write your save any more (they used to count as force-quit runs).

## Combat feel, gore, HUD pass

- **Melee:** right stick click (V / mouse thumb button on keyboard). The AR plays its rifle-butt bash, other guns a
  quick jab. 150 damage (kills early-round zombies in one hit), shoves survivors back, works as a panic button.
- **Hits:** every bullet physically jolts the body part it hits (head snaps back, legs buckle), stutters the zombie's
  stride and shoves it on heavy hits; a tick on every hit and a heavy thud on kills; a camera punch on every shot.
- **Impacts:** bullet holes, dust and chips on concrete / dirt / wood, sparks off metal, and an impact sound per surface
  (the surface is guessed from the material / mesh name). The orange debug dots are off (`zs.ShowImpacts 1` brings
  them back for tuning).
- **Blood:** our own procedural splatter textures and particles: exit mist, droplets, splats that run out on walls and
  floors, pools that spread under bodies, wet gibs that trail blood and splat where they land. Rebuild the textures and
  materials with `Tools/Import/gen_fx_textures.py Saved/GeneratedFX` (plain Python) then `create_gore_fx.py` (editor).
- **Movement:** sprint (left stick click) then crouch (B) to slide; slides are longer and faster, jump out of one to
  keep the speed, and if a slide runs out while you're still pushing forward you're straight back into the sprint.
- **HUD:** Left 4 Dead style plates — survivor card (name, level, big health number and bar in green / yellow / red,
  white damage trail, armour plates), weapon card (name, fire mode, big magazine count, reserve, a pip per round),
  the round number painted over a blood smear; the screen drains to black and white near death.
- **Home base:** a mood volume over the bunker (a stop darker, more contrast, cooler, vignette, grain) and the stations
  are worn-steel benches (they rendered plain white before).

## The Rooftop — the new safe house

The hub between runs is now **The Rooftop** (`L_Rooftop`, built by `Tools/Import/create_rooftop_map.py`): a survivors'
camp on top of a New York tower at golden hour, ~55 m above the dead city (`L_Bunker` is still there but no longer
used). It's the game's default map and where runs return to.

- **The camp:** a camp fire with benches, fire barrels, string lights, tarp canopies with bedrolls, a rooftop garden,
  a water tower, antennas, a flag and a billboard, the closed-off stair house, and the city skyline all round.
- **The people** (`AZSSurvivorNPC`): Marcus the quartermaster (armory), Rosa the trader (market), Sarge (map table),
  Doc Avery (medic), Jonah on lookout, Vic the mechanic, and Lena, Teddy and Milo round the fire. Each one idles with
  a life-like loop from the MC Idles pack (warming hands, arms crossed, wall lean, smoking, music), shows a name tag up
  close and says a line when you walk up. Bodies are the Survivors Mega Pack; the idle pack's mannequin drives their
  pose (leader pose), so any MC Idles loop works on any survivor.
- The five stations work exactly as before (walk up, Interact).

## New York — outside, and the window climb

- Street dressing (`Tools/Import/nyc_dressing.py`): street lights (a few still burning), traffic lights, signs,
  hydrants, trash, benches, a bus stop, guardrails along the river, dead trees, burning barrels and fires in the rubble,
  old blood on the street and through the compound.
- The rubble piles rendered as black shards; their material now uses the Megascans concrete
  (`import_postapoc_ny.py --materials-only`).
- Zombies climbing through a window dive through head-first (a standing body clipped through the wall above it).
- Every local light in the New York and Rooftop maps is shadowless: shadowed point lights with Nanite virtual shadow
  maps hung the GPU on load.
- Test helpers: `-ZSAutoAim` keeps the view on the nearest visible zombie (with `-ZSAutoBursts` for damage tests);
  `ZSZombieInfo` now prints health and hit-box drift.

## Guns: levels, camos, wall buys, the mystery box

- **Gun levels:** every gun type starts at level 1 (max 30). Kills with it level it up (100 XP, 150 for a headshot)
  whether it's your own gun, a wall buy or a box gun, and the level is kept even if you lose the gun. Levels unlock
  that gun's attachments (Reflex 2, Brake 4, Grip 6, Suppressor 8, Extended Mag 10, Light Stock 12) and camos
  (Woodland 3, Desert 6, Urban 9, Arctic 12, Tiger 15, Crimson 18, Blood-Soaked 21, Gold 25, Dark Matter 30).
- **Customising** happens at home only: Armory → a gun → attachments / CAMO, or Armory → WEAPON LEVELS & CAMOS for
  every gun type. In a run you can't customise, but you can buy guns.
- **Wall buys** (`AZSWallBuy`): P9 in the start room ($250), M4 in the hallway and the warehouse ($1000), RD-704 in
  storage and the courtyard ($1500). Buying a gun you carry buys its ammo (half price). With two guns, the one in your
  hands is swapped out.
- **Mystery box** (`AZSMysteryBox`, warehouse, $950): spins, then offers a random gun, sometimes with up to three
  random attachments; take it within ~8 s. The blue light beam shows where it is.
- The HUD's weapon card shows the gun's level and XP bar; a level-up toasts what it unlocked.
- Only three guns exist so far (P9, M4, RD-704) — more gun packs make the walls and the box far more interesting.

## Combat fixes (this pass)

- Body shots now always register: the physics-driven hit reaction could pull the zombie's hit boxes off its body; the
  jolt is now purely visual (the mesh rocks about the hips on a spring).
- Round 1 is easier: walkers 70 hp (3 pistol body shots), runners 50; +16 % health per round after that.
- Slides: a tap of crouch while sprinting is a full slide even with hold-to-crouch; letting go only stands you up when
  the slide ends; jump or crouch again to cancel.
- Gore: severed heads and limbs are the zombie's real body parts flying off; zombies soak with blood as they're shot.
- Smoke columns, embers and ash (`AZSSmokeColumn`, the Rooftop's ash is an `AZSRain` set to slow grey flakes); burning
  towers on the Rooftop skyline.
- Test helpers: `-ZSAutoPlay` (soak test: stats every 10 s, ammo, doors), `-ZSAutoAim[Bone=]`, `-ZSAutoSlide[=hold]`,
  `zs.LogHits 1`, `ZSBuyWall <id>`, `ZSUseBox`.

### Stability notes (soak tests)

- Long automated runs (`-ZSAutoPlay`, 9-10 minutes, rounds 1-5) found and fixed: a crash when the gore cap recycled a
  severed body part's carrier (the part was left attached to a destroyed component), zombie spawns failing when a
  spawn point overlapped geometry (they now nudge free), and a first-frame GPU hang in Nanite shadow culling.
- `r.Nanite.Tessellation=0` (Config/DefaultEngine.ini): the hang's last pass was Nanite tessellation, which nothing of
  ours uses. Keep local lights shadowless in the big maps for the same reason.

## Hits, wounds and the camp (this pass)

- **Every body shot counts.** Bullets test a set of capsules built from the zombie's skeleton (torso, shoulders,
  arms, legs, head) as well as its physics asset, which has gaps (no shoulder bodies, a thin torso). Things that used
  to swallow shots without a hit no longer block bullets: pickup trigger spheres (75 cm, dropped where zombies die),
  flying gibs, flung door boards, spit. Corpses take a splash of blood but don't cost the bullet its penetration or
  damage. `zs.LogHits 1` logs every hit, including the ones only the skeleton test caught ("Skeleton hit ...").
- **Wounds.** Blood appears where each bullet went in and where it came out, running down from the wound; up to 8 wounds
  per zombie, nearby hits open the same wound wider. (This replaces the full-body soak that looked the same on every
  zombie.) `M_ZS_BloodOverlay` also carries the special zombies' rim glow. Rebuild only it with
  `create_gore_fx.py overlay`.
- **Headshots:** a killing shot to the head always takes the head off.
- **Muzzle flash:** our own short white-hot spark star. The pack's Niagara flame rendered as a big pink cloud in
  daylight; `zs.MuzzleFX 1` brings the pack's flame back, `2` its smoke too.
- **The camp** (`create_rooftop_map.py`, Hong Kong city pack props): stations are real furniture (gunsmith's bench with
  the rifle, trader's counter of produce and rounds, a clothes-rack wardrobe, ops desk and chalkboard, planning table);
  a dining corner under paper lanterns, a produce stall and cart, a kitchen table by the fire, a scavenge store of
  pallets and boxes, a washing line, a bike, planters, lamps under the tarps. Nadia (cook) and Eli (scavenger) joined.
- Test helpers: `-ZSAutoBurstsAt=6` (hold fire until 6 s), `-ZSAutoZombieDist=450` (spawn test zombies that close).
- **Severed limbs:** a hidden (cut) bone drops out of the animated pose and keeps a stale transform, frozen where it was
  cut. Never read one directly: `UZSGoreSubsystem::JointTransform` rebuilds the joint from the nearest visible
  ancestor. Hit capsules that end at a cut bone (a thigh after the calf is gone, an upper arm after the forearm) and
  stump blood both use it. Crawler legs are hidden with their physics bodies terminated (invisible calves used to stop
  bullets).
- **Station signs** turn to face the player (a fixed sign was edge-on from half the camp).
- **Playtest, 2026-11-28:** four 15-minute AutoPlay soaks on New York (aimed at chest, pelvis with the P9, shoulder,
  thigh), up to round 10 with 270 kills: no crashes, 90-200 fps. Plus 10 load loops on New York and the Rooftop. The
  one stall was the test bot aiming at a cut-off leg's stale bone (`-ZSAutoAim` now aims at the nearest visible
  parent). The Rooftop's rare first-frame Nanite shadow hang came back once in 23 loads after the camp dressing.

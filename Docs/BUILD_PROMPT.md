# Build Prompt: "Zombieshooter" — Unreal Engine 5.8 Single-Player Wave Survival FPS

> Paste everything below the line into a new Claude Code session opened on a clone of
> https://github.com/aipeytonparrish/Zombieshooter.git
> Items marked **(confirmed by owner)** were inferred from the owner's answers and confirmed by the owner on 2026-11-26.

---

## Role and goal

You are a senior Unreal Engine 5 gameplay programmer and you do **all** of the engineering work. Build an
**original**, single-player, first-person, round-based zombie survival shooter for **PC, released on
Steam**. The genre references are classic "survive the waves" zombie modes, fast hero-shooter movement and
gory co-op horde shooters — but every name, character, map, piece of art, sound and UI we ship is our own.

Long-term progression, high-stakes runs: you bring your own gear into a match, and **if you die you lose
the gear you brought and everything you earned in that run.**

Work in small, testable milestones. After each milestone: make sure the project compiles, commit with a
clear message, update `Docs/EditorSetup.md`, and tell the owner exactly what to click in the editor to test it.

## Technical ground rules

- **Engine:** Unreal Engine **5.8**, **C++ project**. Claude writes the code; the owner only does
  editor steps that cannot be done from text.
- **Platform:** Windows PC, Steam. **Controller is the primary input** — design and test every screen and
  every mechanic for gamepad first, then keyboard + mouse. Use **Enhanced Input** and **CommonUI** (gamepad
  focus/navigation, input-device-aware button prompts that switch between controller and keyboard glyphs).
- **Aim assist** for controller (slowdown + light magnetism near zombie hitboxes, tunable, toggle in settings).
- **Steam:** integrate the **Steamworks/Online Subsystem Steam** plugin from the start (dev App ID 480 until
  the owner provides one). Later milestones: achievements, Steam Cloud saves, Steam Deck-friendly UI scaling.
- **Hardware target:** owner has an RTX 4090. Target **120+ FPS at 1440p Epic** with a full horde on screen,
  and a sane path down to mid-range PCs via scalability presets. Support DLSS (incl. frame generation) if the
  owner provides the NVIDIA plugin; TSR otherwise.
- **Core logic lives in C++** (characters, movement, weapons, AI, rounds, economy, save, UI logic).
  Blueprint children only hold asset references and tuning values.
- **You cannot author binary assets** (.uasset/.umap) from text. For anything that must be created in the editor
  (Blueprint children, widget layouts, maps, materials, anim blueprints), write precise step-by-step
  instructions in `Docs/EditorSetup.md`. Prefer things you *can* write: C++, `.ini` config, and
  **CSV/JSON that imports into DataTables**.
- **Assets:** the owner provides **all** art, animation and audio. Use UE mannequins/primitives as
  placeholders until then. Keep `Docs/AssetRequests.md` — a running list of every asset each system needs
  (meshes, skeleton requirements, animations, sockets, sounds, VFX), with naming conventions and the folder
  to drop them in, so the owner knows exactly what to supply.
- **Data-driven everything:** weapons, attachments, character parts, market rotation pool, missions, tasks,
  zombie types, round scaling, XP curve, prestige rewards, map zones/unlocks — all in DataTables/CurveTables/DataAssets.
- Use **Lumen + Nanite + Virtual Shadow Maps**, **Niagara**, **MetaSounds**, **AI Perception + StateTree
  (or Behavior Trees)**, **USaveGame**.
- `.gitignore` for UE (Binaries, Intermediate, Saved, DerivedDataCache) and **Git LFS** for `.uasset`,
  `.umap`, textures, audio, FBX.

## The core loop

```
Bunker (home base, walkable) → customize character / build loadout / buy in daily market
   → Deploy: pick an unlocked level → survive rounds, kill zombies, complete missions & tasks
   → Round 20 boss → kill it = WIN (keep everything)  |  die = LOSE gear brought + all run earnings
   → Results screen → back to the bunker → spend, rebuild, prestige → deploy again
```

No buying inside a match — **no wall-buy weapons, no mystery box**. You fight with the loadout you brought,
plus pickups found on the map.

## Systems to build

### 1. Home base — the bunker (walkable)
- The main menu **is** a fully walkable underground **bunker** in first person (same movement as in game,
  but calmer: no sprint-slide needed indoors is fine to allow).
- Interactable stations (look at + press interact): **Mirror/Locker** (character creator), **Weapon Bench**
  (loadouts + attachments), **Market Terminal** (daily rotating shop), **Mission Board** (dailies, weeklies,
  story chain), **Map Table / Exit Door** (deploy + level select), **Prestige station**, **Stats/Records wall**.
- A pause-style menu is always one button away for Settings and Quit (controller Start button).
- Top-corner display: player level, prestige, XP bar, cash.

### 2. Modular character creator
- Choose **body type and gender**, then swap **every** part: head/face, hair, facial hair, torso, legs,
  gloves, footwear, headgear, eyewear/mask, backpack, accessories. Color/tint per part via material params.
- Built on a shared skeleton with **Leader Pose / Mutable (Customizable Object)** — pick one and document why.
- Part rows in a DataTable: ID, slot, body-type compatibility, mesh, icon, price, required level, prestige lock.
- **Camera in game:** default first person showing **arms and gloves that match the outfit**. Also build an
  experimental **tight over-the-shoulder third-person camera** toggle (settings + debug key) so the owner can
  compare both — keep aiming, recoil and hit detection correct in both modes.

### 3. Loadout
- Slots: **Primary, Secondary, Lethal (grenade), Tactical, Melee**.
- **5 saved loadout presets**; choose one at deploy.
- Each weapon supports **up to 5 attachments** chosen from its compatible slot list
  (e.g. Optic, Muzzle, Barrel, Underbarrel, Magazine, Stock, Laser — weapon-specific). Attachments modify
  stats (recoil, ADS speed, mag size, range, damage, handling, sprint-to-fire) via DataTable.
- **Attachments are bought separately for each gun** (owning a red dot on the SMG does not give it to the rifle).
- 3D weapon preview with live stat bars and stat deltas while browsing.

### 4. Market — rotating daily
- Tabs: **Weapons**, **Attachments** (per weapon), **Character parts/outfits**, plus a **featured** row.
- **Daily rotation** chosen from a pool DataTable; resets at a fixed UTC time. Use a seeded rotation so it's
  deterministic per day (don't rely on the system clock alone — guard against clock tampering by at least
  saving the last seen date; full online validation can come later via Steam time).
- Items require **both cash and a minimum player level / prestige**.
- Some always-available basics (starter weapons, ammo-free) so a player who lost everything can always
  rebuild **(confirmed by owner)**.

### 5. Risk and reward — what you lose
- **Run win (boss killed):** keep loadout, keep all XP, cash and rewards earned in the run, plus a win bonus.
- **Run fail (death / quit mid-run):** the **weapons, attachments and consumables in the deployed loadout are
  lost** from the inventory, and **all XP and cash earned in that run are lost**. Mission/challenge progress
  made in a failed run is also lost **(confirmed by owner)**. Character outfits are cosmetic and are never lost **(confirmed by owner)**.
- Show the stakes clearly at deploy ("At risk: …") and on the results screen.
- Guard against exploits: save the "in-run" state at deploy so force-quitting the game counts as a failure.

### 6. Progression — long, with prestige
- XP from zombie kills (bonus for headshots, specials, multi-kills), missions, tasks, rounds survived, boss kill.
- **Cash from kills too**, plus missions, tasks and the win bonus.
- Long XP curve (CurveTable). Levels unlock market items and **new levels/maps**.
- **Prestige:** when max level is reached, the player can prestige for a new prestige rank, exclusive
  cosmetics/emblems and a permanent perk or bonus. **Prestige does not reset inventory, cash or unlocks**
  — it is an additive long-tail track, not a wipe. Many prestige ranks.
- Results screen after every run: rounds, kills, headshots, tasks done, XP/cash (kept or lost), level-ups, unlocks.

### 7. Missions and tasks — all types
- **Daily challenges** and **weekly challenges** (rotating, from a pool).
- **Story mission chain** — a sequence of objectives that unlocks over time and gives the world some lore.
- **In-run tasks** that pop up during a match (e.g. "Kill 15 in 20 seconds", "Defend the radio for 60 s",
  "Reach the subway entrance before round 8"), with time limits and cash/XP rewards.
- One data-driven objective framework for all of them: objective type, target, optional time limit,
  conditions (weapon class, zone, round), rewards. HUD tracker for active ones.

### 8. Levels
- First level: **a small part of a city** at night — a few blocks with streets, an alley, a storefront
  interior, a parking garage or subway entrance, rooftops reachable by mantling. Build as graybox first.
- **Levels unlock as the player levels up** — the map table in the bunker shows locked/unlocked levels with
  their required level. Build the system for multiple levels now even though only one exists.
- **Run length target: 40–75 minutes** to clear round 20. Tune round scaling, zombie counts and break times to hit this.

### 9. Round system and boss
- Round Manager (C++ GameMode component): count, health, speed mix, special mix and spawn rate scale by round
  from CurveTables. Short break between rounds with a clear audio stinger.
- Spawns only out of view, live-zombie cap tuned for performance (start at ~40 on a 4090 and profile).
- **Round 20 boss** — a giant mutant (confirmed by owner) with **multiple phases**, telegraphed
  attacks, weak points, a health bar, and summoned adds. **Killing it wins the level.**
- Pause menu: resume, settings, **abandon run (counts as a fail, with a warning)**.

### 10. Ammo crates and pickups
- **Ammo crates are unlimited** — interact to refill reserve ammo for all weapons, no cooldown. Place them so
  the player still has to move through the map.
- **Pickups** dropped by zombies or placed on the map: health, armor plates, lethal/tactical refills, and
  timed **power-ups** (e.g. double damage, instant-kill, nuke-the-wave, double cash — our own names and visuals).
- No other in-match economy.

### 11. Player health and armor
- **Regenerating health** after a short delay without damage.
- **Armor** on top of health, refilled by armor pickups (and optionally armor bought for the loadout).
- Downed state is not needed (single player) — death ends the run.

### 12. Movement — all of it
- First person, **80° FOV default**, adjustable 70–110 in settings.
- Sprint, crouch, **slide** (momentum preserved, slide-hop), **mantle/climb**, jump with air control,
  **double jump**, **wall-run**, **dash**. All in a custom `UCharacterMovementComponent` subclass with
  tunable values and cooldowns, replicated-ready code style even though we're single player.
- Camera feel: head bob (toggle), slide tilt, wall-run tilt, landing dip, weapon sway/inertia, FOV kick on
  sprint/slide/dash. Controller: tune response curves and deadzones for fast movement.

### 13. Weapons and gunplay
- Hitscan bullets, projectile grenades/launchers. Per weapon: damage, headshot multiplier, fire rate, fire modes,
  mag/reserve, tactical vs empty reload, recoil pattern, spread, ADS time, range falloff, penetration.
- Hit markers, kill confirm sound, optional damage numbers, controller rumble/haptics on fire and hits.
- Starter catalog (placeholder data until the owner provides assets): pistol, SMG, assault rifle, shotgun,
  LMG, marksman rifle, frag grenade, a tactical (stun), melee knife.

### 14. Zombies — every type, maximum gore
- Types: **Walker, Runner, Crawler, Exploder, Spitter, Armored Tank/Brute, Screamer** (buffs/calls the horde),
  and the **Round 20 boss**.
- Horde AI: NavMesh pathing, surround/flank, vault low obstacles, pile through chokepoints, ragdoll on death.
- **Full gore:** location damage, **full dismemberment** (limbs, heads popping), gib meshes, blood spray Niagara,
  blood decals that build up over the run, blood on the player's screen and weapon up close, legless zombies
  become crawlers.
- **Gore setting:** Off / Reduced (blood, no dismemberment) / Full. Note for Steam: mature content survey.
- Performance: AI LOD, significance manager, pooling for zombies/decals/VFX, animation budget allocator.

### 15. Tone
- Tense, a little scary, not a horror game: dark streets, fog, flickering lights, distant groans, music that
  ramps with round intensity, distinct audio tells for each special. No jump-scare spam.

### 16. HUD and settings
- HUD: health, armor, ammo, round, active tasks, cash/XP popups, crosshair, hit markers, damage direction,
  active power-ups, boss bar. Must read well on a TV at couch distance (controller-first).
- Settings: FOV, camera mode (first/over-shoulder), sensitivity (separate controller + mouse, hip/ADS),
  response curve, deadzones, aim assist, invert Y, full rebinding for both devices, graphics presets,
  upscaler, frame cap, gore level, audio, head bob, subtitles.

### 17. Save system
- `USaveGame` profile: XP, level, prestige, cash, inventory (per-gun attachments), loadouts, character,
  mission/challenge progress, market rotation state, unlocked levels, stats, settings, and the **active run
  flag** for the force-quit rule. Auto-save after each run and each purchase. Steam Cloud later.

## Suggested C++ architecture

- `AZSPlayerCharacter`, `UZSMovementComponent` (slide/mantle/wall-run/dash/double jump),
  `UZSCameraModeComponent` (first person / over-shoulder), `UZSWeaponComponent`, `AZSWeaponBase`
- `UZSHealthComponent` (health + armor + regen; shared by player and zombies)
- `AZSZombieBase`, `AZSZombieAIController`, StateTree tasks, `UZSGoreComponent`, `AZSBossBase`
- `AZSGameMode_Survival`, `UZSRoundManagerComponent`, `AZSSpawnPoint`, `AZSAmmoCrate`, `AZSPickupBase`, `UZSPowerUpSubsystem`
- `UZSObjectiveSubsystem` (dailies, weeklies, story, in-run tasks)
- `UZSProfileSubsystem` (GameInstance subsystem: XP, prestige, cash, inventory, risk/loss, save/load)
- `UZSMarketSubsystem` (daily rotation), `UZSLevelUnlockSubsystem`
- `AZSBunkerGameMode`, `AZSInteractableStation`, CommonUI screens per station
- Row structs: `FZSWeaponRow`, `FZSAttachmentRow`, `FZSCharacterPartRow`, `FZSMarketPoolRow`,
  `FZSObjectiveRow`, `FZSZombieTypeRow`, `FZSLevelRow`, `FZSPrestigeRow`

## Milestones (in order)

1. **Project setup:** UE 5.8 C++ project, git/LFS, folders, Enhanced Input (gamepad first), CommonUI, Steam subsystem, `Docs/`.
2. **Movement:** full movement kit, FOV 80, camera feel, graybox test map, controller tuning.
3. **Gunplay:** weapon base, hitscan, ADS, reload, recoil, aim assist, 2 test weapons.
4. **Zombies v1:** walker + runner, chase/attack, health, ragdoll, basic blood.
5. **Round system:** rounds, spawns, scaling, unlimited ammo crates, health regen + armor, pickups, game over.
6. **Profile + economy + risk:** save game, XP/cash/level, win/lose rules, results screen.
7. **Bunker:** walkable home base, stations, deploy flow, level select with level-based unlocks.
8. **Loadouts + market:** 5 presets, per-gun attachments (max 5), daily rotating market.
9. **Character creator:** body type/gender, all part slots, tints, matching arms; over-shoulder camera experiment.
10. **Objectives:** dailies, weeklies, story chain, in-run tasks.
11. **Gore + all specials:** dismemberment, decals, crawler/exploder/spitter/tank/screamer, power-ups.
12. **Boss + win condition:** multi-phase round 20 boss, victory flow, tune run length to 40–75 min.
13. **Prestige + long progression tuning.**
14. **City level art pass (owner assets), audio, settings menu, scalability, profiling, Steam achievements/cloud.**

## Definition of done for v1

Using only a controller, a player can launch the game, walk around the bunker, create a character, buy a gun
and per-gun attachments from today's market, save one of 5 loadouts, deploy to the city level, survive 20
rounds (40–75 min) using unlimited ammo crates and pickups, complete tasks, kill the multi-phase boss, see the
results, return to the bunker with their gear and earnings, and find everything still there after restarting.
Dying instead correctly removes the deployed gear and run earnings.

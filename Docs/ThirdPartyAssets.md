# Third-Party Assets — Zombieshooter

Purchased packs supplied by the owner (2026-11-26). They are **not in git** (store licenses + size): every folder
below is listed in `.gitignore`. The original archives on the owner's Desktop are the backup:
`Desktop\Zombie Assets`, `Desktop\Military Assets`, `Desktop\City Assets`.

Our own code, maps, Blueprints and data **may reference** these assets. A fresh clone therefore needs the packs
restored (see the bottom of this file) before maps that use them will look right.

---

## 1. Unreal packs copied into `Content/` (original folder names kept, so internal references stay valid)

| Content folder | Pack | Made in | What's inside | Skeleton | Used for |
|---|---|---|---|---|---|
| `MS_NewYorkCityKitbash_Assets/` | NYC Kitbash (Megascans) | UE 5.3 | 15 modular building bases + ~210 props: street pieces, signs (31), lights (16), trash/cans, AC units, rooftop parts, trees, traffic lights, billboards, stairs, cranes, water tanks, hydrants, benches, bus stops, mailbox, guardrails. Demo map. | — | New York level: buildings 04/06/07/10/12–15 line the street and form the skyline |
| `RoadPack01/` | Road Pack | UE 5.5 | 14+ road / sidewalk / intersection meshes, electricity box. Demo map `RoadPack01Map`. | — | City streets |
| `ZombieCity01/` | Zombie City 01 | UE 5.0 | Zombie mesh `SK_ZombieCity01` + separated parts (body, hoodie, pants, shirt, t-shirt) | **UE4 Mannequin** (`SKEL_ZombieCity01`) | Walker / Runner bodies (M4), modular variety |
| `ZombieDoctor/` | Zombie Doctor | UE 5.0 | `SK_ZombieDoctor` + separated parts (body, robe, shirt, tie, pants, belt, shoes, accessories) | **UE4 Mannequin** (`SKEL_ZombieDoctor`) | Zombie variant (M4/M11) |
| `Zombie_Animation_Pack_Vol01/` | Zombie Anim Vol 01 (mocap) | UE 5.3 | ~20 idle / twitch / walk anims + demo mannequin | **UE5 Manny** (own copy) | Zombie locomotion (M4) |
| `Zombie_Anim/` | Zombie Anim Vol 02 (mocap) | UE 5.3 | ~120 anims: walks (all directions), runs, idles, turns, attacks | **UE5 Manny** (own copy) | Zombie locomotion + attacks (M4) |
| `Zombie_deaths_crawls/` | Death & Crawl pack | UE 5.3 | ~110 anims: deaths (incl. gunshot deaths), crawls, crawl attacks | **UE5 Manny** (own copy) | Deaths (M4), Crawler + legless zombies (M11) |
| `Zombie_attack_hit_reaction_anim/` | Ultimate Hit Reaction pack | UE 5.3 | ~115 anims: attacks, directional hit reacts, staggers | **UE5 Manny** (own copy) | Hit reacts / attacks (M4) |
| `AAA_FPS_Gloves/` | AAA FPS Gloves & Arms Vol 1 | UE 5.0 | `SK_Arms_01–05` + glove meshes, materials, `BP_Arms_A–E`, demo map `Maps/Main` | **UE5 Manny** | First-person arms (M3), glove outfits (M9) |
| `AAA_FPS_Gloves_Vol2/` (+ its `__ExternalActors__`/`__ExternalObjects__` folders) | AAA FPS Gloves & Arms Vol 2 | UE 5.0 | `SK_Arms_06–11` + gloves, demo maps | **UE5 Manny** | More arms / glove outfits |
| `SurvivorsMegaPack/` | Survivors Mega Pack | UE 4.26 | 5 survivor characters (`SK_Survivor1–5`, each with its own physics asset), demo maps | **UE4 Mannequin** (one skeleton per survivor) | Over-shoulder camera body / character creator reference (M9), possible NPCs |

## 2. FBX / source-only packs in `SourceArt/ThirdParty/` (import in the editor when the milestone needs them)

| Folder | Contents | Used for |
|---|---|---|
| `RD704_ModularAK/` | RD-704 AK, fully modular: receiver, handguard, stock (Magpul Zhukov), grip (Scorpius), mag (FAB Ultimag 30) + 7.62 rounds, Bravo4 scope (lens + reticle meshes), WAWE suppressor + muzzle brake, BCM vertical grip, PBR textures. No animations. | Assault rifle + attachments (M3 / M8) |
| `MachineGunAnims/` | First-person M249 LMG with arms: draw, idle, fire, fire auto, reload (FBX; its own arm rig, not Manny) | LMG reference / retarget (M3+) |
| `MuzzleFlash/` | Two muzzle-flash mesh packs (FBX) | Muzzle flash cards for the Niagara effect (M3) |
| `Bullets/` | Bullet / cartridge models (FBX) | Shell casings, ammo UI, pickups (M3/M5) |
| `AmmoBoxes/` | 14 FBX ammo boxes (9mm, 5.56, 7.62, .50, 12 ga, .300 Mag, 40 mm grenade; open + closed) + textures + the original UE 4.21 project | Unlimited ammo crates (M5) |
| `Masks/` | Mask / bandana collection (FBX) on a male base mesh | Character creator face slots (M9) |
| `FPSArms_CGTrader/` | Modular FPS arms (Blender + `UE4Export` FBX: arms, 7 glove sets, sleeves/shirts, watches, patches, camo) | Extra arm / glove variety (M9) |
| `ZombieAnims_FBX/` | FBX copies of the four zombie animation packs | Only if re-import is needed |

## 3. Not extracted (still only on the Desktop)

| Archive | Why |
|---|---|
| `mega-pack-male-hair-card.zip` (10.8 GB) | Hair cards for the character creator — extract the `unreal file.rar` entries in M9 |
| `mask-mania-pack…zip` (1.9 GB, Marvelous Designer/OBJ/FBX) | Character creator face slots — M9 |
| `Survivor1–5Textures.rar` (~3.3 GB) | High-res source textures for the Survivors pack; the UE project already has usable textures |
| Blender/Max/Maya/Unity/OBJ versions, UE4 projects | Duplicates of what's above |

---

## 4. Skeleton notes (matters for Milestone 4)

- **Zombie animations** (four packs, ~370 anims) are on the **UE5 Manny** skeleton, but each pack ships its **own
  copy** of that skeleton. They must be made compatible (Skeleton → *Compatible Skeletons*) or retargeted onto one
  skeleton so one Anim Blueprint can use all of them.
- **Zombie meshes** (`ZombieCity01`, `ZombieDoctor`) and the **Survivors** use the **UE4 Mannequin** skeleton
  (same bone names as UE5 minus `spine_04/05`, `neck_02`, different proportions). UE5-Manny animations need an
  **IK Retargeter** (UE5 Manny → UE4 Mannequin) to play on them. Planned approach for M4: one IK Rig per skeleton
  plus a retargeter, saved under `Content/ZS/Characters/Zombies/Retarget/` (our own, versioned assets).
- **FPS arms** (`SK_Arms_01–11`) are on the full UE5 Manny skeleton, so any Manny first-person animation drives
  them. **The packs contain no first-person weapon animations** — see the gaps list in `AssetRequests.md`.

## 4b. Generated assets (`Content/ZS_Imported/`, git-ignored)

Some packs are FBX-only, so scripts in `Tools/Import/` convert them into Unreal assets headlessly (no clicking):
`powershell -ExecutionPolicy Bypass -File ToolsImport
un_imports.ps1` (editor closed). Currently: the RD-704 rifle
(`import_rd704.py` - static-mesh parts, PBR materials on `M_ZS_PBR_Master`, glowing Bravo4 reticle). Code references
these paths and falls back to placeholders when they're missing.

## 5. Restoring the packs on a fresh clone / another PC

1. Get the Desktop archives (zips/rars listed above).
2. For each pack in section 1, open its zip, then the inner `Uproject…`/`UE5_…`/non-`_Fbx` archive, and copy the
   folder **inside that project's `Content/`** (e.g. `ZombieCity01/`) into `Zombieshooter/Content/` with the same name.
   For Gloves Vol 2 also copy `Content/__ExternalActors__/AAA_FPS_Gloves_Vol2` and
   `Content/__ExternalObjects__/AAA_FPS_Gloves_Vol2`.
3. For section 2, extract the FBX/texture archives into `SourceArt/ThirdParty/<Folder>/`.
4. Windows' own `tar.exe` (`C:\Windows\System32\tar.exe -xf file.rar -C dest`) opens both .zip and .rar files.
   Note: Git Bash's `tar` is a different program and can't.
5. Or ask Claude: "restore the third-party packs from the Desktop archives".

---

## 6. Fab packs added 2026-11-26 (installed straight into `Content/`, git-ignored)

Restore on another PC by adding them to the project again from Fab / the Epic launcher library.

| Folder | What | Skeleton | Needs plugin | Use |
|---|---|---|---|---|
| `InfimaGames/TacticalFPSAnimations` | AAA first-person assault-rifle set: fire (semi/auto/aimed/empty), tactical + empty reloads (+aimed, quick), equip/holster, ADS in/out, walk/run/sprint/jump, inspect, mag check, melee, grenade throw, heal; modular AR mesh, AR audio, muzzle-flash Niagara | `SKEL_TFA_Mannequin` (UE5 Manny bones) | ControlRig | **Rifle FP animations + sounds + muzzle flash (M3 upgrade)** |
| `ProFPSAnimationBundleV2` | 13 FP sets: rifle v1/v2, **pistol v1/v2** (idle, walk, sprint, fire, ADS, reload + empty reload, draw, holster), revolver, sawed-off, sniper, **knife**, blade, **grenade**, katana, FP zombie arms; pistol / rifle / grenade meshes; hands + gloves | Per-set `SK_Mannequin_Arms_Skeleton` (UE5 Manny bones) | GameplayCameras, NiagaraAnimNotifies | **Pistol, knife, grenade, shotgun FP animations (M3/M8)** |
| `ExcellentProceduralRecoils` | Procedural recoil/ADS/idle system, ~33 presets; skeletal example guns incl. **SMG**, pistol, AK, shotgun, sniper, LMG; muzzle flash | Own Manny copy; per-gun skeletons | GameplayCameras, MetasoundEditor | Recoil reference, SMG mesh |
| `RealWeaponSoundFX` | 1,313 sounds: per-gun single / loop / silenced shots, mag in/out, reload, equip (AR ×7, pistol ×6, SMG ×8, shotgun, MG, sniper, revolver…); bullet impacts incl. flesh | — | — | **Gun sounds + impact sounds** |
| `WeaponVerse1` | 18,350 sounds: ~60 named guns, shots at 5/25/100/200 m in field/room, silenced, bolts, mags, draws, drops, sonic booms | — | — | **Premium gunshots + distance tails** |
| `GunSoundsPackVol1` | 19 gunshots, dry fire, load/unload, pump, hammer, safety; 3 explosions | — | — | Dry fire / foley, explosions |
| `Mocap_Zombie` | 45 zombie anims (10 attacks, walk → sprint, grab/struggle pairs, hit reactions), UE4 + UE5 versions | UE5 Manny copy / UE4 Mannequin | — | **More zombie variety (M4/M11)** |
| `Hitreact_Content` | 118 directional hit reacts + stun + hit-react component | UE5 Manny (`/Game/Characters`) | **Chooser** | Zombie hit reacts (M11) |
| `InteractionAnimation` | Door kick/shoulder, cabinet, drawer, chest, generator, lever, valve + props | UE5 Manny | — | Interactions (M5/M7) |
| `RealisticFireFX` | ~50 fire / spark Niagara systems | — | — | Burning city (M14), Exploder / boss (M11/M12) |
| `CityCore_HongKong` | 5.4 GB city kit: buildings, roads, sidewalks, market, pipes, trash, poles; day/night maps | — | GeometryScripting (editor tools) | Street filler props (M14); signage is Asian, not NYC |
| `UltimateTraversals` | Vault / climb / ladder / window traversal anims (UE4 + UE5) | UE4 / UE5 Manny | ControlRig | Possible mantle polish later |
| `MC_Idles_Pack`, `MaleLocomotionSet`, `MovementAnimsetPro`, `RifleAnimsetPro`, `RifleMotionMatchingLoco` | Third-person civilian idles / locomotion / rifle motion matching | various | RifleMotionMatchingLoco needs PoseSearch, Chooser, MotionWarping, AnimationWarping, BlendStack | Mostly not needed for a first-person game (over-shoulder camera / NPCs later) |
| `ModularSciFi`, `FastParallaxOcclusionMapping` | Sci-fi gun parts; parallax material functions | — | — | Probably unused (maybe boss ray gun); material polish |
| `Characters`, `Input`, `LevelPrototyping`, `ThirdPerson` | UE5 third-person template content pulled in by the packs | UE5 Manny | — | Dependency of Hitreact etc. — leave in place |

Gaps that remain: **no blood Niagara effect** in any pack (Milestone 4 uses the code-driven droplets + the project's own
blood decal), **no UI hit-marker / kill-confirm sound**, and only one SMG mesh (no SMG FP animations).

Note: packs that need plugins we haven't enabled (Chooser, PoseSearch, GameplayCameras, …) will log warnings if their
demo assets are opened in the editor. Nothing in our code references them; plugins get enabled when a pack is put to use.

## Bunker environment (Milestone 7 art pass)
`Content/Bunker/` — from `Desktop/Enviorments Assets/Bunker.rar` (Fab "Bunker" pack, UE 4.25). Restore by extracting
`Bunker/Content/Bunker` from the archive into `Content/`. Used by `L_Bunker` (streams `/Game/Bunker/Maps/Demo`).

## New York level (owner's "post-apocalyptic abandoned city New York" scene)
Source: `post-apocalyptic-abandoned-city-new-york.zip` (Desktop / Downloads): a Blender render scene
(`Post-apocalyptic_NY.blend`, 1,350 objects, 9.3M polys) plus `textures.rar`. Pipeline (all scripted, re-runnable):
1. Extract the zip and the textures rar into `Desktop/Enviorments Assets/_extract/PostApocNY/`.
2. `blender -b --disable-autoexec Post-apocalyptic_NY.blend --python Tools/Import/blender_export_postapoc_ny.py -- <textures> <export dir>`:
   relinks textures, drops the sky dome / grass particles / camera, merges everything into ~70 meshes (per
   collection and 25 m cell) at full detail (no decimation: Nanite renders it), exports glTF.
3. `Tools/Import/import_postapoc_ny.py` imports it (Nanite, complex collision) into `Content/NYC_PostApoc/`
   (git-ignored) and places it in `/Game/NYC_PostApoc/Maps/L_NYC_Art`.
   It also removes the scene's "Buildings out of scene" placeholder shells and gives the materials whose textures
   are missing (windows, cables, tanks) flat colours (`--tune-only` / `--materials-only` re-run just those steps).
4. Safehouse kit: unpack `industrial-survival-horror-modular-kit.zip` and its `..._glTF.zip` into
   `Desktop/Enviorments Assets/_extract/IndustrialKit/`, then `Tools/Import/import_industrial_kit.py <glTF dir>`
   → `Content/IndustrialKit/` (git-ignored, complex collision).
5. `Tools/Import/prepare_kitbash_buildings.py`: Nanite + collision on the Megascans NYC kitbash buildings.
6. `Tools/Import/create_nyc_map.py` builds our `L_NYC` (weather, safehouse, kitbash backdrop buildings, gameplay
   actors) which streams the art level.
10 small Textures.com maps referenced by the .blend were not in the archive (a marble floor, a worn wall); those
surfaces import without them.

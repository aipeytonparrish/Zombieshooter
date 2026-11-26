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
| `MS_NewYorkCityKitbash_Assets/` | NYC Kitbash (Megascans) | UE 5.3 | 15 modular building bases + ~210 props: street pieces, signs (31), lights (16), trash/cans, AC units, rooftop parts, trees, traffic lights, billboards, stairs, cranes, water tanks, hydrants, benches, bus stops, mailbox, guardrails. Demo map. | — | City level (graybox dressing now, art pass M14) |
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

# Asset Requests — Zombieshooter

Running list of every art / animation / audio asset the game needs, what each must look like technically, and
where to put it. The code works with placeholders until these arrive. Updated every milestone.

**Status key:** ⬜ needed · 🟨 placeholder in use · ✅ delivered

---

## Conventions

### Folders (under `Content/ZS/`)
| Folder | Contents |
|---|---|
| `Characters/Player/` | Player body parts, first-person arms, player animations |
| `Characters/Zombies/<Type>/` | One folder per zombie type (Walker, Runner, …, Boss) |
| `Weapons/<WeaponName>/` | Mesh, materials, anims, sounds for one weapon |
| `Weapons/Attachments/` | Attachment meshes |
| `Bunker/` | Home-base environment |
| `City/` | First level environment |
| `UI/` | Icons, fonts, button glyphs, HUD art |
| `Audio/Music/`, `Audio/SFX/`, `Audio/VO/` | Sound files (imported as Sound Waves; MetaSounds built from them) |
| `VFX/` | Niagara systems and their textures |
| `Materials/` | Shared master materials |

### Naming (Unreal standard prefixes)
`SK_` skeletal mesh · `SM_` static mesh · `SKEL_` skeleton · `A_` anim sequence · `AM_` montage · `BS_` blend space ·
`ABP_` anim blueprint · `T_` texture (`_D` base color, `_N` normal, `_ORM` occlusion/roughness/metal, `_M` mask) ·
`M_` material · `MI_` material instance · `S_` sound wave · `MS_` MetaSound · `NS_` Niagara system · `PHYS_` physics asset.

Example: `Content/ZS/Weapons/SMG_Vandal/SK_SMG_Vandal.uasset`, `A_SMG_Vandal_Reload_Tactical`.

### Technical rules
- **Scale:** 1 unit = 1 cm. Characters ~180 cm tall, feet at origin, facing **+X** (Unreal forward).
- **Export:** FBX (or glTF), Y-up or Z-up is fine — tick *Convert Scene* on import.
- **Skeletons:** player and humanoid zombies share the **UE5 Manny/Quinn skeleton** (`SK_Mannequin`) so retargeted
  animation and modular parts work. Non-humanoid boss can have its own skeleton.
- **Textures:** power-of-two, 2K default (4K for hero items), packed ORM.
- **Audio:** WAV, 48 kHz, 16- or 24-bit. Mono for 3D/positional sounds, stereo for music/UI.
- **Drop raw source files** (FBX/WAV/PSD) anywhere outside `Content/` if you want them versioned — LFS tracks them.

---

## Delivered so far (2026-11-26) — see `Docs/ThirdPartyAssets.md`

Packs received: NYC city kitbash + road pack, 2 zombie characters (City01, Doctor), 4 zombie animation packs
(~370 anims: locomotion, attacks, hit reacts, deaths, crawls), 11 FPS arms + glove sets (2 packs) + modular arms
source, modular RD-704 AK with attachments, first-person M249 animations, muzzle-flash meshes, bullets, 7 ammo-box
types, 5 survivor characters, masks/bandanas, hair cards.

### Biggest gaps right now (in priority order)
1. **First-person weapon animations on the UE5 Manny skeleton** (idle, walk, sprint, fire, ADS, tactical reload,
   empty reload, equip/unequip) for a **pistol**, **SMG** and **assault rifle**. The arm packs have meshes only.
   Without these, Milestone 3 uses procedural motion (code-driven recoil/sway/reload dip) — that works, but real
   reload animations look much better. A Manny-skeleton FPS animation pack (any store) covers this.
2. **Pistol and SMG meshes** (spec's 2 test weapons). The AK is here; the M3 test weapons will be **AK + pistol**
   unless an SMG arrives. Placeholder cube-guns are used until then.
3. **Weapon sounds** (fire close/distant/tail, dry fire, mag out/in, bolt, equip) — none yet.
4. **Zombie audio** (groans, attacks, deaths) and **blood VFX / decals** — needed for M4.
5. More zombie bodies (the spec's 7 types + boss) — 2 are here; Runner/Walker can share them with variety in M4.

---

## Milestone 2 — Player movement

| Status | Asset | Spec | Where |
|---|---|---|---|
| ✅ | **First-person arms** — `AAA_FPS_Gloves/Modules/SK_Arms_01–05`, Vol2 `SK_Arms_06–11` (UE5 Manny) | Arms + hands on the Mannequin skeleton (or the UE5 first-person arms rig). Will be tinted/swapped to match the glove/sleeve outfit parts in Milestone 9, so split sleeves and gloves into separate material slots. | `Characters/Player/` |
| ⬜ | FP arms anims: idle, walk, sprint, slide, wall-run (L/R), mantle, dash, jump start/loop/land | Loopable where relevant; root-motion **off**. Can wait until weapons (M3) since arms will mostly hold a gun. | `Characters/Player/Animations/` |
| ⬜ | **Footstep sounds** | 6–8 variations each for concrete, metal, wood, dirt, water puddle; separate sprint set (heavier). | `Audio/SFX/Player/Footsteps/` |
| ⬜ | Movement sounds | Jump, double-jump whoosh, land (soft/hard), slide start + slide loop + slide end, wall-run loop (per surface optional), wall-jump push, mantle grunt + hand grab, dash whoosh | `Audio/SFX/Player/Movement/` |
| ⬜ | Breathing | Heavy breathing loop after long sprints (optional) | `Audio/SFX/Player/` |

Movement VFX (optional, later): dash streak, slide dust/sparks — `VFX/Player/`.

---

## Coming next (so you can plan ahead)

### Milestone 3 — Gunplay (built with the RD-704 rifle + a placeholder pistol)
Still needed, in priority order:
- **FPS animations on the UE5 Manny skeleton** for rifle + pistol: idle, walk, sprint, fire, ADS idle/fire, tactical
  reload, empty reload, equip, unequip. Until then arms are code-posed (works, but real reloads look far better).
- **Pistol mesh** (and an SMG if you want the spec's pistol + SMG pair): skeletal mesh with sockets `Muzzle`, `Eject`,
  `Mag`, `Optic`, or modular static parts like the RD-704.
- **Weapon sounds** per gun: fire close / distant / tail, dry fire, mag out / in, bolt, equip. Plus a hit-marker tick and a
  kill-confirm sound.
- **Muzzle flash**: the supplied flash meshes will be turned into a Niagara effect; a flash texture sheet would help.
- Shell casing: the supplied bullets pack covers it (imported later).

Original M3 list:
Per weapon: skeletal mesh with sockets **`Muzzle`**, **`Eject`**, **`Mag`**, optic rail socket **`Optic`**; FP arm anims
(idle, fire, ADS idle, ADS fire, tactical reload, empty reload, equip, unequip, sprint); sounds (fire close/distant,
tail, dry fire, mag out/in, bolt, equip); muzzle flash Niagara; shell casing mesh; crosshair + hit-marker textures.

### Milestone 4 — Zombies v1 (Walker, Runner)
Mannequin-skeleton zombie meshes with a physics asset; anims: idle, shamble walk, run, attack (2–3), hit reacts,
death (2–3, ragdoll takes over after); groans/attack/death sounds; blood impact Niagara + blood decal textures.

### Later milestones
Bunker environment (M7), city block kit (M14), character creator parts on the Mannequin skeleton (M9), all special
zombies + gib meshes (M11), boss (M12), music + UI art (M14). Detailed specs get added here as each milestone starts.

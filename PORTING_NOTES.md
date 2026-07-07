# VehFuncs AML Port — Per-Feature Porting Notes

This document is the engineering guide for actually porting each feature from
PC VehFuncs to Android AML (arm64-v8a). It is the work that remains after the
scaffold in this package.

---

## Triage summary

| Difficulty | Features | Why |
|---|---|---|
| **Easy** (self-contained, no asm) | DigitalSpeedo, DigitalOdometer, PopupLights, Trifork | Pure logic + node-name lookup. No x86 asm, no reg_pack. |
| **Medium** (node-name + few hooks) | Spoiler, Pedal, Steer, Shake, Footpegs, Wheel, GearAndFan, AtomicsVisibility, HandsFix, CustomVisibilityRender, FixMaterials, MatrixBackup | Each needs CVehicle struct layout and a handful of arm64 hooks. No asm. |
| **Hard** (asm or many hooks) | DamageableRearWings (x86 inline asm), CheckRepair (8 reg_pack hooks), Patches (20+ inline hooks), RecursiveExtras (complex traversal), Anims | Each requires per-hook rewrite of register access + control flow. |
| **Blocked** (depends on companion mods) | ApplyGSX (needs GSX), IndieVehHandlingsAPI (needs IVHF), Soundize-related (no Android build) | Cannot port until the dependency is itself ported. |
| **Pure logic** (already portable) | IniReader, SimplexNoise | Done. |

---

## What "porting a feature" actually means

For each feature module:

1. **Open the PC source** (`/research/VehFuncs/VehFuncs/<Feature>.cpp`).
2. **List every PC address** it references (see `ADDRESS_MAP.md` for the master list).
3. **For each address**, find the arm64 equivalent:
   - Open `libGTASA.so` (arm64, v2.10) in IDA Pro / Ghidra.
   - Locate the same function by:
     - (a) **String references** — if the PC function has a unique string nearby, search for that string in arm64.
     - (b) **Symbol name** — if `libGTASA.so` is not stripped, search the Itanium-mangled name (use `c++filt` to decode). Most C++ methods ARE present in the Android build.
     - (c) **Pattern scan** — bytes-equivalent of the PC disassembly, with wildcards for relative offsets. AML's `aml->PatternScan("AA BB ? CC", "libGTASA.so")` is the most version-robust approach.
   - Record the arm64 offset.
4. **Rewrite each hook** using AML's `DECL_HOOK*` + `HOOK*` macros:
   ```cpp
   DECL_HOOKv(SomeFunc, void* self) {
       SomeFunc(self);  // call original
       // your code
   }
   ON_MOD_LOAD() {
       HOOK(SomeFunc, pGame + 0xARM64_OFFSET);
   }
   ```
5. **Replace `reg_pack`-style hooks** (which use x86 registers like `regs.edi`, `regs.esi`) with proper function hooks. On ARM64 the calling convention passes args in X0–X7 — if the PC hook grabbed `edi` as `this`, on arm64 that's `X0`. You don't need `reg_pack` on arm64; use a typed `DECL_HOOK*` with the correct signature.
6. **Replace x86 inline assembly** (`movzx eax, [esp+0x78]`, `fld [esp+0x74]`) — this only exists in `DamageableRearWings.cpp`. The asm is reading args from the stack at specific offsets. On arm64 those same args are in registers (X0–X7) — the asm block becomes a normal typed hook.
7. **Replace plugin-sdk calls** (`plugin::CallAndReturn<RwTexture*, 0x7F3AC0, ...>`) with `aml->GetSym`-resolved function pointers:
   ```cpp
   // PC:    plugin::CallAndReturn<RwTexture*, 0x7F3AC0, const char*, const char*>("txgrass1_3", 0);
   // arm64:
   typedef RwTexture* (*RwTextureReadFn)(const char*, const char*);
   static RwTextureReadFn RwTextureRead = nullptr;
   // In OnModLoad:
   RwTextureRead = (RwTextureReadFn)aml->GetSym(hGame, "_Z13RwTextureReadPKcS0_");
   // Use:
   RwTexture* text = RwTextureRead("txgrass1_3", nullptr);
   ```
8. **Replace C++ class member access** with offset-based access until plugin-sdk-equivalent headers for arm64 are produced. Example:
   ```cpp
   // PC: vehicle->m_nVehicleSubType
   // arm64 (until headers exist):
   uint32_t subType = *(uint32_t*)((uintptr_t)vehicle + 0x...);  // find offset in arm64 CVehicle
   ```
9. **Test on device** with `adb logcat -s AML VehFuncs` open.

---

## Feature-by-feature breakdown

### Easy tier

#### DigitalSpeedo
- **PC source**: `DigitalSpeedo.cpp` (~few hundred lines)
- **What it does**: Renders a digital speedometer texture on the dashboard of vehicles with the `f_speedo` node.
- **PC hooks**: ~3-5 addresses, all in the render pipeline.
- **Porting plan**: Find `CVisibilityPlugins::RenderVehicleAtomic` arm64 equivalent by symbol. Hook it, check for the node name, render the speedo texture via RwIm2D / RwRaster calls.
- **No inline asm.** Direct port.

#### PopupLights
- **PC source**: `PopupLights.cpp`
- **What it does**: Animates popup headlights based on `f_popup_l` node frames.
- **PC hooks**: 1-2 addresses in CVehicle update.
- **Porting plan**: Hook the vehicle update function. Per-frame: find `f_popup_l` child frame, rotate based on speed/time.
- **No inline asm.** Direct port.

#### Trifork
- **PC source**: `Trifork.cpp` (only 4 lines in the header — small)
- **What it does**: 3-fork vehicle frame logic.
- **Porting plan**: Very small. Hook vehicle init, duplicate frame hierarchy.

### Medium tier (one example in detail, others follow same pattern)

#### Spoiler
- **PC source**: `Spoiler.cpp`
- **What it does**: Animated spoiler (e.g. adjusts angle based on speed) for vehicles with `f_spoiler` node.
- **PC hooks**: ~3-5 addresses.
- **Porting plan**:
  1. Hook CVehicle::Update or per-frame render.
  2. For each vehicle, look up `f_spoiler` child frame by name (using RwFrameForAllChildren + a name-compare callback).
  3. If found, apply RwFrameRotate based on speed.
- **CVehicle struct access needed**: `m_vecMoveSpeed` (find arm64 offset), `m_fGasPedal` (find arm64 offset).

### Hard tier

#### DamageableRearWings
- **PC source**: `DamageableRearWings.cpp`
- **What it does**: Allows rear wings on vehicles to be damaged independently.
- **BLOCKER — x86 inline asm**:
  ```asm
  movzx   eax, [esp + 0x78]   ; load a 4th argument from stack
  fld     [esp + 0x74]        ; load a float argument from stack
  push    ecx
  push    eax                 ; component
  push    esi                 ; vehicle
  ```
  This is hand-written x86 assembly patching the function at PC `0x6A7BAE` to add extra args before calling DamageableRearWingsHook.
- **ARM64 port strategy**:
  1. Don't use inline asm at all. ARM64 hooks via AML's `Hook` give you typed function pointers.
  2. Find the arm64 equivalent of `0x6A7BAE` (a CAutomobile method, likely `DamageComponent` or similar).
  3. Write it as a typed hook: `DECL_HOOK(void, DamageComponentHook, void* self, int component)`.
  4. The args that the asm was reading from `[esp+0x78]` and `[esp+0x74]` are normal function args on arm64 — they arrive in X3/X4 or similar. The IDA disassembly of the arm64 function will show you exactly which register holds which arg.
- **NOP patches at `0x6A7ED8` and `0x6A7EE1`**: replace with `aml->PlaceNOP(pGame + 0xARM64_OFFSET, count)`.

#### CheckRepair
- **PC source**: `CheckRepair.cpp`
- **What it does**: Hooks 8 different repair/damage functions to detect when a vehicle is repaired, then re-applies VehFuncs extras.
- **PC hooks**: 8 `MakeInlineAutoCallOriginal` at: `0x6A04E3, 0x6A3466, 0x6C4539, 0x6CABBC, 0x6CE2BC, 0x6D2721` (and 2 commented out).
- **reg_pack usage**: every hook accesses `regs.edi`, `regs.esi`, or `regs.ecx` to grab the `this` pointer (the vehicle).
- **ARM64 port strategy**:
  1. Each of these 6 functions is a CVehicle (or CAutomobile) method. On arm64, `this` is in X0.
  2. Replace each `MakeInlineAutoCallOriginal<0xADDR>([](reg_pack &regs) { ... })` with:
     ```cpp
     DECL_HOOK(void, SomeRepairFunc, void* self) {
         SomeRepairFunc(self);  // call original
         CVehicle* veh = (CVehicle*)self;
         // re-apply extras
     }
     ```
  3. Find arm64 equivalent of each PC address.

#### Patches
- **PC source**: `Patches.cpp`
- **PC hooks**: 20+ addresses — every `MakeInline<0xADDR, 0xADDR+5>`, `MakeCALL(0xADDR, Fn)`, `MakeNOP(0xADDR, n)`, `WriteMemory(0xADDR, val)`.
- **Porting plan**: Go through `ADDRESS_MAP.md`, for each `Patches.cpp` entry, find arm64 equivalent and use `aml->PatchMemory`, `aml->PlaceNOP`, `aml->Hook`.

### Blocked tier

#### ApplyGSX
- **PC source**: `ApplyGSX.cpp` + `GSXAPI.cpp`
- **Blocker**: Mod depends on the GSX (GTA SA Extended) mod for its API. GSX is PC-only.
- **Unblock options**:
  - (a) Port GSX itself first (huge effort).
  - (b) Drop ApplyGSX from the Android VehFuncs port. Other features still work.
  - (c) Write a minimal GSX-compatible API shim that provides just what VehFuncs needs.

#### IndieVehHandlingsAPI
- **PC source**: `IndieVehHandlingsAPI.cpp` + `IndieVehHandlings/ExtendedHandling.h`
- **Blocker**: Same situation — depends on the IndieVehHandlings mod.
- **Unblock**: port IVHF first, OR drop extended handling support and use stock CHandlingMgr.

#### Soundize (audio backfire / turbo)
- **PC source**: referenced via `Soundize/Soundize.h` + `Soundize (Junior_Djjr).lib` (Windows static lib)
- **Blocker**: Soundize is a Windows-only static lib. There is no Android build.
- **Unblock options**:
  - (a) Reimplement the backfire/turbo audio features against Android OpenSL ES or AML's BASS plugin.
  - (b) Drop those features. The visual parts of VehFuncs don't need audio.

---

## The actual hook-port workflow

For each PC address in `ADDRESS_MAP.md`, the workflow is:

1. **Open `libGTASA.so`** (arm64 v2.10) in IDA Pro / Ghidra.
2. **Identify the function** at the PC address in the PC `gta_sa.exe` (1.0 US):
   - Open PC `gta_sa.exe` in IDA.
   - Note the function name (most are named in plugin-sdk or GTAModding wiki).
3. **Find the same function in arm64**:
   - Search by symbol: `Functions window` → filter by name (e.g. `CPools::Init` → `_ZN6CPools4InitEv`).
   - Or search by string reference: pick a unique string the function uses, find it in arm64 `Strings` window, look at xrefs.
   - Or pattern scan: copy 8-16 bytes of distinctive instructions from the PC disassembly, find similar in arm64 (account for instruction differences).
4. **Record the offset**: arm64_offset = function_start_va - libGTASA.so_base_va.
5. **Write the AML hook**:
   ```cpp
   HOOK(FunctionName, pGame + 0xARM64_OFFSET);
   ```
6. **Build, deploy, test**:
   ```bash
   /path/to/ndk/ndk-build
   adb push libs/arm64-v8a/libVehFuncs.so /sdcard/Android/data/com.rockstargames.gtasa/mods/
   adb logcat -c && adb logcat -s AML VehFuncs
   ```
7. **Iterate**: if the hook crashes, the offset is wrong. Re-verify in IDA.

---

## Realistic time estimate

| Tier | Per-feature time | Total for tier |
|---|---|---|
| Easy (3 features) | 2-4 hours each | ~10 hours |
| Medium (12 features) | 4-8 hours each | ~80 hours |
| Hard (5 features) | 1-3 days each | ~10 days |
| Blocked (3 features) | blocked | blocked |

**Total realistic effort for a working VehFuncs Android port: ~3-4 weeks of focused work** for someone with:
- IDA Pro / Ghidra access
- The arm64 `libGTASA.so` binary
- An Android device with the unprotected v2.10 64-bit APK + AML + FLA installed
- Working `adb logcat` setup
- Familiarity with ARM64 assembly and the RenderWare API

This scaffold removes the "where do I even start" friction — once the arm64 offset work begins, every feature has a clear place to live and a clear pattern to follow.

/*
 * amlmod.h — MINIMAL PLACEHOLDER for the AML mod header.
 *
 * This is NOT the real AML header. It exists only so this project compiles
 * standalone for inspection. To build against the real AML, clone
 *   https://github.com/RusJJ/AndroidModLoader
 * and copy include/mod/* over this file and its siblings.
 *
 * The real AML provides the full IAML interface (~200 methods), hook macros,
 * config helpers, JNI bridge, MLS storage, etc. See:
 *   https://github.com/RusJJ/AndroidModLoader/blob/main/docs/DOCUMENTATION_EN.MD
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "iaml.h"     // forward
#include "interface.h"
#include "logger.h"
#include "config.h"

#define IAML_VER 0x01040000  // AML 1.4.0

// ABI selection
#if defined(__arm__)
    #define AML32
#elif defined(__aarch64__)
    #define AML64
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    // Host syntax-check build (not a real target). Pretend to be AML64 so
    // BYBIT picks the 64-bit value, matching the production target.
    #warning "Compiling on x86 host — this is for syntax check only, not a real AML target."
    #define AML64
#else
    #error "AML supports only ARM (armeabi-v7a) and ARM64 (arm64-v8a)"
#endif

// Per-ABI value selector
#ifdef AML32
    #define BYBIT(v32, v64) (v32)
#else
    #define BYBIT(v32, v64) (v64)
#endif

// Provided by MYMOD after init priority 101
extern IAML* aml;
extern Logger* logger;
extern Config* cfg;   // only non-null if MYMODCFG is used

struct ModInfo
{
    const char* _guid, *_name, *_version, *_author;
    int _major, _minor, _rev, _build;
    const char* GUID()         { return _guid; }
    const char* Name()         { return _name; }
    const char* VersionString(){ return _version; }
    const char* Author()       { return _author; }
    int Major()                { return _major; }
    int Minor()                { return _minor; }
    int Revision()             { return _rev; }
    int Build()                { return _build; }
};
extern ModInfo* modinfo;
extern ModInfo  modinfoLocal;

// Extern exports AML looks up
extern "C" __attribute__((visibility("default"))) ModInfo* __GetModInfo();

// Dependency declaration macros (no-op in placeholder)
#define BEGIN_DEPLIST()           struct _DepDummy {
#define ADD_DEPENDENCY(guid)
#define ADD_DEPENDENCY_VER(guid, ver)
#define END_DEPLIST()             } _depDummyInstance;

// Game restriction (no-op in placeholder)
#define NEEDGAME(pkg)

// Lifecycle callback macros — define the matching extern "C" fn in your .cpp
#define ON_MOD_PRELOAD()        extern "C" __attribute__((visibility("default"))) void OnModPreLoad()
#define ON_MOD_LOAD()           extern "C" __attribute__((visibility("default"))) void OnModLoad()
#define ON_ALL_MODS_LOAD()      extern "C" __attribute__((visibility("default"))) void OnAllModsLoaded()
#define ON_MOD_UNLOAD()         extern "C" __attribute__((visibility("default"))) void OnModUnload()
#define ON_GAME_CRASH()         extern "C" __attribute__((visibility("default"))) void OnGameCrash(const char* library, int sig, int code, uintptr_t libaddr, void* mcontext)
#define ON_NEW_INTERFACE()      extern "C" __attribute__((visibility("default"))) void OnInterfaceAdded(const char* name, const void* ptr)
#define UPDATER_URL()           extern "C" __attribute__((visibility("default"))) const char* OnUpdaterURLRequested()

// MYMOD — declares modinfo + auto-fills aml
#define MYMOD(guid, name, ver, author) \
    ModInfo modinfoLocal = { #guid, #name, #ver, #author, 0,0,0,0 }; \
    ModInfo* modinfo = &modinfoLocal; \
    IAML* aml = nullptr; \
    Logger* logger = nullptr; \
    Config* cfg = nullptr; \
    static void _aml_init() __attribute__((constructor(101))); \
    static void _aml_init() { \
        aml = (IAML*)GetInterface("AMLInterface"); \
        logger = (Logger*)GetInterface("AMLLogger"); \
    } \
    extern "C" __attribute__((visibility("default"))) ModInfo* __GetModInfo() { return modinfo; }

// MYMODCFG — MYMOD + global Config* cfg
#define MYMODCFG(guid, name, ver, author) \
    MYMOD(guid, name, ver, author) \
    static void _aml_cfg_init() __attribute__((constructor(102))); \
    static void _aml_cfg_init() { \
        if (aml) cfg = new Config(#guid); \
    }

// Helper macros from the real AML
#define SET_TO(var, addr)        (*(void**)&(var) = (void*)(addr))
#define SET_TO_PTR(var, ptraddr) (*(void**)&(var) = *(void**)(ptraddr))
#define SETSYM_TO(var, lib, sym) ((var) = (decltype(var))aml->GetSym((lib), (sym)))
#define SETSYM_TO_PTR(var, lib, sym) (SET_TO_PTR(var, aml->GetSym((lib), (sym))))
#define AS_ADDR(var)             (*(uintptr_t*)&(var))

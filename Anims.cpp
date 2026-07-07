/*
 * Anims — VehFuncs AML Port
 *
 * Original PC source: VehFuncs/Anims.cpp
 * Porting notes: Vehicle animation extras. PC hooks CAnimBlendAssociation etc. Needs arm64 equivalents of animation-related addresses. No x86 inline asm.
 */

#include "FeatureRegistry.h"
#include <mod/amlmod.h>
#include <mod/logger.h>

// External globals from main.cpp
extern uintptr_t pGame;
extern void*     hGame;

class Anims_Feature : public IFeature
{
public:
    const char* Name() const override         { return "Anims"; }
    const char* StatusString() const override { return "stub"; }

    void Init() override
    {
        // TODO(port): port PC hook addresses from VehFuncs/Anims.cpp.
        // Each PC address (0x4xxxxx - 0xBFFFFF) must be re-found in the arm64
        // libGTASA.so binary. See docs/ADDRESS_MAP.md for the complete list.
        //
        // Once an arm64 offset is found, install the hook like:
        //   HOOK(SomeFunction, pGame + 0xNEW_ARM64_OFFSET);
        //
        // Pattern-scan fallback (more version-robust):
        //   uintptr_t addr = aml->PatternScan("AA BB ? CC DD", "libGTASA.so");
        //   if (addr) { /* install hook */ }

        logger->Info("[Anims] init skipped — status: stub");
        logger->Info("[Anims] porting notes: Vehicle animation extras. PC hooks CAnimBlendAssociation etc. Needs arm64 equivalents of animation-related addresses. No x86 inline asm.");
    }

    void Shutdown() override
    {
        // TODO(port): undo hooks here if ToggleHook/DeHook was used.
    }
};

REGISTER_FEATURE(Anims);

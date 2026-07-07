# VehFuncs AML Port — Android.mk
#
# AML naming convention (matches AMLLA / FLA / etc.):
#   armeabi-v7a  -> libVehFuncs.so   (32-bit)
#   arm64-v8a    -> libVehFuncs64.so (64-bit)   <-- what we ship for v2.10 64-bit
#
# CRITICAL: This build requires the upstream AML source files (logger.cpp,
# config.cpp, and the full mod/ headers directory) to be present at
# .aml-upstream/. Run scripts/bootstrap-aml-headers.sh first, or let
# GitHub Actions do it for you.
#
# The bootstrap clones https://github.com/RusJJ/AndroidModLoader.git into
# .aml-upstream/. We then compile logger.cpp + config.cpp from there directly
# into our .so (this is how the official AML template_of_mod works — those
# files define the global `logger` and `cfg` pointers your mod uses).

LOCAL_PATH := $(call my-dir)

# ---- Sanity check: upstream AML must be present ----
ifeq ($(wildcard $(LOCAL_PATH)/.aml-upstream/mod/amlmod.h),)
$(error ============================================================)
$(error AML upstream headers not found at .aml-upstream/mod/)
$(error Run: ./scripts/bootstrap-aml-headers.sh)
$(error Or on GitHub Actions: the workflow does this automatically.)
$(error ============================================================)
endif

# ---- VehFuncs sources ----
VF_SOURCES := \
    src/main.cpp \
    src/IniReader/IniReader.cpp \
    src/PerlinNoise/SimplexNoise.cpp \
    src/features/Anims.cpp \
    src/features/ApplyGSX.cpp \
    src/features/AtomicsVisibility.cpp \
    src/features/Characteristics.cpp \
    src/features/CheckRepair.cpp \
    src/features/CustomSeed.cpp \
    src/features/CustomVisibilityRender.cpp \
    src/features/DamageableRearWings.cpp \
    src/features/DigitalOdometer.cpp \
    src/features/DigitalSpeedo.cpp \
    src/features/FixMaterials.cpp \
    src/features/Footpegs.cpp \
    src/features/GearAndFan.cpp \
    src/features/HandsFix.cpp \
    src/features/IndieVehHandlingsAPI.cpp \
    src/features/LoadModel.cpp \
    src/features/MatrixBackup.cpp \
    src/features/Pedal.cpp \
    src/features/Patches.cpp \
    src/features/PopupLights.cpp \
    src/features/RecursiveExtras.cpp \
    src/features/Shake.cpp \
    src/features/Spoiler.cpp \
    src/features/Steer.cpp \
    src/features/Trifork.cpp \
    src/features/Wheel.cpp

# ---- AML upstream sources (define logger + cfg globals) ----
AML_SOURCES := \
    .aml-upstream/mod/logger.cpp \
    .aml-upstream/mod/config.cpp

# ---- Build the 64-bit variant: libVehFuncs64.so ----
include $(CLEAR_VARS)
LOCAL_MODULE               := VehFuncs64
LOCAL_CPP_EXTENSION        := .cpp .cc
# Include path order matters: .aml-upstream first so real AML headers
# take precedence over any placeholders. Then src/ for our own headers.
LOCAL_C_INCLUDES           := \
    $(LOCAL_PATH)/.aml-upstream \
    $(LOCAL_PATH)/src
LOCAL_LDLIBS               := -llog
LOCAL_SRC_FILES            := $(VF_SOURCES) $(AML_SOURCES)
LOCAL_CFLAGS               := -O2 -DNDEBUG -Wall -Wno-unused-parameter -Wno-unused-variable
LOCAL_CPPFLAGS             := -std=c++17 -fexceptions -frtti
include $(BUILD_SHARED_LIBRARY)

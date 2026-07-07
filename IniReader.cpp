/*
 * IniReader.cpp — VehFuncs AML Port
 *
 * Implements SetIniPath() portably. On PC the original used
 * GetModuleFileNameA to find the .ini next to the .dll. On Android AML
 * we resolve against AML's config path (typically
 * /sdcard/Android/data/com.rockstargames.gtasa/files/).
 */

#include "IniReader.h"
#include <mod/amlmod.h>  // for aml->GetConfigPath()
#include <mod/logger.h>
#include <sys/stat.h>
#include <unistd.h>

const char* VehFuncs_GetConfigDir()
{
    if (aml) {
        const char* p = aml->GetConfigPath();
        if (p && *p) return p;
    }
    // Fallback: AML's per-game files dir
    return "/sdcard/Android/data/com.rockstargames.gtasa/files/";
}

void CIniReader::SetIniPath(std::string_view szFileName)
{
    std::string fname(szFileName);

    // Absolute path? (starts with '/') — use as-is.
    if (!fname.empty() && fname[0] == '/') {
        m_szFileName = fname;
    }
    else if (fname.empty()) {
        // Default: config dir + mod name + ".ini"
        m_szFileName = std::string(VehFuncs_GetConfigDir()) + "VehFuncs.ini";
    }
    else {
        // Relative: prepend config dir
        std::string dir = VehFuncs_GetConfigDir();
        if (!dir.empty() && dir.back() != '/')
            dir.push_back('/');
        m_szFileName = dir + fname;
    }

    // Attempt to load. If file doesn't exist, data stays empty — that's fine,
    // callers fall back to defaults.
    data.load_file(m_szFileName);

    if (logger) {
        logger->Info("[IniReader] path = %s (sections=%zu)",
                     m_szFileName.c_str(), data.size());
    }
}

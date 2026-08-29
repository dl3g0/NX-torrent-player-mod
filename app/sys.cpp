#include "sys.hpp"

#include <borealis.hpp>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace sys
{

static bool cpuBoostActive = false;

void setCpuBoost(bool enable)
{
#ifdef __SWITCH__
    if (cpuBoostActive == enable) return;
    cpuBoostActive = enable;
    if (enable)
    {
        Result rc = appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
        brls::Logger::info("[sys] CPU Boost ON (1785 MHz FastLoad): rc=0x{:x}", rc);
    }
    else
    {
        Result rc = appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
        brls::Logger::info("[sys] CPU Boost OFF (1020 MHz Normal): rc=0x{:x}", rc);
    }
#endif
}

void preventSleep(bool enable)
{
#ifdef __SWITCH__
    appletSetAutoSleepDisabled(enable);
    appletSetMediaPlaybackState(enable);
#endif
}

std::string formatSpeed(double bytesPerSec)
{
    if (bytesPerSec <= 0.0) return "0 KB/s";
    char buf[32];
    double mb = bytesPerSec / (1024.0 * 1024.0);
    if (mb >= 1.0)
    {
        std::snprintf(buf, sizeof(buf), "%.1f MB/s", mb);
        return buf;
    }
    double kb = bytesPerSec / 1024.0;
    std::snprintf(buf, sizeof(buf), "%.0f KB/s", kb);
    return buf;
}

std::string formatSize(int64_t bytes)
{
    if (bytes <= 0) return "0 MB";
    char buf[32];
    double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
    {
        std::snprintf(buf, sizeof(buf), "%.2f GB", gb);
        return buf;
    }
    double mb = (double)bytes / (1024.0 * 1024.0);
    std::snprintf(buf, sizeof(buf), "%.0f MB", mb);
    return buf;
}

} // namespace sys

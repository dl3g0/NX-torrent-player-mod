#pragma once

#include <cstdint>
#include <string>

namespace sys
{

// Sets CPU boost mode (1785 MHz Type1 for FastLoad vs 1020 MHz Disabled).
void setCpuBoost(bool enable);

// Prevents console auto-sleep/inactivity dimming during playback or downloads.
void preventSleep(bool enable);

// Formats download speed in bytes/sec to human string (e.g. "2.4 MB/s", "650 KB/s").
std::string formatSpeed(double bytesPerSec);

// Formats size in bytes to human string (e.g. "1.45 GB", "320 MB").
std::string formatSize(int64_t bytes);

} // namespace sys

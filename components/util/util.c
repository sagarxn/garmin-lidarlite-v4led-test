#include "util.h"

#include <stdio.h>
#include <time.h>
#include <stdint.h>

uint32_t util_tm_to_fat32_time(struct tm *tm_info)
{
    uint32_t fat32_time = 0;

    fat32_time |= ((tm_info->tm_year - 80) & 0x7F) << 25; // Year since 1980
    fat32_time |= ((tm_info->tm_mon + 1) & 0x0F) << 21;   // Month (1-12)
    fat32_time |= (tm_info->tm_mday & 0x1F) << 16;        // Day (1-31)
    fat32_time |= (tm_info->tm_hour & 0x1F) << 11;        // Hour (0-23)
    fat32_time |= (tm_info->tm_min & 0x3F) << 5;          // Minute (0-59)
    fat32_time |= (tm_info->tm_sec / 2) & 0x1F;           // Second divided by 2 (0-29)

    return fat32_time;
}

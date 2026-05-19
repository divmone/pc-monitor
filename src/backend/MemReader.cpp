//
// Created by divmone on 5/19/2026.
//

#include "MemReader.hpp"

#include <fstream>
#include <string>

MemInfo MemReader::read() {
    MemInfo m;
    std::ifstream f("/proc/meminfo");
    if (!f) return m;

    long avail = 0;
    long swap_free = 0;
    std::string key;
    long val;
    std::string unit;

    while (f >> key >> val) {
        f >> unit;
        if      (key == "MemTotal:")     m.total_kb      = val;
        else if (key == "MemAvailable:") avail           = val;
        else if (key == "Cached:")       m.cached_kb     = val;
        else if (key == "Buffers:")      m.buffers_kb    = val;
        else if (key == "SwapTotal:")    m.swap_total_kb = val;
        else if (key == "SwapFree:")     swap_free       = val;
    }

    m.free_kb      = avail;
    m.used_kb      = m.total_kb - avail;
    m.swap_used_kb = m.swap_total_kb - swap_free;
    return m;
}

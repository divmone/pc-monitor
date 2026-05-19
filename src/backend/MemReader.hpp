//
// Created by divmone on 5/19/2026.
//

#pragma once
#include <nlohmann/json.hpp>

struct MemInfo {
    long total_kb      = 0;
    long used_kb       = 0;
    long free_kb       = 0;
    long cached_kb     = 0;
    long buffers_kb    = 0;
    long swap_total_kb = 0;
    long swap_used_kb  = 0;
};

inline nlohmann::json to_json(const MemInfo& m) {
    return {
        {"total_kb",      m.total_kb},
        {"used_kb",       m.used_kb},
        {"free_kb",       m.free_kb},
        {"cached_kb",     m.cached_kb},
        {"buffers_kb",    m.buffers_kb},
        {"swap_total_kb", m.swap_total_kb},
        {"swap_used_kb",  m.swap_used_kb},
    };
}

class MemReader {
public:
    MemInfo read();
};

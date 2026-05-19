//
// Created by divmone on 5/19/2026.
//

#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

struct ProcessInfo {
    int    pid      = 0;
    int    ppid     = 0;
    std::string name;
    std::string user;
    char   state    = '?';
    double cpu_pct  = 0;
    double mem_pct  = 0;
    long   mem_kb   = 0;
    long   threads  = 0;
    long   priority = 0;
};

inline nlohmann::json to_json(const ProcessInfo& p) {
    return {
        {"pid",      p.pid},
        {"ppid",     p.ppid},
        {"name",     p.name},
        {"user",     p.user},
        {"state",    std::string(1, p.state)},
        {"cpu_pct",  p.cpu_pct},
        {"mem_pct",  p.mem_pct},
        {"mem_kb",   p.mem_kb},
        {"threads",  p.threads},
        {"priority", p.priority},
    };
}

class ProcessReader {
public:
    std::vector<ProcessInfo> read(long total_mem_kb);

private:
    static bool isPidDir(const char* name);
    static std::string lookupUsername(int pid);
    static bool parseStat(int pid, ProcessInfo& info, unsigned long& ticks);
    static unsigned long readTotalCpuTicks();

    std::unordered_map<int, unsigned long> prev;
    unsigned long prevTotalTicks = 0;
};

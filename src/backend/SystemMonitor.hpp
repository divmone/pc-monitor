//
// Created by divmone on 5/19/2026.
//
#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "CpuReader.hpp"
#include "MemReader.hpp"
#include "ProcessReader.hpp"


struct Snapshot {
    std::chrono::system_clock::time_point timestamp;
    std::string                    hostname;
    double                         uptime  = 0;
    double                         load1   = 0;
    double                         load5   = 0;
    double                         load15  = 0;
    std::vector<CpuCoreUsageInfo>  cpus;
    MemInfo                        mem;
    std::vector<ProcessInfo>       processes;
};

inline nlohmann::json to_json(const Snapshot& s, std::size_t max_processes = 50) {
    nlohmann::json cpus = nlohmann::json::array();
    for (const auto& c : s.cpus) cpus.push_back(to_json(c));

    nlohmann::json procs = nlohmann::json::array();
    const std::size_t n = std::min(max_processes, s.processes.size());
    for (std::size_t i = 0; i < n; ++i) procs.push_back(to_json(s.processes[i]));

    return {
        {"timestamp_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                            s.timestamp.time_since_epoch()).count()},
        {"hostname",     s.hostname},
        {"uptime",       s.uptime},
        {"load",         {s.load1, s.load5, s.load15}},
        {"cpus",         std::move(cpus)},
        {"mem",          to_json(s.mem)},
        {"processes",    std::move(procs)},
    };
}

class SystemMonitor {
public:
    Snapshot snapshot();

private:
    CpuReader     cpuReader_;
    MemReader     memReader_;
    ProcessReader processReader_;
};

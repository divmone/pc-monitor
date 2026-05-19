//
// Created by divmone on 5/19/2026.
//

#include "SystemMonitor.hpp"

#include <fstream>

namespace {

std::string readHostname() {
    std::ifstream f("/proc/sys/kernel/hostname");
    std::string h;
    if (f) std::getline(f, h);
    return h.empty() ? "unknown" : h;
}

double readUptime() {
    std::ifstream f("/proc/uptime");
    double up = 0;
    if (f) f >> up;
    return up;
}

void readLoadavg(double& l1, double& l5, double& l15) {
    std::ifstream f("/proc/loadavg");
    if (f) f >> l1 >> l5 >> l15;
}

}

Snapshot SystemMonitor::snapshot() {
    Snapshot s;
    s.timestamp = std::chrono::system_clock::now();
    s.hostname  = readHostname();
    s.uptime    = readUptime();
    readLoadavg(s.load1, s.load5, s.load15);

    s.cpus      = cpuReader_.read();
    s.mem       = memReader_.read();
    s.processes = processReader_.read(s.mem.total_kb);
    return s;
}

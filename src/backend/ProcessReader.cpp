//
// Created by divmone on 5/19/2026.
//

#include "ProcessReader.hpp"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

std::vector<ProcessInfo> ProcessReader::read(long total_mem_kb) {
    unsigned long cur_total   = readTotalCpuTicks();
    unsigned long delta_total = (prevTotalTicks > 0 && cur_total > prevTotalTicks)
                                  ? (cur_total - prevTotalTicks) : 1;
    prevTotalTicks = cur_total;

    int ncpus = std::max(1, (int) sysconf(_SC_NPROCESSORS_ONLN));

    std::vector<ProcessInfo> result;
    std::unordered_map<int, unsigned long> next_prev;

    DIR* dir = opendir("/proc");
    if (!dir) return result;

    while (auto* ent = readdir(dir)) {
        if (!isPidDir(ent->d_name)) continue;
        int pid = std::atoi(ent->d_name);

        ProcessInfo p;
        unsigned long ticks = 0;
        if (!parseStat(pid, p, ticks)) continue;

        unsigned long prev_ticks = 0;
        if (auto it = prev.find(pid); it != prev.end()) prev_ticks = it->second;
        unsigned long delta_proc = (ticks > prev_ticks) ? (ticks - prev_ticks) : 0;
        p.cpu_pct = std::min(100.0 * delta_proc / delta_total, 100.0 * ncpus);
        p.mem_pct = total_mem_kb > 0 ? 100.0 * p.mem_kb / total_mem_kb : 0.0;
        p.user    = lookupUsername(pid);

        next_prev[pid] = ticks;
        result.push_back(std::move(p));
    }
    closedir(dir);
    prev = std::move(next_prev);

    std::sort(result.begin(), result.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.cpu_pct > b.cpu_pct;
              });
    return result;
}

bool ProcessReader::isPidDir(const char* name) {
    for (const char* p = name; *p; ++p)
        if (*p < '0' || *p > '9') return false;
    return name[0] != '\0';
}

std::string ProcessReader::lookupUsername(int pid) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/status", pid);
    std::ifstream f(path);
    if (!f) return {};

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Uid:", 0) != 0) continue;
        unsigned uid = 0;
        std::sscanf(line.c_str(), "Uid: %u", &uid);
        if (auto* pw = getpwuid(uid)) return pw->pw_name;
        return std::to_string(uid);
    }
    return {};
}

bool ProcessReader::parseStat(int pid, ProcessInfo& info, unsigned long& ticks) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    std::ifstream f(path);
    if (!f) return false;

    std::string raw;
    std::getline(f, raw);
    if (raw.empty()) return false;

    auto lp = raw.find('(');
    auto rp = raw.rfind(')');
    if (lp == std::string::npos || rp == std::string::npos || rp < lp) return false;

    info.pid  = pid;
    info.name = raw.substr(lp + 1, rp - lp - 1);

    std::istringstream ss(raw.substr(rp + 2));
    char  state = '?';
    long  ppid_l = 0, prio = 0, threads = 0, rss = 0;
    long  nice_v = 0, itr = 0;
    unsigned long utime = 0, stime = 0, starttime = 0, vsize = 0;

    ss >> state >> ppid_l;
    for (int i = 0; i < 9; ++i) { long tmp; ss >> tmp; }
    ss >> utime >> stime;
    for (int i = 0; i < 2; ++i) { long tmp; ss >> tmp; }
    ss >> prio >> nice_v >> threads >> itr >> starttime >> vsize >> rss;

    info.state    = state;
    info.ppid     = (int) ppid_l;
    info.priority = prio;
    info.threads  = threads;
    info.mem_kb   = rss * (sysconf(_SC_PAGESIZE) / 1024);
    ticks         = utime + stime;
    return true;
}

unsigned long ProcessReader::readTotalCpuTicks() {
    std::ifstream f("/proc/stat");
    if (!f) return 0;

    std::string label;
    unsigned long u=0, n=0, s=0, i=0, io=0, irq=0, sirq=0;
    f >> label >> u >> n >> s >> i >> io >> irq >> sirq;
    return u + n + s + i + io + irq + sirq;
}

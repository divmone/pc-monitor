//
// Created by divmone on 5/19/2026.
//

#pragma once
#include <algorithm>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

struct CpuCoreTick {
    unsigned long user=0, nice=0, system=0,
                  idle=0, iowait=0, irq=0, softirq=0;

    unsigned long total()  const { return user+nice+system+idle+iowait+irq+softirq; }
    unsigned long active() const { return user+nice+system+irq+softirq; }
};

struct CpuCoreUsageInfo {
    int    id;
    double usage  = 0;
    double user   = 0;
    double system = 0;
    double iowait = 0;

    static CpuCoreUsageInfo get(int id,
                              const CpuCoreTick& cur,
                              const CpuCoreTick& prev) {
        CpuCoreUsageInfo c;
        c.id = id;
        unsigned long dt = cur.total() - prev.total();
        if (dt == 0) return c;
        c.usage  = 100.0 * (cur.active() - prev.active()) / dt;
        c.user   = 100.0 * (cur.user   - prev.user)   / dt;
        c.system = 100.0 * (cur.system - prev.system) / dt;
        c.iowait = 100.0 * (cur.iowait - prev.iowait) / dt;
        c.usage  = std::clamp(c.usage, 0.0, 100.0);
        return c;
    }
};

inline nlohmann::json to_json(const CpuCoreUsageInfo& c) {
    return {
        {"id",     c.id},
        {"usage",  c.usage},
        {"user",   c.user},
        {"system", c.system},
        {"iowait", c.iowait},
    };
}

class CpuReader {
public:
    std::vector<CpuCoreUsageInfo> read();

private:
    std::vector<CpuCoreTick> readTicks();

    std::vector<CpuCoreTick> prev;
};

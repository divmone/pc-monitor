//
// Created by divmone on 5/19/2026.
//

#include "CpuReader.hpp"

std::vector<CpuCoreUsageInfo> CpuReader::read() {
    auto cur = readTicks();
    std::vector<CpuCoreUsageInfo> result;
    result.reserve(cur.size());
    for (size_t i = 0; i < cur.size(); ++i) {
        int id = (i == 0) ? -1 : static_cast<int>(i) - 1;
        result.push_back(
            (prev.size() == cur.size())
                ? CpuCoreUsageInfo::get(id, cur[i], prev[i])
                : CpuCoreUsageInfo{id}
        );
    }
    prev = std::move(cur);
    return result;
}

std::vector<CpuCoreTick> CpuReader::readTicks() {
    std::ifstream file("/proc/stat");
    std::vector<CpuCoreTick> result;
    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("cpu", 0) != 0) break;

        std::istringstream ss(line);
        std::string label;
        CpuCoreTick t;
        ss >> label >> t.user >> t.nice >> t.system >> t.idle
           >> t.iowait >> t.irq >> t.softirq;
        result.push_back(t);
    }

    return result;
}

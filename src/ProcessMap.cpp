#include <amdump/ProcessMap.hpp>
#include <stdexcept>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <string>

namespace Amdump {

static const char* SKIP_NAMES[] = { "[vsyscall]", "[vvar]", "[vdso]" };

static bool ShouldSkip(const char* line) {
    for (const char* name : SKIP_NAMES)
        if (std::strstr(line, name)) return true;
    return false;
}

ProcessMap::ProcessMap(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) throw std::runtime_error("cannot open " + path);
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (ShouldSkip(line)) continue;
        std::uint64_t start, end;
        char perms[8];
        if (std::sscanf(line, "%" SCNx64 "-%" SCNx64 " %7s", &start, &end, perms) != 3) continue;
        _regions.push_back({start, end});
    }
    std::fclose(f);
    if (_regions.empty()) throw std::runtime_error("no mapped regions found");
}

const std::vector<Region>& ProcessMap::Regions() const { return _regions; }

std::uint64_t ProcessMap::Base() const { return _regions.front().start; }

std::uint64_t ProcessMap::Top() const { return _regions.back().end; }

}

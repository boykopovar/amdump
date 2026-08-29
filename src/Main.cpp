#include <amdump/ProcessFinder.hpp>
#include <amdump/ProcessMap.hpp>
#include <amdump/Dumper.hpp>
#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <iostream>

static const std::uint64_t DEFAULT_MAX_GB = 10;

static void PrintUsage() {
    std::cerr << "usage: amdump <pid|package> <outfile> [--max-gb <N>] [--only-show-regions]\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    std::uint64_t maxGb = DEFAULT_MAX_GB;
    bool onlyShowRegions = false;
    for (int i = 3; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--only-show-regions") {
            onlyShowRegions = true;
        } else if (flag == "--max-gb") {
            if (i + 1 >= argc) {
                std::cerr << "error: --max-gb requires a value\n";
                return 1;
            }
            ++i;
            char* end;
            long val = std::strtol(argv[i], &end, 10);
            if (*end != '\0' || val <= 0) {
                std::cerr << "error: --max-gb must be a positive integer\n";
                return 1;
            }
            maxGb = static_cast<std::uint64_t>(val);
        } else {
            PrintUsage();
            return 1;
        }
    }
    try {
        std::string arg = argv[1];
        int pid = Amdump::ProcessFinder::IsAllDigits(arg)
            ? std::atoi(arg.c_str())
            : Amdump::ProcessFinder::FindByPackage(arg);
        std::fprintf(stderr, "pid=%d\n", pid);
        Amdump::ProcessMap map(pid);
        if (onlyShowRegions) {
            const auto& regions = map.Regions();
            std::fprintf(stdout, "%-18s %-18s %s\n", "start", "end", "name");
            for (const auto& r : regions)
                std::fprintf(stdout, "0x%016" PRIx64 " 0x%016" PRIx64 " %s\n",
                    r.start, r.end, r.name.c_str());
            return 0;
        }
        std::fprintf(stderr, "base=0x%" PRIx64 " top=0x%" PRIx64 " regions=%zu limit=%" PRIu64 "GB\n",
            map.Base(), map.Top(), map.Regions().size(), maxGb);
        Amdump::Dumper dumper(pid, map, maxGb * 1024 * 1024 * 1024);
        dumper.Run(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

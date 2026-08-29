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
    std::cerr << "usage: amdump <pid|package> <outfile> [--max-gb <N>]\n";
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 5) {
        PrintUsage();
        return 1;
    }
    std::uint64_t maxGb = DEFAULT_MAX_GB;
    if (argc == 5) {
        std::string flag = argv[3];
        if (flag != "--max-gb") {
            PrintUsage();
            return 1;
        }
        char* end;
        long val = std::strtol(argv[4], &end, 10);
        if (*end != '\0' || val <= 0) {
            std::cerr << "error: --max-gb must be a positive integer\n";
            return 1;
        }
        maxGb = static_cast<std::uint64_t>(val);
    }
    std::uint64_t maxBytes = maxGb * 1024 * 1024 * 1024;
    try {
        std::string arg = argv[1];
        int pid = Amdump::ProcessFinder::IsAllDigits(arg)
            ? std::atoi(arg.c_str())
            : Amdump::ProcessFinder::FindByPackage(arg);
        std::fprintf(stderr, "pid=%d\n", pid);
        Amdump::ProcessMap map(pid);
        std::fprintf(stderr, "base=0x%" PRIx64 " top=0x%" PRIx64 " regions=%zu limit=%" PRIu64 "GB\n",
            map.Base(), map.Top(), map.Regions().size(), maxGb);
        Amdump::Dumper dumper(pid, map, maxBytes);
        dumper.Run(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

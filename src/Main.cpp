#include <amdump/ProcessFinder.hpp>
#include <amdump/ProcessMap.hpp>
#include <amdump/Dumper.hpp>
#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: amdump <pid|package> <outfile>\n";
        return 1;
    }
    try {
        std::string arg = argv[1];
        int pid = Amdump::ProcessFinder::IsAllDigits(arg)
            ? std::atoi(arg.c_str())
            : Amdump::ProcessFinder::FindByPackage(arg);
        std::fprintf(stderr, "pid=%d\n", pid);
        Amdump::ProcessMap map(pid);
        std::fprintf(stderr, "base=0x%" PRIx64 " top=0x%" PRIx64 " total=%" PRIu64 " MB\n",
            map.Base(), map.Top(), (map.Top() - map.Base()) / (1024 * 1024));
        Amdump::Dumper dumper(pid, map);
        dumper.Run(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

#include <amdump/ProcessMap.hpp>
#include <amdump/Dumper.hpp>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: amdump <pid> <outfile>\n";
        return 1;
    }
    try {
        int pid = std::atoi(argv[1]);
        Amdump::ProcessMap map(pid);
        std::fprintf(stderr, "base=0x%lx top=0x%lx total=%lu MB\n",
            map.Base(), map.Top(), (map.Top() - map.Base()) / (1024 * 1024));
        Amdump::Dumper dumper(pid, map);
        dumper.Run(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

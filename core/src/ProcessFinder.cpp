#include <amdump/ProcessFinder.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>

namespace Amdump {

static std::string ReadCmdline(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open " + path);
    char buf[512];
    std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    return std::string(buf);
}

bool ProcessFinder::IsAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

int ProcessFinder::FindByPackage(const std::string& package) {
    int found = -1;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (!IsAllDigits(name)) continue;
        int pid = std::atoi(name.c_str());
        std::string cmdline;
        try {
            cmdline = ReadCmdline(pid);
        } catch (...) {
            continue;
        }
        if (cmdline != package) continue;
        if (found != -1)
            throw std::runtime_error("duplicate process found for package: " + package);
        found = pid;
    }
    if (found == -1)
        throw std::runtime_error("process not found for package: " + package);
    return found;
}

}

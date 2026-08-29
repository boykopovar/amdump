#pragma once
#include <amdump/ProcessMap.hpp>
#include <string>
#include <cstdint>

namespace Amdump {

class Dumper {
public:
    Dumper(int pid, const ProcessMap& map, std::uint64_t maxBytes);
    void Run(const std::string& outPath);

private:
    int _pid;
    const ProcessMap& _map;
    std::uint64_t _maxBytes;
};

}

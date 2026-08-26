#pragma once
#include <amdump/ProcessMap.hpp>
#include <string>

namespace Amdump {

class Dumper {
public:
    Dumper(int pid, const ProcessMap& map);
    void Run(const std::string& outPath);

private:
    int _pid;
    const ProcessMap& _map;
};

}

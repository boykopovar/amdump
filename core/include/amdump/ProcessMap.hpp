#pragma once
#include <amdump/Region.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace Amdump {

class ProcessMap {
public:
    explicit ProcessMap(int pid, const std::vector<std::string>& customSoPrefixes = {});
    const std::vector<Region>& Regions() const;
    std::uint64_t Base() const;
    std::uint64_t Top() const;
    static std::uint16_t EMachine(int pid);

private:
    std::vector<Region> _regions;
};

}

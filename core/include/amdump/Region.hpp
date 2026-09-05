#pragma once
#include <cstdint>
#include <string>

namespace Amdump {

struct Region {
    std::uint64_t start;
    std::uint64_t end;
    std::uint32_t flags;
    std::uint64_t fileOffset;
    std::string name;
};

}

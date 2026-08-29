#pragma once
#include <cstdint>

namespace Amdump {

struct Region {
    std::uint64_t start;
    std::uint64_t end;
    std::uint32_t flags;
};

}

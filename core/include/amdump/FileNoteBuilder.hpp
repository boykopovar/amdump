#pragma once
#include <amdump/Region.hpp>
#include <vector>
#include <cstdint>

namespace Amdump {

class FileNoteBuilder {
public:
    static std::vector<std::uint8_t> Build(const std::vector<Region>& regions);

private:
    static constexpr std::uint64_t PAGE_SIZE = 0x1000;

    static void AppendPadded(std::vector<std::uint8_t>& buf, const void* data, std::size_t size);
    static std::vector<std::uint8_t> BuildDescriptor(const std::vector<const Region*>& named);
};

}

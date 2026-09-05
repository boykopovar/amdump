#include <amdump/FileIo.hpp>
#include <stdexcept>
#include <algorithm>

namespace Amdump {
namespace FileIo {

void Write(std::FILE* f, const void* data, std::size_t size) {
    if (std::fwrite(data, 1, size, f) != size)
        throw std::runtime_error("fwrite failed");
}

void WriteZeroes(std::FILE* f, const char* zeroBuf, std::size_t zeroBufSize, std::uint64_t count) {
    while (count > 0) {
        std::size_t chunk = static_cast<std::size_t>(std::min(count, static_cast<std::uint64_t>(zeroBufSize)));
        Write(f, zeroBuf, chunk);
        count -= chunk;
    }
}

std::uint64_t Tell(std::FILE* f) {
    long pos = std::ftell(f);
    if (pos < 0) throw std::runtime_error("ftell failed");
    return static_cast<std::uint64_t>(pos);
}

void Seek(std::FILE* f, std::uint64_t offset) {
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        throw std::runtime_error("fseek failed");
}

}
}

#pragma once
#include <cstdio>
#include <cstdint>

namespace Amdump {
namespace FileIo {

void Write(std::FILE* f, const void* data, std::size_t size);
void WriteZeroes(std::FILE* f, const char* zeroBuf, std::size_t zeroBufSize, std::uint64_t count);
std::uint64_t Tell(std::FILE* f);
void Seek(std::FILE* f, std::uint64_t offset);

}
}

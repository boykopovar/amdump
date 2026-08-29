#include <amdump/ProcessMap.hpp>
#include <stdexcept>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <string>
#include <elf.h>

namespace Amdump {

static const char* SKIP_NAMES[] = { "[vsyscall]", "[vvar]", "[vdso]" };

static bool ShouldSkip(const char* line) {
    for (const char* name : SKIP_NAMES)
        if (std::strstr(line, name)) return true;
    return false;
}

static std::uint32_t ParseFlags(const char* perms) {
    std::uint32_t flags = 0;
    if (perms[0] == 'r') flags |= PF_R;
    if (perms[1] == 'w') flags |= PF_W;
    if (perms[2] == 'x') flags |= PF_X;
    return flags;
}

ProcessMap::ProcessMap(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) throw std::runtime_error("cannot open " + path);
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (ShouldSkip(line)) continue;
        std::uint64_t start, end;
        char perms[8];
        if (std::sscanf(line, "%" SCNx64 "-%" SCNx64 " %7s", &start, &end, perms) != 3) continue;
        _regions.push_back({start, end, ParseFlags(perms)});
    }
    std::fclose(f);
    if (_regions.empty()) throw std::runtime_error("no mapped regions found");
}

const std::vector<Region>& ProcessMap::Regions() const { return _regions; }

std::uint64_t ProcessMap::Base() const { return _regions.front().start; }

std::uint64_t ProcessMap::Top() const { return _regions.back().end; }

static std::uint16_t ReadEMachine(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/exe";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open exe for pid " + std::to_string(pid));
    unsigned char ident[EI_NIDENT];
    if (std::fread(ident, 1, EI_NIDENT, f) != EI_NIDENT) {
        std::fclose(f);
        throw std::runtime_error("cannot read ELF ident from exe");
    }
    if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
        ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
    {
        std::fclose(f);
        throw std::runtime_error("exe is not an ELF file");
    }
    if (ident[EI_CLASS] != ELFCLASS64) {
        std::fclose(f);
        throw std::runtime_error("only ELF64 targets are supported");
    }
    if (ident[EI_DATA] != ELFDATA2LSB) {
        std::fclose(f);
        throw std::runtime_error("only little-endian targets are supported");
    }
    Elf64_Ehdr ehdr;
    std::rewind(f);
    if (std::fread(&ehdr, 1, sizeof(ehdr), f) != sizeof(ehdr)) {
        std::fclose(f);
        throw std::runtime_error("cannot read ELF header from exe");
    }
    std::fclose(f);
    return ehdr.e_machine;
}

std::uint16_t ProcessMap::EMachine(int pid) {
    return ReadEMachine(pid);
}

}

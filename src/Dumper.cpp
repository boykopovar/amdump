#include <amdump/Dumper.hpp>
#include <amdump/ProcessMap.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <string>
#include <algorithm>
#include <vector>
#include <elf.h>

namespace Amdump {

static const std::size_t BUF = 1 << 20;
static char _buf[BUF];
static char _zero[BUF];

Dumper::Dumper(int pid, const ProcessMap& map) : _pid(pid), _map(map) {}

static void Write(std::FILE* f, const void* data, std::size_t size) {
    if (std::fwrite(data, 1, size, f) != size)
        throw std::runtime_error("fwrite failed");
}

static void WriteZeroes(std::FILE* f, std::uint64_t count) {
    while (count > 0) {
        std::size_t chunk = static_cast<std::size_t>(std::min(count, static_cast<std::uint64_t>(BUF)));
        Write(f, _zero, chunk);
        count -= chunk;
    }
}

static std::uint64_t Ftell(std::FILE* f) {
    long pos = std::ftell(f);
    if (pos < 0) throw std::runtime_error("ftell failed");
    return static_cast<std::uint64_t>(pos);
}

static void Fseek(std::FILE* f, std::uint64_t offset) {
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        throw std::runtime_error("fseek failed");
}

void Dumper::Run(const std::string& outPath) {
    const auto& regions = _map.Regions();
    std::uint16_t emachine = ProcessMap::EMachine(_pid);

    std::string memPath = "/proc/" + std::to_string(_pid) + "/mem";
    std::FILE* memF = std::fopen(memPath.c_str(), "rb");
    if (!memF) throw std::runtime_error("cannot open " + memPath);

    std::FILE* outF = std::fopen(outPath.c_str(), "wb");
    if (!outF) {
        std::fclose(memF);
        throw std::runtime_error("cannot open output: " + outPath);
    }

    std::size_t nsegs = regions.size();
    std::uint64_t hdrsSize = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * nsegs;

    WriteZeroes(outF, hdrsSize);

    struct SegInfo {
        std::uint64_t fileOffset;
        std::uint64_t filesz;
    };
    std::vector<SegInfo> segInfos;
    segInfos.reserve(nsegs);

    std::uint64_t bytesRead = 0;
    std::uint64_t bytesZeroed = 0;
    int idx = 0;

    std::memset(_zero, 0, BUF);

    for (const auto& r : regions) {
        std::uint64_t fileOffset = Ftell(outF);
        std::uint64_t rsize = r.end - r.start;
        std::uint64_t done = 0;

        while (done < rsize) {
            std::uint64_t chunk = std::min(rsize - done, static_cast<std::uint64_t>(BUF));
            std::uint64_t vaddr = r.start + done;

            if (std::fseek(memF, static_cast<long>(vaddr), SEEK_SET) != 0)
                throw std::runtime_error("fseek on /proc/mem failed at 0x" + std::to_string(vaddr));

            std::size_t n = std::fread(_buf, 1, static_cast<std::size_t>(chunk), memF);
            if (n == 0) {
                Write(outF, _zero, static_cast<std::size_t>(chunk));
                bytesZeroed += chunk;
                done += chunk;
                continue;
            }
            Write(outF, _buf, n);
            bytesRead += static_cast<std::uint64_t>(n);
            done += static_cast<std::uint64_t>(n);
        }

        segInfos.push_back({fileOffset, rsize});
        std::fprintf(stderr, "\r[%d/%zu] 0x%" PRIx64 "-0x%" PRIx64,
            ++idx, nsegs, r.start, r.end);
    }

    std::uint64_t totalFileSize = Ftell(outF);

    Elf64_Ehdr ehdr;
    std::memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_LINUX;
    ehdr.e_type = ET_CORE;
    ehdr.e_machine = emachine;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = regions.front().start;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    if (nsegs >= 0xffff)
        throw std::runtime_error("too many segments for ELF phnum field");
    ehdr.e_phnum = static_cast<Elf64_Half>(nsegs);

    std::vector<Elf64_Phdr> phdrs(nsegs);
    for (std::size_t i = 0; i < nsegs; ++i) {
        Elf64_Phdr& ph = phdrs[i];
        std::memset(&ph, 0, sizeof(ph));
        ph.p_type = PT_LOAD;
        ph.p_flags = static_cast<Elf64_Word>(regions[i].flags);
        ph.p_offset = segInfos[i].fileOffset;
        ph.p_vaddr = regions[i].start;
        ph.p_paddr = regions[i].start;
        ph.p_filesz = segInfos[i].filesz;
        ph.p_memsz = segInfos[i].filesz;
        ph.p_align = 0x1000;
    }

    Fseek(outF, 0);
    Write(outF, &ehdr, sizeof(ehdr));
    Write(outF, phdrs.data(), sizeof(Elf64_Phdr) * nsegs);

    std::fclose(memF);
    std::fclose(outF);

    std::fprintf(stderr, "\n\n");
    std::fprintf(stderr, "regions   : %zu\n", nsegs);
    std::fprintf(stderr, "e_machine : 0x%x\n", static_cast<unsigned>(emachine));
    std::fprintf(stderr, "base      : 0x%" PRIx64 "\n", regions.front().start);
    std::fprintf(stderr, "top       : 0x%" PRIx64 "\n", regions.back().end);
    std::fprintf(stderr, "file size : %" PRIu64 " MB\n", totalFileSize / (1024 * 1024));
    std::fprintf(stderr, "read      : %" PRIu64 " MB\n", bytesRead / (1024 * 1024));
    std::fprintf(stderr, "zeroed    : %" PRIu64 " MB\n", bytesZeroed / (1024 * 1024));
    std::fprintf(stderr, "output    : %s\n", outPath.c_str());
}

}

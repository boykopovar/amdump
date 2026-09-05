#include <amdump/Dumper.hpp>
#include <amdump/ProcessMap.hpp>
#include <amdump/Elf64.hpp>
#include <amdump/FileIo.hpp>
#include <amdump/FileNoteBuilder.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <cerrno>
#include <string>
#include <algorithm>
#include <vector>

namespace Amdump {

static constexpr std::size_t READ_BUF_SIZE = 1 << 20;
static char _readBuf[READ_BUF_SIZE];
static char _zeroBuf[READ_BUF_SIZE];

Dumper::Dumper(const int pid, const ProcessMap& map, const std::uint64_t maxBytes)
    : _pid(pid), _map(map), _maxBytes(maxBytes) {}

std::uint64_t Dumper::EstimateTotalSize(const std::vector<std::uint8_t>& noteData) const {
    const auto& regions = _map.Regions();
    std::uint64_t totalSize = sizeof(Elf64Ehdr) + sizeof(Elf64Phdr) * (regions.size() + 1) + noteData.size();
    for (const auto& r : regions)
        totalSize += r.end - r.start;
    return totalSize;
}

void Dumper::CheckSizeLimit(std::uint64_t totalSize) const {
    std::fprintf(stderr, "estimated file size: %" PRIu64 " MB\n", totalSize / (1024 * 1024));
    if (totalSize > _maxBytes)
        throw std::runtime_error(
            "estimated dump size " + std::to_string(totalSize / (1024 * 1024)) +
            " MB exceeds limit " + std::to_string(_maxBytes / (1024 * 1024)) + " MB");
}

void Dumper::CopyRegionBytes(std::FILE* memF, std::FILE* outF, const Region& r, ReadStats& stats) {
    const std::uint64_t rsize = r.end - r.start;
    std::uint64_t done = 0;

    while (done < rsize) {
        std::uint64_t chunk = std::min(rsize - done, static_cast<std::uint64_t>(READ_BUF_SIZE));
        std::uint64_t vaddr = r.start + done;

        if (std::fseek(memF, static_cast<long>(vaddr), SEEK_SET) != 0)
            throw std::runtime_error("fseek on /proc/mem failed at 0x" + std::to_string(vaddr));

        errno = 0;
        std::size_t n = std::fread(_readBuf, 1, static_cast<std::size_t>(chunk), memF);
        if (n == 0) {
            const char* reason = std::feof(memF) ? "EOF" : std::strerror(errno);
            std::fprintf(stderr,
                "\nwarning: unreadable page at 0x%" PRIx64 " size 0x%" PRIx64 " in %s, reason: %s, zero-filled\n",
                vaddr, chunk, r.name.c_str(), reason);
            std::clearerr(memF);
            FileIo::Write(outF, _zeroBuf, static_cast<std::size_t>(chunk));
            stats.bytesZeroed += chunk;
            done += chunk;
            continue;
        }
        if (n < static_cast<std::size_t>(chunk)) {
            std::fprintf(stderr,
                "\nwarning: short read at 0x%" PRIx64 ", requested 0x%" PRIx64 " got 0x%zx in %s\n",
                vaddr, chunk, n, r.name.c_str());
        }
        FileIo::Write(outF, _readBuf, n);
        stats.bytesRead += static_cast<std::uint64_t>(n);
        done += static_cast<std::uint64_t>(n);
    }
}

Dumper::ReadStats Dumper::WriteRegionData(std::FILE* memF, std::FILE* outF,
                                           std::vector<SegmentLayout>& outLayouts) {
    const auto& regions = _map.Regions();
    ReadStats stats;
    int idx = 0;

    std::memset(_zeroBuf, 0, READ_BUF_SIZE);

    for (const auto& r : regions) {
        std::uint64_t fileOffset = FileIo::Tell(outF);
        CopyRegionBytes(memF, outF, r, stats);
        outLayouts.push_back({fileOffset, r.end - r.start});
        std::fprintf(stderr, "\r[%d/%zu] 0x%" PRIx64 "-0x%" PRIx64,
            ++idx, regions.size(), r.start, r.end);
    }
    return stats;
}

Elf64Ehdr Dumper::BuildEhdr(std::size_t nsegs) const {
    const auto& regions = _map.Regions();
    std::uint16_t emachine = ProcessMap::EMachine(_pid);

    Elf64Ehdr ehdr{};
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
    ehdr.e_version = static_cast<std::uint32_t>(EV_CURRENT);
    ehdr.e_entry = regions.front().start;
    ehdr.e_phoff = sizeof(Elf64Ehdr);
    ehdr.e_ehsize = static_cast<std::uint16_t>(sizeof(Elf64Ehdr));
    ehdr.e_phentsize = static_cast<std::uint16_t>(sizeof(Elf64Phdr));
    if (nsegs >= 0xffff)
        throw std::runtime_error("too many segments for ELF phnum field");
    ehdr.e_phnum = static_cast<std::uint16_t>(nsegs);
    return ehdr;
}

std::vector<Elf64Phdr> Dumper::BuildPhdrs(const std::vector<SegmentLayout>& layouts, std::uint64_t noteOffset, std::uint64_t noteSize) const {
    const auto& regions = _map.Regions();
    if (regions.empty())
        throw std::runtime_error("no mapped regions to build program headers from");

    std::vector<Elf64Phdr> phdrs;
    phdrs.reserve(regions.size() + 1);

    Elf64Phdr notePh{};
    std::memset(&notePh, 0, sizeof(notePh));
    notePh.p_type = PT_NOTE;
    notePh.p_offset = noteOffset;
    notePh.p_filesz = noteSize;
    notePh.p_align = 4;
    phdrs.push_back(notePh);

    for (std::size_t i = 0; i < regions.size(); ++i) {
        Elf64Phdr ph{};
        std::memset(&ph, 0, sizeof(ph));
        ph.p_type = PT_LOAD;
        ph.p_flags = regions[i].flags;
        ph.p_offset = layouts[i].fileOffset;
        ph.p_vaddr = regions[i].start;
        ph.p_paddr = regions[i].start;
        ph.p_filesz = layouts[i].filesz;
        ph.p_memsz = layouts[i].filesz;
        ph.p_align = 0x1000;
        phdrs.push_back(ph);
    }
    return phdrs;
}

void Dumper::PrintSummary(std::uint64_t actualFileSize, const ReadStats& stats,
                           const std::string& outPath) const {
    const auto& regions = _map.Regions();
    std::uint16_t emachine = ProcessMap::EMachine(_pid);

    std::fprintf(stderr, "\n\n");
    std::fprintf(stderr, "regions   : %zu\n", regions.size());
    std::fprintf(stderr, "e_machine : 0x%x\n", static_cast<unsigned>(emachine));
    std::fprintf(stderr, "base      : 0x%" PRIx64 "\n", regions.front().start);
    std::fprintf(stderr, "top       : 0x%" PRIx64 "\n", regions.back().end);
    std::fprintf(stderr, "file size : %" PRIu64 " MB\n", actualFileSize / (1024 * 1024));
    std::fprintf(stderr, "read      : %" PRIu64 " MB\n", stats.bytesRead / (1024 * 1024));
    std::fprintf(stderr, "zeroed    : %" PRIu64 " MB\n", stats.bytesZeroed / (1024 * 1024));
    std::fprintf(stderr, "output    : %s\n", outPath.c_str());
}

void Dumper::Run(const std::string& outPath) {
    const auto& regions = _map.Regions();

    std::vector<std::uint8_t> noteData = FileNoteBuilder::Build(regions);
    CheckSizeLimit(EstimateTotalSize(noteData));

    std::string memPath = "/proc/" + std::to_string(_pid) + "/mem";
    std::FILE* memF = std::fopen(memPath.c_str(), "rb");
    if (!memF) throw std::runtime_error("cannot open " + memPath);

    std::FILE* outF = std::fopen(outPath.c_str(), "wb");
    if (!outF) {
        std::fclose(memF);
        throw std::runtime_error("cannot open output: " + outPath);
    }

    std::size_t nsegs = regions.size() + 1;
    const std::uint64_t hdrsSize = sizeof(Elf64Ehdr) + sizeof(Elf64Phdr) * nsegs;
    FileIo::WriteZeroes(outF, _zeroBuf, READ_BUF_SIZE, hdrsSize);

    const std::uint64_t noteOffset = FileIo::Tell(outF);
    FileIo::Write(outF, noteData.data(), noteData.size());

    std::vector<SegmentLayout> layouts;
    layouts.reserve(regions.size());
    const ReadStats stats = WriteRegionData(memF, outF, layouts);

    const std::uint64_t actualFileSize = FileIo::Tell(outF);

    Elf64Ehdr ehdr = BuildEhdr(nsegs);
    std::vector<Elf64Phdr> phdrs = BuildPhdrs(layouts, noteOffset, noteData.size());

    FileIo::Seek(outF, 0);
    FileIo::Write(outF, &ehdr, sizeof(ehdr));
    FileIo::Write(outF, phdrs.data(), sizeof(Elf64Phdr) * nsegs);

    std::fclose(memF);
    std::fclose(outF);

    PrintSummary(actualFileSize, stats, outPath);
}

}

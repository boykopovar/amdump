#include <amdump/Dumper.hpp>
#include <amdump/ProcessMap.hpp>
#include <amdump/Elf64.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <cerrno>
#include <string>
#include <algorithm>
#include <vector>

namespace Amdump {

static const std::size_t BUF = 1 << 20;
static char _buf[BUF];
static char _zero[BUF];

Dumper::Dumper(int pid, const ProcessMap& map, std::uint64_t maxBytes)
    : _pid(pid), _map(map), _maxBytes(maxBytes) {}

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

static const std::uint64_t NOTE_PAGE_SIZE = 0x1000;

static void AppendPadded(std::vector<std::uint8_t>& buf, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buf.insert(buf.end(), bytes, bytes + size);
    while (buf.size() % 4 != 0)
        buf.push_back(0);
}

static std::vector<std::uint8_t> BuildFileNote(const std::vector<Region>& regions) {
    std::vector<const Region*> named;
    for (const auto& r : regions)
        if (!r.name.empty())
            named.push_back(&r);

    std::vector<std::uint8_t> desc;
    auto count = static_cast<std::int64_t>(named.size());
    auto pageSize = static_cast<std::int64_t>(NOTE_PAGE_SIZE);
    desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&count), reinterpret_cast<std::uint8_t*>(&count) + sizeof(count));
    desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&pageSize), reinterpret_cast<std::uint8_t*>(&pageSize) + sizeof(pageSize));

    for (const Region* r : named) {
        if (r->fileOffset % NOTE_PAGE_SIZE != 0)
            throw std::runtime_error("region file offset not page-aligned for " + r->name);
        auto start = static_cast<std::int64_t>(r->start);
        auto end = static_cast<std::int64_t>(r->end);
        auto fileOfsPages = static_cast<std::int64_t>(r->fileOffset / NOTE_PAGE_SIZE);
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&start), reinterpret_cast<std::uint8_t*>(&start) + sizeof(start));
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&end), reinterpret_cast<std::uint8_t*>(&end) + sizeof(end));
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&fileOfsPages), reinterpret_cast<std::uint8_t*>(&fileOfsPages) + sizeof(fileOfsPages));
    }
    for (const Region* r : named)
        desc.insert(desc.end(), r->name.begin(), r->name.end() + 1);

    static constexpr char noteName[] = "CORE";
    Elf64Nhdr nhdr{};
    nhdr.n_namesz = static_cast<std::uint32_t>(sizeof(noteName));
    nhdr.n_descsz = static_cast<std::uint32_t>(desc.size());
    nhdr.n_type = NT_FILE;

    std::vector<std::uint8_t> note;
    note.insert(note.end(), reinterpret_cast<std::uint8_t*>(&nhdr), reinterpret_cast<std::uint8_t*>(&nhdr) + sizeof(nhdr));
    AppendPadded(note, noteName, sizeof(noteName));
    AppendPadded(note, desc.data(), desc.size());
    return note;
}

void Dumper::Run(const std::string& outPath) {
    const auto& regions = _map.Regions();
    std::uint16_t emachine = ProcessMap::EMachine(_pid);

    std::vector<std::uint8_t> noteData = BuildFileNote(regions);

    std::uint64_t totalSize = sizeof(Elf64Ehdr) + sizeof(Elf64Phdr) * (regions.size() + 1) + noteData.size();
    for (const auto& r : regions)
        totalSize += r.end - r.start;

    std::fprintf(stderr, "estimated file size: %" PRIu64 " MB\n", totalSize / (1024 * 1024));

    if (totalSize > _maxBytes)
        throw std::runtime_error(
            "estimated dump size " + std::to_string(totalSize / (1024 * 1024)) +
            " MB exceeds limit " + std::to_string(_maxBytes / (1024 * 1024)) + " MB");

    std::string memPath = "/proc/" + std::to_string(_pid) + "/mem";
    std::FILE* memF = std::fopen(memPath.c_str(), "rb");
    if (!memF) throw std::runtime_error("cannot open " + memPath);

    std::FILE* outF = std::fopen(outPath.c_str(), "wb");
    if (!outF) {
        std::fclose(memF);
        throw std::runtime_error("cannot open output: " + outPath);
    }

    std::size_t nsegs = regions.size() + 1;
    std::uint64_t hdrsSize = sizeof(Elf64Ehdr) + sizeof(Elf64Phdr) * nsegs;

    WriteZeroes(outF, hdrsSize);

    std::uint64_t noteOffset = Ftell(outF);
    Write(outF, noteData.data(), noteData.size());

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

            errno = 0;
            std::size_t n = std::fread(_buf, 1, static_cast<std::size_t>(chunk), memF);
            if (n == 0) {
                const char* reason = std::feof(memF) ? "EOF" : std::strerror(errno);
                std::fprintf(stderr,
                    "\nwarning: unreadable page at 0x%" PRIx64 " size 0x%" PRIx64 " in %s, reason: %s, zero-filled\n",
                    vaddr, chunk, r.name.c_str(), reason);
                std::clearerr(memF);
                Write(outF, _zero, static_cast<std::size_t>(chunk));
                bytesZeroed += chunk;
                done += chunk;
                continue;
            }
            if (n < static_cast<std::size_t>(chunk)) {
                std::fprintf(stderr,
                    "\nwarning: short read at 0x%" PRIx64 ", requested 0x%" PRIx64 " got 0x%zx in %s\n",
                    vaddr, chunk, n, r.name.c_str());
            }
            Write(outF, _buf, n);
            bytesRead += static_cast<std::uint64_t>(n);
            done += static_cast<std::uint64_t>(n);
        }

        segInfos.push_back({fileOffset, rsize});
        std::fprintf(stderr, "\r[%d/%zu] 0x%" PRIx64 "-0x%" PRIx64,
            ++idx, regions.size(), r.start, r.end);
    }

    const std::uint64_t actualFileSize = Ftell(outF);

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

    std::vector<Elf64Phdr> phdrs(nsegs);
    Elf64Phdr& notePh = phdrs[0];
    std::memset(&notePh, 0, sizeof(notePh));
    notePh.p_type = PT_NOTE;
    notePh.p_offset = noteOffset;
    notePh.p_filesz = noteData.size();
    notePh.p_align = 4;

    for (std::size_t i = 0; i < regions.size(); ++i) {
        Elf64Phdr& ph = phdrs[i + 1];
        std::memset(&ph, 0, sizeof(ph));
        ph.p_type = PT_LOAD;
        ph.p_flags = regions[i].flags;
        ph.p_offset = segInfos[i].fileOffset;
        ph.p_vaddr = regions[i].start;
        ph.p_paddr = regions[i].start;
        ph.p_filesz = segInfos[i].filesz;
        ph.p_memsz = segInfos[i].filesz;
        ph.p_align = 0x1000;
    }

    Fseek(outF, 0);
    Write(outF, &ehdr, sizeof(ehdr));
    Write(outF, phdrs.data(), sizeof(Elf64Phdr) * nsegs);

    std::fclose(memF);
    std::fclose(outF);

    std::fprintf(stderr, "\n\n");
    std::fprintf(stderr, "regions   : %zu\n", regions.size());
    std::fprintf(stderr, "e_machine : 0x%x\n", static_cast<unsigned>(emachine));
    std::fprintf(stderr, "base      : 0x%" PRIx64 "\n", regions.front().start);
    std::fprintf(stderr, "top       : 0x%" PRIx64 "\n", regions.back().end);
    std::fprintf(stderr, "file size : %" PRIu64 " MB\n", actualFileSize / (1024 * 1024));
    std::fprintf(stderr, "read      : %" PRIu64 " MB\n", bytesRead / (1024 * 1024));
    std::fprintf(stderr, "zeroed    : %" PRIu64 " MB\n", bytesZeroed / (1024 * 1024));
    std::fprintf(stderr, "output    : %s\n", outPath.c_str());
}

}

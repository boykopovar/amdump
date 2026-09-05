#pragma once
#include <amdump/ProcessMap.hpp>
#include <amdump/Elf64.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

namespace Amdump {

class Dumper {
public:
    Dumper(int pid, const ProcessMap& map, std::uint64_t maxBytes);
    void Run(const std::string& outPath);

private:
    struct SegmentLayout {
        std::uint64_t fileOffset;
        std::uint64_t filesz;
    };

    struct ReadStats {
        std::uint64_t bytesRead = 0;
        std::uint64_t bytesZeroed = 0;
    };

    std::uint64_t EstimateTotalSize(const std::vector<std::uint8_t>& noteData) const;
    void CheckSizeLimit(std::uint64_t totalSize) const;

    ReadStats WriteRegionData(std::FILE* memF, std::FILE* outF, std::vector<SegmentLayout>& outLayouts);
    void CopyRegionBytes(std::FILE* memF, std::FILE* outF, const Region& r, ReadStats& stats);

    Elf64Ehdr BuildEhdr(std::size_t nsegs) const;
    std::vector<Elf64Phdr> BuildPhdrs(const std::vector<SegmentLayout>& layouts,
                                       std::uint64_t noteOffset,
                                       std::uint64_t noteSize) const;

    void PrintSummary(std::uint64_t actualFileSize, const ReadStats& stats,
                       const std::string& outPath) const;

    int _pid;
    const ProcessMap& _map;
    std::uint64_t _maxBytes;
};

}

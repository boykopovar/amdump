#include <amdump/Dumper.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <string>
#include <algorithm>

namespace Amdump {

static const std::size_t BUF = 1 << 20;

static void SeekWrite(std::FILE* f, std::uint64_t offset, const void* data, std::size_t size) {
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        throw std::runtime_error("fseek out failed");
    if (std::fwrite(data, 1, size, f) != size)
        throw std::runtime_error("fwrite failed");
}

static std::size_t SeekRead(std::FILE* f, std::uint64_t offset, void* data, std::size_t size) {
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        return 0;
    return std::fread(data, 1, size, f);
}

Dumper::Dumper(int pid, const ProcessMap& map) : _pid(pid), _map(map) {}

void Dumper::Run(const std::string& outPath) {
    std::string memPath = "/proc/" + std::to_string(_pid) + "/mem";
    std::FILE* memF = std::fopen(memPath.c_str(), "rb");
    if (!memF) throw std::runtime_error("cannot open " + memPath);

    std::FILE* outF = std::fopen(outPath.c_str(), "wb");
    if (!outF) { std::fclose(memF); throw std::runtime_error("cannot open output: " + outPath); }

    static char buf[BUF];
    static char zero[BUF];
    std::memset(zero, 0, BUF);

    std::uint64_t base = _map.Base();
    std::uint64_t cur = base;
    std::uint64_t bytesRead = 0;
    std::uint64_t bytesZeroed = 0;
    int idx = 0;

    for (const auto& r : _map.Regions()) {
        if (r.start > cur) {
            std::uint64_t gap = r.start - cur;
            std::uint64_t written = 0;
            while (written < gap) {
                std::uint64_t chunk = std::min(gap - written, static_cast<std::uint64_t>(BUF));
                SeekWrite(outF, cur + written - base, zero, static_cast<std::size_t>(chunk));
                written += chunk;
            }
            bytesZeroed += gap;
            cur = r.start;
        }
        std::uint64_t rsize = r.end - r.start;
        std::uint64_t done = 0;
        while (done < rsize) {
            std::uint64_t chunk = std::min(rsize - done, static_cast<std::uint64_t>(BUF));
            std::uint64_t vaddr = r.start + done;
            std::size_t n = SeekRead(memF, vaddr, buf, static_cast<std::size_t>(chunk));
            if (n == 0) {
                SeekWrite(outF, vaddr - base, zero, static_cast<std::size_t>(chunk));
                bytesZeroed += chunk;
                done += chunk;
                continue;
            }
            SeekWrite(outF, vaddr - base, buf, n);
            bytesRead += static_cast<std::uint64_t>(n);
            done += static_cast<std::uint64_t>(n);
        }
        std::fprintf(stderr, "\r[%d/%zu] 0x%" PRIx64 "-0x%" PRIx64,
            ++idx, _map.Regions().size(), r.start, r.end);
        cur = r.end;
    }

    std::uint64_t total = _map.Top() - base;
    std::fprintf(stderr, "\n\n");
    std::fprintf(stderr, "regions   : %zu\n", _map.Regions().size());
    std::fprintf(stderr, "base      : 0x%" PRIx64 "\n", base);
    std::fprintf(stderr, "top       : 0x%" PRIx64 "\n", _map.Top());
    std::fprintf(stderr, "dump size : %" PRIu64 " MB (%" PRIu64 " bytes)\n", total / (1024 * 1024), total);
    std::fprintf(stderr, "read      : %" PRIu64 " MB (%" PRIu64 " bytes)\n", bytesRead / (1024 * 1024), bytesRead);
    std::fprintf(stderr, "zeroed    : %" PRIu64 " MB (%" PRIu64 " bytes)\n", bytesZeroed / (1024 * 1024), bytesZeroed);
    std::fprintf(stderr, "output    : %s\n", outPath.c_str());

    std::fclose(memF);
    std::fclose(outF);
}

}

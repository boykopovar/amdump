#include <amdump/Dumper.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>

namespace Amdump {

static const std::size_t BUF = 1 << 20;

Dumper::Dumper(int pid, const ProcessMap& map) : _pid(pid), _map(map) {}

void Dumper::Run(const std::string& outPath) {
    std::string memPath = "/proc/" + std::to_string(_pid) + "/mem";
    int memFd = open(memPath.c_str(), O_RDONLY);
    if (memFd < 0) throw std::runtime_error("cannot open " + memPath);

    int outFd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outFd < 0) { close(memFd); throw std::runtime_error("cannot open output: " + outPath); }

    std::uint64_t base = _map.Base();
    std::uint64_t total = _map.Top() - base;

    if (ftruncate(outFd, static_cast<off_t>(total)) < 0)
        throw std::runtime_error("ftruncate failed");

    static char buf[BUF];
    static char zero[BUF];
    std::memset(zero, 0, BUF);

    std::uint64_t cur = base;
    int idx = 0;
    for (const auto& r : _map.Regions()) {
        if (r.start > cur) {
            std::uint64_t gap = r.start - cur;
            std::uint64_t written = 0;
            while (written < gap) {
                std::uint64_t chunk = std::min(gap - written, (std::uint64_t)BUF);
                if (pwrite(outFd, zero, chunk, static_cast<off_t>(cur + written - base)) < 0)
                    throw std::runtime_error("pwrite gap failed");
                written += chunk;
            }
            cur = r.start;
        }
        std::uint64_t rsize = r.end - r.start;
        std::uint64_t done = 0;
        while (done < rsize) {
            std::uint64_t chunk = std::min(rsize - done, (std::uint64_t)BUF);
            std::uint64_t vaddr = r.start + done;
            ssize_t n = pread(memFd, buf, chunk, static_cast<off_t>(vaddr));
            if (n <= 0) {
                if (pwrite(outFd, zero, chunk, static_cast<off_t>(vaddr - base)) < 0)
                    throw std::runtime_error("pwrite zero failed");
                done += chunk;
                continue;
            }
            if (pwrite(outFd, buf, static_cast<std::size_t>(n), static_cast<off_t>(vaddr - base)) < 0)
                throw std::runtime_error("pwrite data failed");
            done += static_cast<std::uint64_t>(n);
        }
        std::fprintf(stderr, "\r[%d/%zu] 0x%lx-0x%lx",
            ++idx, _map.Regions().size(), r.start, r.end);
        cur = r.end;
    }
    std::fprintf(stderr, "\ndone\n");
    close(memFd);
    close(outFd);
}

}

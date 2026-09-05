#include <amdump/FileNoteBuilder.hpp>
#include <amdump/Elf64.hpp>
#include <stdexcept>

namespace Amdump {

void FileNoteBuilder::AppendPadded(std::vector<std::uint8_t>& buf, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    buf.insert(buf.end(), bytes, bytes + size);
    while (buf.size() % 4 != 0)
        buf.push_back(0);
}

std::vector<std::uint8_t> FileNoteBuilder::BuildDescriptor(const std::vector<const Region*>& named) {
    std::vector<std::uint8_t> desc;
    auto count = static_cast<std::int64_t>(named.size());
    auto pageSize = static_cast<std::int64_t>(PAGE_SIZE);
    desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&count), reinterpret_cast<std::uint8_t*>(&count) + sizeof(count));
    desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&pageSize), reinterpret_cast<std::uint8_t*>(&pageSize) + sizeof(pageSize));

    for (const Region* r : named) {
        if (r->fileOffset % PAGE_SIZE != 0)
            throw std::runtime_error("region file offset not page-aligned for " + r->name);
        auto start = static_cast<std::int64_t>(r->start);
        auto end = static_cast<std::int64_t>(r->end);
        auto fileOfsPages = static_cast<std::int64_t>(r->fileOffset / PAGE_SIZE);
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&start), reinterpret_cast<std::uint8_t*>(&start) + sizeof(start));
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&end), reinterpret_cast<std::uint8_t*>(&end) + sizeof(end));
        desc.insert(desc.end(), reinterpret_cast<std::uint8_t*>(&fileOfsPages), reinterpret_cast<std::uint8_t*>(&fileOfsPages) + sizeof(fileOfsPages));
    }
    for (const Region* r : named)
        desc.insert(desc.end(), r->name.begin(), r->name.end() + 1);

    return desc;
}

std::vector<std::uint8_t> FileNoteBuilder::Build(const std::vector<Region>& regions) {
    std::vector<const Region*> named;
    for (const auto& r : regions)
        if (!r.name.empty())
            named.push_back(&r);

    const std::vector<std::uint8_t> desc = BuildDescriptor(named);

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

}

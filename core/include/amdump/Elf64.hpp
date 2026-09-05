#pragma once
#include <cstdint>

namespace Amdump {

static constexpr std::uint8_t ELFMAG0 = 0x7f;
static constexpr std::uint8_t ELFMAG1 = 'E';
static constexpr std::uint8_t ELFMAG2 = 'L';
static constexpr std::uint8_t ELFMAG3 = 'F';
static constexpr std::uint8_t ELFCLASS64 = 2;
static constexpr std::uint8_t ELFDATA2LSB = 1;
static constexpr std::uint8_t EV_CURRENT = 1;
static constexpr std::uint8_t ELFOSABI_LINUX = 3;

static constexpr std::size_t EI_MAG0 = 0;
static constexpr std::size_t EI_MAG1 = 1;
static constexpr std::size_t EI_MAG2 = 2;
static constexpr std::size_t EI_MAG3 = 3;
static constexpr std::size_t EI_CLASS = 4;
static constexpr std::size_t EI_DATA = 5;
static constexpr std::size_t EI_VERSION = 6;
static constexpr std::size_t EI_OSABI = 7;
static constexpr std::size_t EI_NIDENT = 16;

static constexpr std::uint16_t ET_CORE = 4;

static constexpr std::uint32_t PT_LOAD = 1;
static constexpr std::uint32_t PT_NOTE = 4;

static constexpr std::uint32_t PF_X = 0x1;
static constexpr std::uint32_t PF_W = 0x2;
static constexpr std::uint32_t PF_R = 0x4;

static constexpr std::uint32_t NT_FILE = 0x46494c45;

struct Elf64Ehdr {
    std::uint8_t e_ident[EI_NIDENT];
    std::uint16_t e_type;
    std::uint16_t e_machine;
    std::uint32_t e_version;
    std::uint64_t e_entry;
    std::uint64_t e_phoff;
    std::uint64_t e_shoff;
    std::uint32_t e_flags;
    std::uint16_t e_ehsize;
    std::uint16_t e_phentsize;
    std::uint16_t e_phnum;
    std::uint16_t e_shentsize;
    std::uint16_t e_shnum;
    std::uint16_t e_shstrndx;
};

struct Elf64Phdr {
    std::uint32_t p_type;
    std::uint32_t p_flags;
    std::uint64_t p_offset;
    std::uint64_t p_vaddr;
    std::uint64_t p_paddr;
    std::uint64_t p_filesz;
    std::uint64_t p_memsz;
    std::uint64_t p_align;
};

struct Elf64Nhdr {
    std::uint32_t n_namesz;
    std::uint32_t n_descsz;
    std::uint32_t n_type;
};

static_assert(sizeof(Elf64Ehdr) == 64, "Elf64Ehdr size mismatch");
static_assert(sizeof(Elf64Phdr) == 56, "Elf64Phdr size mismatch");
static_assert(sizeof(Elf64Nhdr) == 12, "Elf64Nhdr size mismatch");

}

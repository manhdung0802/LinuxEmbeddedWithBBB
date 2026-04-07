/*
 * elf_loader.h - ELF32 ARM binary loader
 */
#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct cpu_emu cpu_emu_t;

/* ELF32 types */
#define EI_NIDENT   16
#define ET_EXEC     2
#define ET_REL      1
#define EM_ARM      40
#define PT_LOAD     1
#define SHT_SYMTAB  2
#define SHT_STRTAB  3
#define SHT_REL     9
#define SHT_RELA    4
#define SHT_PROGBITS 1
#define SHT_NOBITS  8

/* ELF32 header */
typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

/* Program header */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

/* Section header */
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;

/* Symbol table entry */
typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
} Elf32_Sym;

/* Relocation entry */
typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
} Elf32_Rel;

typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t  r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(i)   ((i) >> 8)
#define ELF32_R_TYPE(i)   ((i) & 0xFF)
#define ELF32_ST_BIND(i)  ((i) >> 4)
#define ELF32_ST_TYPE(i)  ((i) & 0xF)

/* ARM relocation types */
#define R_ARM_ABS32        2
#define R_ARM_CALL         28
#define R_ARM_JUMP24       29
#define R_ARM_MOVW_ABS_NC  43
#define R_ARM_MOVT_ABS     44
#define R_ARM_THM_CALL     10
#define R_ARM_V4BX         40

/* Loaded ELF info */
typedef struct {
    uint32_t entry_point;
    uint32_t load_base;     /* Lowest loaded address */
    uint32_t load_end;      /* Highest loaded address + size */
    bool is_relocatable;    /* ET_REL vs ET_EXEC */

    /* Symbol table (for .ko support) */
    Elf32_Sym *symtab;
    int num_symbols;
    char *strtab;
    uint32_t strtab_size;

    /* Section info (for .ko support) */
    Elf32_Shdr *sections;
    int num_sections;
    char *shstrtab;
    uint32_t *section_addrs;  /* Resolved load addresses per section */
} elf_info_t;

/* Load a statically linked ELF executable */
int elf_load_exec(cpu_emu_t *cpu, const char *path, elf_info_t *info);

/* Load a relocatable ELF (.ko) */
int elf_load_relocatable(cpu_emu_t *cpu, const char *path, elf_info_t *info,
                         uint32_t load_addr);

/* Find symbol by name in loaded ELF */
uint32_t elf_find_symbol(elf_info_t *info, const char *name);

/* Free ELF info resources */
void elf_info_free(elf_info_t *info);

#endif /* ELF_LOADER_H */

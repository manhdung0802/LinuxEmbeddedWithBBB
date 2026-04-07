/*
 * elf_loader.c - ELF32 ARM binary loader
 */
#include "elf_loader.h"
#include "cpu_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ELF magic: 0x7F 'E' 'L' 'F' */
static int validate_elf(const Elf32_Ehdr *ehdr)
{
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        fprintf(stderr, "[ELF] Invalid ELF magic\n");
        return -1;
    }

    if (ehdr->e_ident[4] != 1) { /* ELFCLASS32 */
        fprintf(stderr, "[ELF] Not a 32-bit ELF\n");
        return -1;
    }

    if (ehdr->e_ident[5] != 1) { /* ELFDATA2LSB (little-endian) */
        fprintf(stderr, "[ELF] Not little-endian ELF\n");
        return -1;
    }

    if (ehdr->e_machine != EM_ARM) {
        fprintf(stderr, "[ELF] Not an ARM ELF (e_machine=0x%X)\n", ehdr->e_machine);
        return -1;
    }

    return 0;
}

int elf_load_exec(cpu_emu_t *cpu, const char *path, elf_info_t *info)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("[ELF] Cannot open file");
        return -1;
    }

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *data = malloc(fsize);
    if (!data) {
        fclose(fp);
        return -1;
    }
    if (fread(data, 1, fsize, fp) != (size_t)fsize) {
        fprintf(stderr, "[ELF] Failed to read file\n");
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)data;
    if (validate_elf(ehdr) != 0) {
        free(data);
        return -1;
    }

    if (ehdr->e_type != ET_EXEC) {
        fprintf(stderr, "[ELF] Not an executable ELF (type=%d). Use static linking.\n",
                ehdr->e_type);
        free(data);
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->entry_point = ehdr->e_entry;
    info->is_relocatable = false;
    info->load_base = 0xFFFFFFFF;
    info->load_end = 0;

    /* Load PT_LOAD segments */
    Elf32_Phdr *phdrs = (Elf32_Phdr *)(data + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0) continue;

        printf("[ELF] Loading segment: vaddr=0x%08X filesz=0x%X memsz=0x%X\n",
               ph->p_vaddr, ph->p_filesz, ph->p_memsz);

        /* Track load range */
        if (ph->p_vaddr < info->load_base)
            info->load_base = ph->p_vaddr;
        if (ph->p_vaddr + ph->p_memsz > info->load_end)
            info->load_end = ph->p_vaddr + ph->p_memsz;

        /* Write file data to Unicorn memory */
        if (ph->p_filesz > 0) {
            int ret = cpu_emu_write_mem(cpu, ph->p_vaddr,
                                        data + ph->p_offset, ph->p_filesz);
            if (ret != 0) {
                fprintf(stderr, "[ELF] Failed to write segment at 0x%08X\n",
                        ph->p_vaddr);
                free(data);
                return -1;
            }
        }

        /* Zero BSS portion (memsz > filesz) */
        if (ph->p_memsz > ph->p_filesz) {
            size_t bss_size = ph->p_memsz - ph->p_filesz;
            uint8_t *zeros = calloc(1, bss_size);
            if (zeros) {
                cpu_emu_write_mem(cpu, ph->p_vaddr + ph->p_filesz, zeros, bss_size);
                free(zeros);
            }
        }
    }

    /* Setup initial stack with argc/argv */
    uint32_t sp = 0x8F000000;
    uint32_t argc = 1;
    /* Push argv[0] string "firmware" */
    sp -= 16;
    uint32_t argv0_addr = sp;
    cpu_emu_write_mem(cpu, sp, "firmware\0\0\0\0\0\0\0\0", 16);
    /* Push argv pointer */
    sp -= 4;
    cpu_emu_write_mem(cpu, sp, &argv0_addr, 4);
    uint32_t argv_addr = sp;
    /* Push NULL (end of argv) */
    sp -= 4;
    uint32_t null_val = 0;
    cpu_emu_write_mem(cpu, sp, &null_val, 4);
    /* Push argv pointer and argc */
    sp -= 4;
    cpu_emu_write_mem(cpu, sp, &argv_addr, 4);
    sp -= 4;
    cpu_emu_write_mem(cpu, sp, &argc, 4);

    /* Set registers */
    cpu_emu_set_reg(cpu, UC_ARM_REG_SP, sp);
    cpu_emu_set_reg(cpu, UC_ARM_REG_R0, argc);
    cpu_emu_set_reg(cpu, UC_ARM_REG_R1, argv_addr);

    printf("[ELF] Loaded executable: entry=0x%08X, load range=0x%08X-0x%08X\n",
           info->entry_point, info->load_base, info->load_end);

    free(data);
    return 0;
}

int elf_load_relocatable(cpu_emu_t *cpu, const char *path, elf_info_t *info,
                         uint32_t load_addr)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("[ELF] Cannot open .ko file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *data = malloc(fsize);
    if (!data) { fclose(fp); return -1; }
    if (fread(data, 1, fsize, fp) != (size_t)fsize) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)data;
    if (validate_elf(ehdr) != 0) { free(data); return -1; }
    if (ehdr->e_type != ET_REL) {
        fprintf(stderr, "[ELF] Not a relocatable ELF (.ko)\n");
        free(data);
        return -1;
    }

    memset(info, 0, sizeof(*info));
    info->is_relocatable = true;
    info->load_base = load_addr;
    info->num_sections = ehdr->e_shnum;

    /* Parse section headers */
    Elf32_Shdr *shdrs = (Elf32_Shdr *)(data + ehdr->e_shoff);

    /* Get section name string table */
    if (ehdr->e_shstrndx < ehdr->e_shnum) {
        Elf32_Shdr *shstrtab_sh = &shdrs[ehdr->e_shstrndx];
        info->shstrtab = malloc(shstrtab_sh->sh_size);
        memcpy(info->shstrtab, data + shstrtab_sh->sh_offset, shstrtab_sh->sh_size);
    }

    /* Allocate section address array */
    info->section_addrs = calloc(ehdr->e_shnum, sizeof(uint32_t));

    /* Load allocatable sections into Unicorn memory */
    uint32_t cur_addr = load_addr;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *sh = &shdrs[i];
        if (!(sh->sh_flags & 0x2 /* SHF_ALLOC */)) {
            info->section_addrs[i] = 0;
            continue;
        }

        /* Align */
        if (sh->sh_addralign > 1) {
            cur_addr = (cur_addr + sh->sh_addralign - 1) & ~(sh->sh_addralign - 1);
        }

        info->section_addrs[i] = cur_addr;

        if (sh->sh_type == SHT_PROGBITS && sh->sh_size > 0) {
            cpu_emu_write_mem(cpu, cur_addr, data + sh->sh_offset, sh->sh_size);
        } else if (sh->sh_type == SHT_NOBITS && sh->sh_size > 0) {
            /* BSS - already zero in Unicorn */
        }

        const char *sec_name = info->shstrtab ? info->shstrtab + sh->sh_name : "?";
        printf("[KO] Section '%s' loaded at 0x%08X (size=0x%X)\n",
               sec_name, cur_addr, sh->sh_size);

        cur_addr += sh->sh_size;
    }
    info->load_end = cur_addr;

    /* Find symbol table and string table */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *sh = &shdrs[i];
        if (sh->sh_type == SHT_SYMTAB) {
            info->num_symbols = sh->sh_size / sizeof(Elf32_Sym);
            info->symtab = malloc(sh->sh_size);
            memcpy(info->symtab, data + sh->sh_offset, sh->sh_size);

            /* Get associated string table */
            if (sh->sh_link < ehdr->e_shnum) {
                Elf32_Shdr *strtab_sh = &shdrs[sh->sh_link];
                info->strtab_size = strtab_sh->sh_size;
                info->strtab = malloc(strtab_sh->sh_size);
                memcpy(info->strtab, data + strtab_sh->sh_offset, strtab_sh->sh_size);
            }
            break;
        }
    }

    /* Resolve symbol addresses */
    if (info->symtab) {
        for (int i = 0; i < info->num_symbols; i++) {
            Elf32_Sym *sym = &info->symtab[i];
            if (sym->st_shndx > 0 && sym->st_shndx < ehdr->e_shnum) {
                uint32_t sec_addr = info->section_addrs[sym->st_shndx];
                if (sec_addr) {
                    sym->st_value += sec_addr;
                }
            }
        }
    }

    /* Apply relocations */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf32_Shdr *sh = &shdrs[i];
        if (sh->sh_type != SHT_REL) continue;

        int target_sec = sh->sh_info;
        if (target_sec == 0 || target_sec >= ehdr->e_shnum) continue;
        uint32_t target_base = info->section_addrs[target_sec];
        if (!target_base) continue;

        int num_rels = sh->sh_size / sizeof(Elf32_Rel);
        Elf32_Rel *rels = (Elf32_Rel *)(data + sh->sh_offset);

        for (int r = 0; r < num_rels; r++) {
            Elf32_Rel *rel = &rels[r];
            int sym_idx = ELF32_R_SYM(rel->r_info);
            int type = ELF32_R_TYPE(rel->r_info);
            uint32_t rel_addr = target_base + rel->r_offset;

            uint32_t sym_val = 0;
            if (sym_idx < info->num_symbols) {
                sym_val = info->symtab[sym_idx].st_value;
            }

            /* Read current value at relocation address */
            uint32_t cur_val = 0;
            cpu_emu_read_mem(cpu, rel_addr, &cur_val, 4);

            uint32_t new_val = cur_val;
            switch (type) {
                case R_ARM_ABS32:
                    new_val = sym_val + cur_val;
                    break;
                case R_ARM_CALL:
                case R_ARM_JUMP24: {
                    int32_t offset = (int32_t)((cur_val & 0x00FFFFFF) << 2);
                    if (offset & 0x02000000) offset |= 0xFC000000; /* sign extend */
                    offset += (int32_t)(sym_val - rel_addr);
                    new_val = (cur_val & 0xFF000000) | ((offset >> 2) & 0x00FFFFFF);
                    break;
                }
                case R_ARM_MOVW_ABS_NC: {
                    uint32_t imm = sym_val & 0xFFFF;
                    new_val = (cur_val & 0xFFF0F000) | ((imm & 0xF000) << 4) | (imm & 0xFFF);
                    break;
                }
                case R_ARM_MOVT_ABS: {
                    uint32_t imm = (sym_val >> 16) & 0xFFFF;
                    new_val = (cur_val & 0xFFF0F000) | ((imm & 0xF000) << 4) | (imm & 0xFFF);
                    break;
                }
                case R_ARM_V4BX:
                    /* No-op for this relocation */
                    continue;
                default:
                    fprintf(stderr, "[KO] Unsupported relocation type %d at 0x%08X\n",
                            type, rel_addr);
                    continue;
            }

            cpu_emu_write_mem(cpu, rel_addr, &new_val, 4);
        }
    }

    printf("[KO] Loaded relocatable: base=0x%08X, end=0x%08X, %d symbols\n",
           info->load_base, info->load_end, info->num_symbols);

    free(data);
    return 0;
}

uint32_t elf_find_symbol(elf_info_t *info, const char *name)
{
    if (!info->symtab || !info->strtab) return 0;

    for (int i = 0; i < info->num_symbols; i++) {
        Elf32_Sym *sym = &info->symtab[i];
        if (sym->st_name < info->strtab_size) {
            if (strcmp(info->strtab + sym->st_name, name) == 0) {
                return sym->st_value;
            }
        }
    }
    return 0;
}

void elf_info_free(elf_info_t *info)
{
    if (info->symtab) free(info->symtab);
    if (info->strtab) free(info->strtab);
    if (info->shstrtab) free(info->shstrtab);
    if (info->section_addrs) free(info->section_addrs);
    if (info->sections) free(info->sections);
    memset(info, 0, sizeof(*info));
}

/*
 * ko_loader.c - Kernel module (.ko) loader with mock kernel API
 *
 * Provides mock implementations of key kernel functions (printk, kmalloc,
 * ioremap, etc.) as ARM stubs in Unicorn memory. When a .ko is loaded,
 * its undefined symbols are resolved against these stubs.
 *
 * The init_module() function is called to initialize the module, and
 * cleanup_module() can be called to unload it.
 */
#include "ko_loader.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include "elf_loader.h"
#include "mem_subsys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ARM instruction: SVC #0xFF (used as breakpoint/trap for mock functions) */
#define ARM_SVC_MOCK    0xEF0000FF
/* ARM instruction: BX LR (return from function) */
#define ARM_BX_LR       0xE12FFF1E

/* Mock syscall numbers (intercepted via SVC #0xFF) */
#define MOCK_PRINTK         1
#define MOCK_KMALLOC        2
#define MOCK_KFREE          3
#define MOCK_IOREMAP        4
#define MOCK_IOUNMAP        5
#define MOCK_REQUEST_MEM    6
#define MOCK_RELEASE_MEM    7
#define MOCK_READL          8
#define MOCK_WRITEL         9
#define MOCK_MSLEEP         10
#define MOCK_UDELAY         11
#define MOCK_GPIO_REQUEST   12
#define MOCK_GPIO_FREE      13
#define MOCK_GPIO_DIR_OUT   14
#define MOCK_GPIO_DIR_IN    15
#define MOCK_GPIO_SET       16
#define MOCK_GPIO_GET       17

/* Max mock symbols */
#define MAX_MOCK_SYMBOLS    64

/* Static storage for mock symbols */
static mock_symbol_t mock_symbols[MAX_MOCK_SYMBOLS];
static int num_mock_symbols = 0;

/* KO state (only one loaded at a time) */
static elf_info_t ko_elf_info;
static bool ko_loaded = false;
static uint32_t ko_init_addr = 0;
static uint32_t ko_cleanup_addr = 0;

/* Heap for kmalloc */
static uint32_t kmalloc_next = 0x84000000;
#define KMALLOC_REGION_BASE  0x84000000
#define KMALLOC_REGION_SIZE  (16 * 1024 * 1024)

/* Register a mock symbol at the given address */
static void register_mock(const char *name, uint32_t addr)
{
    if (num_mock_symbols < MAX_MOCK_SYMBOLS) {
        mock_symbols[num_mock_symbols].name = name;
        mock_symbols[num_mock_symbols].addr = addr;
        num_mock_symbols++;
    }
}

/* Write a stub function: SVC #0xFF; BX LR with mock_id in R12 */
static uint32_t write_stub(cpu_emu_t *cpu, uint32_t addr, int mock_id)
{
    /* MOV R12, #mock_id */
    uint32_t mov_r12 = 0xE3A0C000 | (mock_id & 0xFF);
    /* SVC #0xFF */
    uint32_t svc = ARM_SVC_MOCK;
    /* BX LR */
    uint32_t bx_lr = ARM_BX_LR;

    cpu_emu_write_mem(cpu, addr, &mov_r12, 4);
    cpu_emu_write_mem(cpu, addr + 4, &svc, 4);
    cpu_emu_write_mem(cpu, addr + 8, &bx_lr, 4);

    return addr + 12;  /* Next available address */
}

int mock_kernel_init(bbb_sim_t *sim)
{
    cpu_emu_t *cpu = sim->cpu;

    /* Map mock kernel region */
    int err = cpu_emu_map_memory(cpu, MOCK_KERNEL_BASE,
                                  MOCK_KERNEL_MAX - MOCK_KERNEL_BASE,
                                  7 /* RWX */);
    if (err) {
        fprintf(stderr, "[KO] Failed to map mock kernel region\n");
        return -1;
    }

    /* Map kmalloc region */
    err = cpu_emu_map_memory(cpu, KMALLOC_REGION_BASE, KMALLOC_REGION_SIZE,
                              7 /* RWX */);
    if (err) {
        fprintf(stderr, "[KO] Failed to map kmalloc region\n");
        return -1;
    }

    num_mock_symbols = 0;
    uint32_t addr = MOCK_KERNEL_BASE;

    /* Create stubs for common kernel functions */
    struct { const char *name; int id; } stubs[] = {
        { "printk",                MOCK_PRINTK },
        { "__kmalloc",             MOCK_KMALLOC },
        { "kmalloc",              MOCK_KMALLOC },
        { "kfree",                MOCK_KFREE },
        { "ioremap",              MOCK_IOREMAP },
        { "__ioremap",            MOCK_IOREMAP },
        { "iounmap",              MOCK_IOUNMAP },
        { "__request_region",     MOCK_REQUEST_MEM },
        { "__release_region",     MOCK_RELEASE_MEM },
        { "readl",                MOCK_READL },
        { "writel",               MOCK_WRITEL },
        { "__raw_readl",          MOCK_READL },
        { "__raw_writel",         MOCK_WRITEL },
        { "msleep",               MOCK_MSLEEP },
        { "__udelay",             MOCK_UDELAY },
        { "udelay",               MOCK_UDELAY },
        { "gpio_request",         MOCK_GPIO_REQUEST },
        { "gpio_free",            MOCK_GPIO_FREE },
        { "gpio_direction_output", MOCK_GPIO_DIR_OUT },
        { "gpio_direction_input",  MOCK_GPIO_DIR_IN },
        { "gpio_set_value",        MOCK_GPIO_SET },
        { "gpio_get_value",        MOCK_GPIO_GET },
        { NULL, 0 }
    };

    for (int i = 0; stubs[i].name != NULL; i++) {
        register_mock(stubs[i].name, addr);
        addr = write_stub(cpu, addr, stubs[i].id);
    }

    /* __this_module: just a zero-filled area */
    register_mock("__this_module", addr);
    uint32_t zero = 0;
    for (int i = 0; i < 64; i++)
        cpu_emu_write_mem(cpu, addr + i * 4, &zero, 4);
    addr += 256;

    /* jiffies: a simple counter */
    register_mock("jiffies", addr);
    uint32_t jiffies_val = 0x10000;
    cpu_emu_write_mem(cpu, addr, &jiffies_val, 4);
    addr += 4;

    /* __aeabi_unwind_cpp_pr0: stub for ARM AEABI */
    register_mock("__aeabi_unwind_cpp_pr0", addr);
    addr = write_stub(cpu, addr, 0);

    /* __aeabi_unwind_cpp_pr1 */
    register_mock("__aeabi_unwind_cpp_pr1", addr);
    addr = write_stub(cpu, addr, 0);

    printf("[KO] Mock kernel initialized: %d symbols, 0x%08X-0x%08X\n",
           num_mock_symbols, MOCK_KERNEL_BASE, addr);

    return 0;
}

/* Handle mock SVC calls (called from cpu_emu interrupt handler) */
int ko_handle_mock_svc(bbb_sim_t *sim, uint32_t mock_id)
{
    cpu_emu_t *cpu = sim->cpu;

    switch (mock_id) {
    case MOCK_PRINTK: {
        /* R0 = format string address */
        uint32_t fmt_addr = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        char buf[256] = {0};
        cpu_emu_read_mem(cpu, fmt_addr, buf, sizeof(buf) - 1);
        /* Simple: just print the format string (no va_args in emulation) */
        printf("[KO:printk] %s", buf);
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, strlen(buf));
        break;
    }

    case MOCK_KMALLOC: {
        /* R0 = size, R1 = flags (ignored) */
        uint32_t size = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        /* Align to 16 bytes */
        size = (size + 15) & ~15;
        uint32_t ptr = kmalloc_next;
        kmalloc_next += size;
        printf("[KO:kmalloc] size=%u -> 0x%08X\n", size, ptr);
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, ptr);
        break;
    }

    case MOCK_KFREE:
        /* No-op (simplified) */
        printf("[KO:kfree] 0x%08X (no-op)\n",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R0));
        break;

    case MOCK_IOREMAP: {
        /* R0 = phys_addr, R1 = size -> return phys_addr (identity map) */
        uint32_t phys = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        uint32_t size = cpu_emu_get_reg(cpu, UC_ARM_REG_R1);
        printf("[KO:ioremap] 0x%08X size=%u (identity)\n", phys, size);
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, phys);
        break;
    }

    case MOCK_IOUNMAP:
        /* No-op */
        break;

    case MOCK_REQUEST_MEM:
    case MOCK_RELEASE_MEM:
        /* Always succeed */
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, 0);
        break;

    case MOCK_READL: {
        /* R0 = addr -> read from Unicorn memory (triggers MMIO hook) */
        uint32_t addr = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        uint32_t val = 0;
        cpu_emu_read_mem(cpu, addr, &val, 4);
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, val);
        break;
    }

    case MOCK_WRITEL: {
        /* R0 = value, R1 = addr */
        uint32_t val = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        uint32_t addr = cpu_emu_get_reg(cpu, UC_ARM_REG_R1);
        cpu_emu_write_mem(cpu, addr, &val, 4);
        break;
    }

    case MOCK_MSLEEP: {
        uint32_t ms = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        if (ms > 2000) ms = 2000;
        printf("[KO:msleep] %u ms\n", ms);
        /* Actually sleep on host (capped) */
        usleep(ms * 1000);
        break;
    }

    case MOCK_UDELAY: {
        uint32_t us = cpu_emu_get_reg(cpu, UC_ARM_REG_R0);
        if (us > 2000000) us = 2000000;
        usleep(us);
        break;
    }

    case MOCK_GPIO_REQUEST:
        printf("[KO:gpio_request] gpio=%u\n",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R0));
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, 0);
        break;

    case MOCK_GPIO_FREE:
        printf("[KO:gpio_free] gpio=%u\n",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R0));
        break;

    case MOCK_GPIO_DIR_OUT:
    case MOCK_GPIO_DIR_IN:
        printf("[KO:gpio_direction] gpio=%u dir=%s val=%u\n",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R0),
               mock_id == MOCK_GPIO_DIR_OUT ? "out" : "in",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R1));
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, 0);
        break;

    case MOCK_GPIO_SET:
        printf("[KO:gpio_set] gpio=%u value=%u\n",
               cpu_emu_get_reg(cpu, UC_ARM_REG_R0),
               cpu_emu_get_reg(cpu, UC_ARM_REG_R1));
        break;

    case MOCK_GPIO_GET:
        /* Return 0 (low) by default */
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, 0);
        break;

    default:
        /* Unknown mock - just return 0 */
        cpu_emu_set_reg(cpu, UC_ARM_REG_R0, 0);
        break;
    }

    return 0;
}

/* Look up a mock symbol by name */
static uint32_t find_mock_symbol(const char *name)
{
    for (int i = 0; i < num_mock_symbols; i++) {
        if (strcmp(mock_symbols[i].name, name) == 0)
            return mock_symbols[i].addr;
    }
    return 0;
}

int ko_load(bbb_sim_t *sim, const char *ko_path)
{
    if (ko_loaded) {
        fprintf(stderr, "[KO] A module is already loaded. Unload first.\n");
        return -1;
    }

    /* Ensure mock kernel is initialized */
    if (num_mock_symbols == 0) {
        if (mock_kernel_init(sim) != 0)
            return -1;
    }

    /* Map KO region if not already mapped */
    cpu_emu_map_memory(sim->cpu, KO_LOAD_BASE, KO_LOAD_MAX - KO_LOAD_BASE,
                       7 /* RWX */);

    /* Load the .ko as a relocatable ELF */
    memset(&ko_elf_info, 0, sizeof(ko_elf_info));
    int ret = elf_load_relocatable(sim->cpu, ko_path, &ko_elf_info, KO_LOAD_BASE);
    if (ret != 0) {
        fprintf(stderr, "[KO] Failed to load %s\n", ko_path);
        return -1;
    }

    /* Resolve external symbols against mock kernel */
    if (ko_elf_info.symtab && ko_elf_info.strtab) {
        for (int i = 0; i < ko_elf_info.num_symbols; i++) {
            Elf32_Sym *sym = &ko_elf_info.symtab[i];
            if (sym->st_shndx == 0 && sym->st_name != 0) {
                /* Undefined symbol - try to resolve */
                const char *name = ko_elf_info.strtab + sym->st_name;
                uint32_t addr = find_mock_symbol(name);
                if (addr != 0) {
                    sym->st_value = addr;
                    sym->st_shndx = 0xFFF1; /* SHN_ABS */
                    printf("[KO] Resolved: %s -> 0x%08X\n", name, addr);
                } else {
                    printf("[KO] WARNING: Unresolved symbol: %s\n", name);
                }
            }
        }
    }

    /* Find init_module / cleanup_module */
    ko_init_addr = elf_find_symbol(&ko_elf_info, "init_module");
    ko_cleanup_addr = elf_find_symbol(&ko_elf_info, "cleanup_module");

    if (ko_init_addr == 0) {
        fprintf(stderr, "[KO] init_module not found in %s\n", ko_path);
        elf_info_free(&ko_elf_info);
        return -1;
    }

    ko_loaded = true;
    sim->ko_loaded = true;

    printf("[KO] Loaded %s\n", ko_path);
    printf("[KO]   init_module   = 0x%08X\n", ko_init_addr);
    printf("[KO]   cleanup_module = 0x%08X\n", ko_cleanup_addr);

    /* Execute init_module */
    printf("[KO] Calling init_module()...\n");
    cpu_emu_set_reg(sim->cpu, UC_ARM_REG_PC, ko_init_addr);

    /* Set up return address to a halt stub */
    uint32_t halt_addr = MOCK_KERNEL_BASE + 0xFFF0;
    uint32_t halt_instr = 0xE7F000F0; /* UDF #0 - undefined instruction */
    cpu_emu_write_mem(sim->cpu, halt_addr, &halt_instr, 4);
    cpu_emu_set_reg(sim->cpu, UC_ARM_REG_LR, halt_addr);

    /* Set up a stack for the module */
    uint32_t ko_stack = KO_LOAD_BASE + (KO_LOAD_MAX - KO_LOAD_BASE) - 0x1000;
    cpu_emu_set_reg(sim->cpu, UC_ARM_REG_SP, ko_stack);

    return 0;
}

int ko_unload(bbb_sim_t *sim)
{
    if (!ko_loaded) {
        fprintf(stderr, "[KO] No module loaded\n");
        return -1;
    }

    if (ko_cleanup_addr != 0) {
        printf("[KO] Calling cleanup_module()...\n");
        cpu_emu_set_reg(sim->cpu, UC_ARM_REG_PC, ko_cleanup_addr);

        uint32_t halt_addr = MOCK_KERNEL_BASE + 0xFFF0;
        cpu_emu_set_reg(sim->cpu, UC_ARM_REG_LR, halt_addr);

        /* Run cleanup (will hit UDF and stop) */
        cpu_emu_start(sim->cpu, ko_cleanup_addr);
    }

    elf_info_free(&ko_elf_info);
    ko_loaded = false;
    sim->ko_loaded = false;
    ko_init_addr = 0;
    ko_cleanup_addr = 0;
    kmalloc_next = KMALLOC_REGION_BASE;

    printf("[KO] Module unloaded\n");
    return 0;
}

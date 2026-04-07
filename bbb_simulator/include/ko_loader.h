/*
 * ko_loader.h - Kernel module (.ko) loader with mock kernel API
 */
#ifndef KO_LOADER_H
#define KO_LOADER_H

#include <stdint.h>

typedef struct bbb_sim bbb_sim_t;

/* Mock kernel symbol entry */
typedef struct {
    const char *name;
    uint32_t addr;  /* Address in Unicorn memory where stub resides */
} mock_symbol_t;

/* KO load address (region reserved for kernel modules) */
#define KO_LOAD_BASE        0x82000000
#define KO_LOAD_MAX         0x83000000
#define MOCK_KERNEL_BASE    0x83000000
#define MOCK_KERNEL_MAX     0x83100000

/* Load and execute a kernel module (.ko) */
int ko_load(bbb_sim_t *sim, const char *ko_path);

/* Unload a kernel module (call cleanup_module) */
int ko_unload(bbb_sim_t *sim);

/* Initialize mock kernel stubs in Unicorn memory */
int mock_kernel_init(bbb_sim_t *sim);

/* Handle mock SVC calls from kernel module code */
int ko_handle_mock_svc(bbb_sim_t *sim, uint32_t mock_id);

#endif /* KO_LOADER_H */

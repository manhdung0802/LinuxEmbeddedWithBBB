/*
 * main.c - BBB Simulator entry point
 *
 * Usage:
 *   bbb_simulator                   # Launch GUI only
 *   bbb_simulator firmware.elf      # Load firmware and launch GUI
 *   bbb_simulator --ko module.ko    # Load kernel module and launch GUI
 */
#include "bbb_sim.h"
#include "gui.h"
#include <stdio.h>
#include <string.h>

static void print_usage(const char *prog)
{
    printf("BBB Simulator - BeagleBone Black Hardware Simulator\n\n");
    printf("Usage:\n");
    printf("  %s                       Launch GUI\n", prog);
    printf("  %s <firmware.elf>        Load firmware and launch\n", prog);
    printf("  %s --ko <module.ko>      Load kernel module\n", prog);
    printf("  %s --help                Show this help\n\n", prog);
    printf("Requirements:\n");
    printf("  - Firmware must be statically compiled ARM ELF:\n");
    printf("    arm-linux-gnueabihf-gcc -static -o firmware firmware.c\n");
    printf("  - Kernel modules must be compiled for ARM:\n");
    printf("    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-\n");
}

int main(int argc, char **argv)
{
    const char *firmware_path = NULL;
    const char *ko_path = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--ko") == 0) {
            if (i + 1 < argc) {
                ko_path = argv[++i];
            } else {
                fprintf(stderr, "Error: --ko requires a path argument\n");
                return 1;
            }
        } else if (argv[i][0] != '-') {
            firmware_path = argv[i];
        }
    }

    printf("=== BBB Simulator ===\n");
    printf("Initializing AM335x SoC emulation...\n\n");

    /* Create simulator */
    bbb_sim_t *sim = bbb_sim_create();
    if (!sim) {
        fprintf(stderr, "Failed to create simulator\n");
        return 1;
    }

    /* Initialize all subsystems */
    if (bbb_sim_init(sim) != 0) {
        fprintf(stderr, "Failed to initialize simulator\n");
        bbb_sim_destroy(sim);
        return 1;
    }

    /* Load firmware if specified */
    if (firmware_path) {
        printf("\nLoading firmware: %s\n", firmware_path);
        if (bbb_sim_load_firmware(sim, firmware_path) != 0) {
            fprintf(stderr, "Failed to load firmware\n");
            bbb_sim_destroy(sim);
            return 1;
        }
    }

    /* Load kernel module if specified */
    if (ko_path) {
        printf("\nLoading kernel module: %s\n", ko_path);
        if (bbb_sim_load_ko(sim, ko_path) != 0) {
            fprintf(stderr, "Failed to load kernel module\n");
            bbb_sim_destroy(sim);
            return 1;
        }
    }

    /* Create and run GUI */
    sim->gui = gui_create(sim);
    if (!sim->gui) {
        fprintf(stderr, "Failed to create GUI\n");
        bbb_sim_destroy(sim);
        return 1;
    }

    int ret = gui_run(sim->gui, argc, argv);

    /* Cleanup */
    printf("\nShutting down...\n");
    gui_destroy(sim->gui);
    bbb_sim_destroy(sim);

    return ret;
}

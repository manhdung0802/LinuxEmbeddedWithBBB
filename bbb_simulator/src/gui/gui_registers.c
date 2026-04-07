/*
 * gui_registers.c - Register viewer panel
 *
 * Shows register values for selected peripheral in a tree view.
 * Dropdown selects which peripheral to inspect (CPU, GPIO0-3, UART0, etc.)
 */
#include "gui.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include "periph/gpio.h"
#include "periph/uart.h"
#include "periph/timer.h"
#include "periph/prcm.h"
#include "am335x_map.h"
#include <stdio.h>
#include <string.h>
#include <unicorn/unicorn.h>

enum {
    COL_REG_NAME,
    COL_REG_OFFSET,
    COL_REG_VALUE,
    COL_REG_NUM
};

static void on_periph_changed(GtkComboBox *combo, gpointer data);

void gui_registers_init(bbb_gui_t *gui, GtkWidget *container)
{
    gui->reg_frame = gtk_frame_new("Registers");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(gui->reg_frame), vbox);

    /* Peripheral selector combo */
    gui->reg_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "CPU (ARM)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "GPIO0");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "GPIO1");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "GPIO2");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "GPIO3");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "UART0");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "TIMER2");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->reg_combo), "CM_PER");
    gtk_combo_box_set_active(GTK_COMBO_BOX(gui->reg_combo), 0);
    g_signal_connect(gui->reg_combo, "changed", G_CALLBACK(on_periph_changed), gui);
    gtk_box_pack_start(GTK_BOX(vbox), gui->reg_combo, FALSE, FALSE, 0);

    /* Tree view */
    gui->reg_store = gtk_list_store_new(COL_REG_NUM,
                                         G_TYPE_STRING,   /* Name */
                                         G_TYPE_STRING,   /* Offset */
                                         G_TYPE_STRING);  /* Value */

    gui->reg_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gui->reg_store));
    g_object_unref(gui->reg_store);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Register", renderer, "text", COL_REG_NAME, NULL);
    gtk_tree_view_column_set_min_width(col, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->reg_treeview), col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Offset", renderer, "text", COL_REG_OFFSET, NULL);
    gtk_tree_view_column_set_min_width(col, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->reg_treeview), col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Value", renderer, "text", COL_REG_VALUE, NULL);
    gtk_tree_view_column_set_min_width(col, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->reg_treeview), col);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), gui->reg_treeview);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    /* Initial populate */
    gui_update_registers(gui);
}

static void add_reg_row(GtkListStore *store, const char *name, const char *offset, uint32_t value)
{
    GtkTreeIter iter;
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "0x%08X", value);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COL_REG_NAME, name,
                       COL_REG_OFFSET, offset,
                       COL_REG_VALUE, val_str, -1);
}

static void populate_cpu_regs(bbb_gui_t *gui)
{
    GtkListStore *store = gui->reg_store;

    static const struct { const char *name; int reg; } arm_regs[] = {
        {"R0",  UC_ARM_REG_R0},  {"R1",  UC_ARM_REG_R1},
        {"R2",  UC_ARM_REG_R2},  {"R3",  UC_ARM_REG_R3},
        {"R4",  UC_ARM_REG_R4},  {"R5",  UC_ARM_REG_R5},
        {"R6",  UC_ARM_REG_R6},  {"R7",  UC_ARM_REG_R7},
        {"R8",  UC_ARM_REG_R8},  {"R9",  UC_ARM_REG_R9},
        {"R10", UC_ARM_REG_R10}, {"R11 (FP)", UC_ARM_REG_R11},
        {"R12", UC_ARM_REG_R12},
        {"SP",  UC_ARM_REG_SP},  {"LR",  UC_ARM_REG_LR},
        {"PC",  UC_ARM_REG_PC},  {"CPSR", UC_ARM_REG_CPSR},
        {NULL, 0}
    };

    for (int i = 0; arm_regs[i].name; i++) {
        uint32_t val = cpu_emu_get_reg(gui->sim->cpu, arm_regs[i].reg);
        add_reg_row(store, arm_regs[i].name, "--", val);
    }
}

static void populate_gpio_regs(bbb_gui_t *gui, int bank)
{
    GtkListStore *store = gui->reg_store;
    gpio_periph_t *gpio = gui->sim->gpio[bank];
    if (!gpio) return;

    static const struct { const char *name; uint32_t off; } regs[] = {
        {"REVISION",    GPIO_REVISION},
        {"OE",          GPIO_OE},
        {"DATAIN",      GPIO_DATAIN},
        {"DATAOUT",     GPIO_DATAOUT},
        {"SETDATAOUT",  GPIO_SETDATAOUT},
        {"CLEARDATAOUT",GPIO_CLEARDATAOUT},
        {"LEVELDET0",   GPIO_LEVELDETECT0},
        {"LEVELDET1",   GPIO_LEVELDETECT1},
        {"RISINGDET",   GPIO_RISINGDETECT},
        {"FALLINGDET",  GPIO_FALLINGDETECT},
        {"IRQSTATUS_0", GPIO_IRQSTATUS_0},
        {"IRQSTATUS_1", GPIO_IRQSTATUS_1},
        {"CTRL",        GPIO_CTRL},
        {NULL, 0}
    };

    for (int i = 0; regs[i].name; i++) {
        uint32_t val = gpio_read(gpio, 0, regs[i].off);
        char off_str[16];
        snprintf(off_str, sizeof(off_str), "0x%03X", regs[i].off);
        add_reg_row(store, regs[i].name, off_str, val);
    }
}

static void on_periph_changed(GtkComboBox *combo, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    gui_update_registers(gui);
}

void gui_update_registers(bbb_gui_t *gui)
{
    if (!gui || !gui->gui_active || !gui->reg_store) return;

    gtk_list_store_clear(gui->reg_store);

    int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(gui->reg_combo));
    switch (sel) {
    case 0: populate_cpu_regs(gui); break;
    case 1: populate_gpio_regs(gui, 0); break;
    case 2: populate_gpio_regs(gui, 1); break;
    case 3: populate_gpio_regs(gui, 2); break;
    case 4: populate_gpio_regs(gui, 3); break;
    case 5: {
        /* UART0 basic registers */
        if (gui->sim->uart[0]) {
            static const struct { const char *name; uint32_t off; } regs[] = {
                {"LCR", UART_LCR}, {"LSR", UART_LSR},
                {"MDR1", UART_MDR1}, {"IER", UART_IER},
                {"MCR", UART_MCR}, {"MSR", UART_MSR},
                {NULL, 0}
            };
            for (int i = 0; regs[i].name; i++) {
                uint32_t val = uart_read(gui->sim->uart[0], 0, regs[i].off);
                char off_str[16];
                snprintf(off_str, sizeof(off_str), "0x%03X", regs[i].off);
                add_reg_row(gui->reg_store, regs[i].name, off_str, val);
            }
        }
        break;
    }
    default:
        break;
    }
}

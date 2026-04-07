/*
 * gui_main.c - Main GTK3 window layout, toolbar, and event loop
 */
#include "gui.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
static void on_open_firmware(GtkWidget *widget, gpointer data);
static void on_open_ko(GtkWidget *widget, gpointer data);
static void on_run(GtkWidget *widget, gpointer data);
static void on_pause(GtkWidget *widget, gpointer data);
static void on_step(GtkWidget *widget, gpointer data);
static void on_reset(GtkWidget *widget, gpointer data);
static gboolean on_refresh_timer(gpointer data);

bbb_gui_t *gui_create(bbb_sim_t *sim)
{
    bbb_gui_t *gui = calloc(1, sizeof(bbb_gui_t));
    if (!gui) return NULL;
    gui->sim = sim;
    gui->gui_active = false;
    return gui;
}

void gui_destroy(bbb_gui_t *gui)
{
    if (!gui) return;
    if (gui->refresh_timer_id > 0)
        g_source_remove(gui->refresh_timer_id);
    free(gui);
}

static void activate(GtkApplication *app, gpointer user_data)
{
    bbb_gui_t *gui = (bbb_gui_t *)user_data;

    /* Main window */
    gui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gui->window), "BBB Simulator - BeagleBone Black");
    gtk_window_set_default_size(GTK_WINDOW(gui->window), 1200, 800);

    /* Main vertical box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gui->window), vbox);

    /* Toolbar */
    gui->toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(gui->toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    GtkToolItem *ti;

    /* Open Firmware button */
    ti = gtk_tool_button_new(NULL, "Open FW");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "document-open");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_open_firmware), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_open_fw = GTK_WIDGET(ti);

    /* Open .ko button */
    ti = gtk_tool_button_new(NULL, "Open .ko");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "document-open");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_open_ko), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_open_ko = GTK_WIDGET(ti);

    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), gtk_separator_tool_item_new(), -1);

    /* Run button */
    ti = gtk_tool_button_new(NULL, "Run");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "media-playback-start");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_run), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_run = GTK_WIDGET(ti);

    /* Pause button */
    ti = gtk_tool_button_new(NULL, "Pause");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "media-playback-pause");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_pause), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_pause = GTK_WIDGET(ti);

    /* Step button */
    ti = gtk_tool_button_new(NULL, "Step");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "go-next");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_step), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_step = GTK_WIDGET(ti);

    /* Reset button */
    ti = gtk_tool_button_new(NULL, "Reset");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(ti), "view-refresh");
    g_signal_connect(ti, "clicked", G_CALLBACK(on_reset), gui);
    gtk_toolbar_insert(GTK_TOOLBAR(gui->toolbar), ti, -1);
    gui->btn_reset = GTK_WIDGET(ti);

    gtk_box_pack_start(GTK_BOX(vbox), gui->toolbar, FALSE, FALSE, 0);

    /* Main content: 2x2 grid */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_set_margin_start(grid, 4);
    gtk_widget_set_margin_end(grid, 4);
    gtk_widget_set_margin_top(grid, 4);
    gtk_widget_set_margin_bottom(grid, 4);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    /* Top-left: GPIO visualization */
    gui_gpio_init(gui, grid);
    gtk_grid_attach(GTK_GRID(grid), gui->gpio_frame, 0, 0, 1, 1);
    gtk_widget_set_hexpand(gui->gpio_frame, TRUE);
    gtk_widget_set_vexpand(gui->gpio_frame, TRUE);

    /* Top-right: UART console */
    gui_uart_init(gui, grid);
    gtk_grid_attach(GTK_GRID(grid), gui->uart_frame, 1, 0, 1, 1);
    gtk_widget_set_hexpand(gui->uart_frame, TRUE);
    gtk_widget_set_vexpand(gui->uart_frame, TRUE);

    /* Bottom-left: Register viewer */
    gui_registers_init(gui, grid);
    gtk_grid_attach(GTK_GRID(grid), gui->reg_frame, 0, 1, 1, 1);
    gtk_widget_set_hexpand(gui->reg_frame, TRUE);
    gtk_widget_set_vexpand(gui->reg_frame, TRUE);

    /* Bottom-right: Memory viewer */
    gui_memory_init(gui, grid);
    gtk_grid_attach(GTK_GRID(grid), gui->mem_frame, 1, 1, 1, 1);
    gtk_widget_set_hexpand(gui->mem_frame, TRUE);
    gtk_widget_set_vexpand(gui->mem_frame, TRUE);

    /* Status bar */
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(status_box, 4);
    gtk_widget_set_margin_bottom(status_box, 2);

    gui->lbl_state = gtk_label_new("State: IDLE");
    gtk_box_pack_start(GTK_BOX(status_box), gui->lbl_state, FALSE, FALSE, 0);

    gui->lbl_pc = gtk_label_new("PC: 0x00000000");
    gtk_box_pack_start(GTK_BOX(status_box), gui->lbl_pc, FALSE, FALSE, 0);

    gui->lbl_instr_count = gtk_label_new("Instructions: 0");
    gtk_box_pack_start(GTK_BOX(status_box), gui->lbl_instr_count, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(vbox), status_box, FALSE, FALSE, 0);

    /* Start refresh timer (100ms) */
    gui->refresh_timer_id = g_timeout_add(100, on_refresh_timer, gui);
    gui->gui_active = true;

    gtk_widget_show_all(gui->window);
}

int gui_run(bbb_gui_t *gui, int argc, char **argv)
{
    gui->app = gtk_application_new("com.bbb.simulator", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gui->app, "activate", G_CALLBACK(activate), gui);

    int status = g_application_run(G_APPLICATION(gui->app), 0, NULL);
    g_object_unref(gui->app);
    return status;
}

/* ---- Toolbar callbacks ---- */

static void on_open_firmware(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open Firmware ELF", GTK_WINDOW(gui->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "ELF files");
    gtk_file_filter_add_pattern(filter, "*.elf");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        bbb_sim_load_firmware(gui->sim, filename);
        gui_update_status(gui);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_open_ko(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open Kernel Module", GTK_WINDOW(gui->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Kernel modules (*.ko)");
    gtk_file_filter_add_pattern(filter, "*.ko");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        bbb_sim_load_ko(gui->sim, filename);
        gui_update_status(gui);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_run(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    bbb_sim_run(gui->sim);
    gui_update_status(gui);
}

static void on_pause(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    bbb_sim_pause(gui->sim);
    gui_update_status(gui);
}

static void on_step(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    bbb_sim_step(gui->sim);
    gui_update_status(gui);
    gui_update_registers(gui);
}

static void on_reset(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    bbb_sim_reset(gui->sim);
    gui_update_status(gui);
    gui_update_gpio(gui);
    gui_update_registers(gui);
}

/* ---- Status bar update ---- */

void gui_update_status(bbb_gui_t *gui)
{
    if (!gui || !gui->gui_active) return;

    const char *state_str[] = {"IDLE", "RUNNING", "PAUSED", "STOPPED", "ERROR"};
    sim_state_t st = bbb_sim_get_state(gui->sim);
    char buf[64];

    snprintf(buf, sizeof(buf), "State: %s", state_str[st]);
    gtk_label_set_text(GTK_LABEL(gui->lbl_state), buf);

    snprintf(buf, sizeof(buf), "PC: 0x%08X", bbb_sim_get_pc(gui->sim));
    gtk_label_set_text(GTK_LABEL(gui->lbl_pc), buf);

    snprintf(buf, sizeof(buf), "Instructions: %lu", bbb_sim_get_instr_count(gui->sim));
    gtk_label_set_text(GTK_LABEL(gui->lbl_instr_count), buf);
}

/* ---- Periodic refresh ---- */

static gboolean on_refresh_timer(gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    if (!gui->gui_active) return G_SOURCE_REMOVE;

    gui_update_status(gui);

    if (gui->sim->state == SIM_STATE_RUNNING) {
        gui_update_gpio(gui);
    }

    return G_SOURCE_CONTINUE;
}

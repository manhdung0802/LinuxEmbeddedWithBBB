/*
 * gui.h - GTK3 GUI interface
 */
#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct bbb_sim bbb_sim_t;

typedef struct bbb_gui {
    bbb_sim_t *sim;

    /* Main window */
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *toolbar;

    /* Toolbar buttons */
    GtkWidget *btn_open_fw;
    GtkWidget *btn_open_ko;
    GtkWidget *btn_run;
    GtkWidget *btn_pause;
    GtkWidget *btn_step;
    GtkWidget *btn_reset;

    /* Status bar */
    GtkWidget *statusbar;
    GtkWidget *lbl_state;
    GtkWidget *lbl_pc;
    GtkWidget *lbl_instr_count;

    /* GPIO panel */
    GtkWidget *gpio_drawing_area;
    GtkWidget *gpio_frame;

    /* UART console */
    GtkWidget *uart_textview;
    GtkTextBuffer *uart_buffer;
    GtkWidget *uart_entry;
    GtkWidget *uart_frame;

    /* Register viewer */
    GtkWidget *reg_combo;       /* Peripheral selector */
    GtkWidget *reg_treeview;
    GtkListStore *reg_store;
    GtkWidget *reg_frame;

    /* Memory viewer */
    GtkWidget *mem_addr_entry;
    GtkWidget *mem_textview;
    GtkTextBuffer *mem_buffer;
    GtkWidget *mem_frame;

    /* Periodic refresh timer */
    guint refresh_timer_id;
    bool gui_active;
} bbb_gui_t;

/* Lifecycle */
bbb_gui_t *gui_create(bbb_sim_t *sim);
void gui_destroy(bbb_gui_t *gui);

/* Run GTK main loop (blocks until window closes) */
int gui_run(bbb_gui_t *gui, int argc, char **argv);

/* Update functions (thread-safe via gdk_threads_add_idle) */
void gui_update_gpio(bbb_gui_t *gui);
void gui_append_uart_text(bbb_gui_t *gui, const char *text, int len);
void gui_update_registers(bbb_gui_t *gui);
void gui_update_memory(bbb_gui_t *gui);
void gui_update_status(bbb_gui_t *gui);

/* GUI sub-module init functions */
void gui_gpio_init(bbb_gui_t *gui, GtkWidget *container);
void gui_uart_init(bbb_gui_t *gui, GtkWidget *container);
void gui_registers_init(bbb_gui_t *gui, GtkWidget *container);
void gui_memory_init(bbb_gui_t *gui, GtkWidget *container);

/* GPIO drawing */
gboolean gui_gpio_draw(GtkWidget *widget, cairo_t *cr, gpointer data);

#endif /* GUI_H */

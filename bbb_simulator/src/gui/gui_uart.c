/*
 * gui_uart.c - UART console panel
 *
 * Shows UART0 output in a scrollable text view.
 * Entry field at the bottom for sending characters to UART0 RX.
 */
#include "gui.h"
#include "bbb_sim.h"
#include "periph/uart.h"
#include <string.h>

/* Thread-safe data for appending text via gdk_threads_add_idle */
typedef struct {
    bbb_gui_t *gui;
    char *text;
    int len;
} uart_append_data_t;

static gboolean idle_append_text(gpointer user_data)
{
    uart_append_data_t *d = (uart_append_data_t *)user_data;
    if (d->gui && d->gui->uart_buffer) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(d->gui->uart_buffer, &end);
        gtk_text_buffer_insert(d->gui->uart_buffer, &end, d->text, d->len);

        /* Auto-scroll to bottom */
        GtkTextMark *mark = gtk_text_buffer_get_insert(d->gui->uart_buffer);
        gtk_text_buffer_get_end_iter(d->gui->uart_buffer, &end);
        gtk_text_buffer_move_mark(d->gui->uart_buffer, mark, &end);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(d->gui->uart_textview), mark);
    }
    g_free(d->text);
    g_free(d);
    return G_SOURCE_REMOVE;
}

/* UART TX -> console callback */
static void uart_console_cb(const char *text, int len, void *user_data)
{
    bbb_gui_t *gui = (bbb_gui_t *)user_data;
    if (!gui->gui_active) return;

    uart_append_data_t *d = g_new0(uart_append_data_t, 1);
    d->gui = gui;
    d->text = g_strndup(text, len);
    d->len = len;
    gdk_threads_add_idle(idle_append_text, d);
}

static void on_uart_entry_activate(GtkEntry *entry, gpointer user_data)
{
    bbb_gui_t *gui = (bbb_gui_t *)user_data;
    const char *text = gtk_entry_get_text(entry);
    if (!text || !*text) return;

    /* Send each character to UART0 RX FIFO */
    if (gui->sim->uart[0]) {
        int len = strlen(text);
        for (int i = 0; i < len; i++)
            uart_receive_char(gui->sim->uart[0], (uint8_t)text[i]);
        /* Send newline */
        uart_receive_char(gui->sim->uart[0], '\n');
    }

    gtk_entry_set_text(entry, "");
}

void gui_uart_init(bbb_gui_t *gui, GtkWidget *container)
{
    gui->uart_frame = gtk_frame_new("UART0 Console");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(gui->uart_frame), vbox);

    /* Scrolled text view */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 400, 200);

    gui->uart_buffer = gtk_text_buffer_new(NULL);
    gui->uart_textview = gtk_text_view_new_with_buffer(gui->uart_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gui->uart_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(gui->uart_textview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(gui->uart_textview), TRUE);

    /* Dark background for console look */
    GdkRGBA bg = {0.05, 0.05, 0.05, 1.0};
    GdkRGBA fg = {0.0, 1.0, 0.0, 1.0};
    gtk_widget_override_background_color(gui->uart_textview, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_override_color(gui->uart_textview, GTK_STATE_FLAG_NORMAL, &fg);

    gtk_container_add(GTK_CONTAINER(scroll), gui->uart_textview);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    /* Input entry */
    gui->uart_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->uart_entry), "Type to send to UART0...");
    g_signal_connect(gui->uart_entry, "activate", G_CALLBACK(on_uart_entry_activate), gui);
    gtk_box_pack_start(GTK_BOX(vbox), gui->uart_entry, FALSE, FALSE, 0);

    /* Register console callback */
    bbb_sim_set_console_callback(gui->sim, uart_console_cb, gui);
}

void gui_append_uart_text(bbb_gui_t *gui, const char *text, int len)
{
    if (!gui || !gui->gui_active || !gui->uart_buffer) return;

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(gui->uart_buffer, &end);
    gtk_text_buffer_insert(gui->uart_buffer, &end, text, len);
}

/*
 * gui_memory.c - Memory hex viewer panel
 *
 * Shows hex dump of Unicorn memory at a given address.
 * User types address in entry box and content is displayed.
 */
#include "gui.h"
#include "bbb_sim.h"
#include "cpu_emu.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MEM_VIEW_BYTES  256  /* Show 256 bytes per refresh */

static void on_mem_view_clicked(GtkWidget *widget, gpointer data);

void gui_memory_init(bbb_gui_t *gui, GtkWidget *container)
{
    gui->mem_frame = gtk_frame_new("Memory Viewer");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(gui->mem_frame), vbox);

    /* Address entry */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *lbl = gtk_label_new("Address:");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);

    gui->mem_addr_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(gui->mem_addr_entry), "0x80000000");
    gtk_entry_set_width_chars(GTK_ENTRY(gui->mem_addr_entry), 12);
    g_signal_connect(gui->mem_addr_entry, "activate", G_CALLBACK(on_mem_view_clicked), gui);
    gtk_box_pack_start(GTK_BOX(hbox), gui->mem_addr_entry, FALSE, FALSE, 0);

    GtkWidget *btn = gtk_button_new_with_label("View");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_mem_view_clicked), gui);
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Hex view text */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gui->mem_buffer = gtk_text_buffer_new(NULL);
    gui->mem_textview = gtk_text_view_new_with_buffer(gui->mem_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gui->mem_textview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(gui->mem_textview), TRUE);

    GdkRGBA bg = {0.05, 0.05, 0.1, 1.0};
    GdkRGBA fg = {0.8, 0.9, 1.0, 1.0};
    gtk_widget_override_background_color(gui->mem_textview, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_widget_override_color(gui->mem_textview, GTK_STATE_FLAG_NORMAL, &fg);

    gtk_container_add(GTK_CONTAINER(scroll), gui->mem_textview);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
}

static void on_mem_view_clicked(GtkWidget *widget, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    if (!gui || !gui->sim || !gui->sim->cpu) return;
    gui_update_memory(gui);
}

void gui_update_memory(bbb_gui_t *gui)
{
    if (!gui || !gui->gui_active || !gui->mem_buffer) return;

    const char *text = gtk_entry_get_text(GTK_ENTRY(gui->mem_addr_entry));
    uint32_t start_addr = (uint32_t)strtoul(text, NULL, 0);

    uint8_t buf[MEM_VIEW_BYTES];
    memset(buf, 0, sizeof(buf));
    cpu_emu_read_mem(gui->sim->cpu, start_addr, buf, MEM_VIEW_BYTES);

    /* Build hex dump string */
    char hex_dump[MEM_VIEW_BYTES * 5 + (MEM_VIEW_BYTES / 16) * 20 + 256];
    int pos = 0;

    pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos,
                     "Address    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos,
                     "--------  -----------------------------------------------  ----------------\n");

    for (int row = 0; row < MEM_VIEW_BYTES / 16; row++) {
        int off = row * 16;
        pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos,
                         "%08X  ", start_addr + off);

        for (int col = 0; col < 16; col++) {
            if (col == 8)
                pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos, " ");
            pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos,
                             "%02X ", buf[off + col]);
        }

        pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos, " ");
        for (int col = 0; col < 16; col++) {
            char c = buf[off + col];
            pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos,
                             "%c", (c >= 32 && c < 127) ? c : '.');
        }
        pos += snprintf(hex_dump + pos, sizeof(hex_dump) - pos, "\n");
    }

    gtk_text_buffer_set_text(gui->mem_buffer, hex_dump, pos);
}

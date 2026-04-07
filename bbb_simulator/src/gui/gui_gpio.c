/*
 * gui_gpio.c - GPIO visualization panel using Cairo
 *
 * Draws 4 GPIO banks (GPIO0-3), each with 32 LEDs.
 * Green = output HIGH, dark = output LOW, blue = input.
 */
#include "gui.h"
#include "bbb_sim.h"
#include "periph/gpio.h"
#include "am335x_map.h"
#include <stdio.h>
#include <math.h>

#define LED_RADIUS    8
#define LED_SPACING   22
#define BANK_SPACING  30
#define LABEL_WIDTH   60

void gui_gpio_init(bbb_gui_t *gui, GtkWidget *container)
{
    gui->gpio_frame = gtk_frame_new("GPIO Status");

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(gui->gpio_frame), scroll);

    gui->gpio_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(gui->gpio_drawing_area, 750, 200);
    g_signal_connect(gui->gpio_drawing_area, "draw", G_CALLBACK(gui_gpio_draw), gui);
    gtk_container_add(GTK_CONTAINER(scroll), gui->gpio_drawing_area);
}

gboolean gui_gpio_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    bbb_gui_t *gui = (bbb_gui_t *)data;
    if (!gui || !gui->sim) return FALSE;

    /* Background */
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_paint(cr);

    for (int bank = 0; bank < AM335X_GPIO_COUNT; bank++) {
        gpio_periph_t *gpio = gui->sim->gpio[bank];
        if (!gpio) continue;

        double y = 20 + bank * (LED_SPACING + BANK_SPACING);

        /* Bank label */
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        char label[16];
        snprintf(label, sizeof(label), "GPIO%d", bank);
        cairo_move_to(cr, 5, y + LED_RADIUS + 4);
        cairo_show_text(cr, label);

        uint32_t oe = gpio_get_oe(gpio);
        uint32_t dataout = gpio_get_dataout(gpio);
        uint32_t datain = gpio_get_datain(gpio);

        for (int pin = 0; pin < 32; pin++) {
            double x = LABEL_WIDTH + pin * LED_SPACING;
            uint32_t mask = 1U << pin;

            bool is_output = !(oe & mask); /* OE: 0=output, 1=input */
            bool is_high;

            if (is_output)
                is_high = (dataout & mask) != 0;
            else
                is_high = (datain & mask) != 0;

            /* LED color */
            if (is_output) {
                if (is_high)
                    cairo_set_source_rgb(cr, 0.0, 0.9, 0.0); /* Green = HIGH */
                else
                    cairo_set_source_rgb(cr, 0.2, 0.3, 0.2); /* Dark green = LOW */
            } else {
                if (is_high)
                    cairo_set_source_rgb(cr, 0.3, 0.5, 1.0); /* Blue = input HIGH */
                else
                    cairo_set_source_rgb(cr, 0.1, 0.15, 0.3); /* Dark blue = input LOW */
            }

            cairo_arc(cr, x + LED_RADIUS, y + LED_RADIUS, LED_RADIUS, 0, 2 * M_PI);
            cairo_fill(cr);

            /* Pin border */
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
            cairo_set_line_width(cr, 1.0);
            cairo_arc(cr, x + LED_RADIUS, y + LED_RADIUS, LED_RADIUS, 0, 2 * M_PI);
            cairo_stroke(cr);

            /* Pin number (every 4th pin) */
            if (pin % 4 == 0) {
                cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
                cairo_set_font_size(cr, 8);
                char num[8];
                snprintf(num, sizeof(num), "%d", pin);
                cairo_move_to(cr, x + LED_RADIUS - 4, y + LED_RADIUS * 2 + 10);
                cairo_show_text(cr, num);
            }
        }
    }

    return FALSE;
}

void gui_update_gpio(bbb_gui_t *gui)
{
    if (!gui || !gui->gui_active || !gui->gpio_drawing_area) return;
    gtk_widget_queue_draw(gui->gpio_drawing_area);
}

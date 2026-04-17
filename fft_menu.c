/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include "css.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "menu_page.h"
#include "new_menu.h"
#include "fft_menu.h"
#include "radio.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void filter_type_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  set_filter_type((unsigned int)i);
}

#ifdef SET_FILTER_SIZE
static void filter_size_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int sizes[] = { 1024, 2048, 4096, 8192, 16384 };
  if (i >= (int)(sizeof(sizes) / sizeof(sizes[0]))) {
    return;
  }
  set_filter_size(sizes[i]);
}
#endif

void fft_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - FFT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *filter_type_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(filter_type_label), "<b>Filter type</b>");
  gtk_widget_set_halign(filter_type_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),filter_type_label,0,1,1,1);

  GtkWidget *filter_type_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_type_combo), NULL, "Linear phase");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_type_combo), NULL, "Low latency");
  gtk_combo_box_set_active(GTK_COMBO_BOX(filter_type_combo), receiver[0]->low_latency ? 1 : 0);
  gtk_widget_set_hexpand(filter_type_combo, TRUE);
  gtk_widget_set_halign(filter_type_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),filter_type_combo,1,1,1,1);
  g_signal_connect(filter_type_combo, "changed", G_CALLBACK(filter_type_combo_changed), NULL);

#ifdef SET_FILTER_SIZE
  GtkWidget *filter_size_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(filter_size_label), "<b>FFT size</b>");
  gtk_widget_set_halign(filter_size_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),filter_size_label,0,2,1,1);

  GtkWidget *filter_size_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "1024");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "2048");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "4096");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "8192");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_size_combo), NULL, "16384");
  {
    static const int sizes[] = { 1024, 2048, 4096, 8192, 16384 };
    int j;
    int sel = 0;
    for (j = 0; j < (int)(sizeof(sizes) / sizeof(sizes[0])); j++) {
      if (receiver[0]->fft_size == sizes[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_size_combo), sel);
  }
  gtk_widget_set_hexpand(filter_size_combo, TRUE);
  gtk_widget_set_halign(filter_size_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),filter_size_combo,1,2,1,1);
  g_signal_connect(filter_size_combo, "changed", G_CALLBACK(filter_size_combo_changed), NULL);
#endif

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

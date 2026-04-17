/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "menu_page.h"
#include "new_menu.h"
#include "radio.h"
#include "vfo.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;

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

static void step_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= STEPS) {
    return;
  }
  vfo_set_step_from_index(i);
  g_idle_add(ext_vfo_update,NULL);
}

void step_menu(GtkWidget *parent) {

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - VFO Step");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);


  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *step_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(step_label), "<b>VFO step</b>");
  gtk_widget_set_halign(step_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),step_label,0,1,1,1);

  GtkWidget *step_combo=gtk_combo_box_text_new();
  int idx;
  for (idx = 0; idx < STEPS; idx++) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(step_combo), NULL, step_labels[idx]);
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(step_combo), vfo_get_stepindex());
  gtk_widget_set_hexpand(step_combo, TRUE);
  gtk_widget_set_halign(step_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),step_combo,1,1,1,1);
  g_signal_connect(step_combo, "changed", G_CALLBACK(step_combo_changed), NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

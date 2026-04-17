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
#include "agc_menu.h"
#include "agc.h"
#include "band.h"
#include "channel.h"
#include "radio.h"
#include "receiver.h"
#include "vfo.h"
#include "button_text.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static void cleanup() {
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
    active_menu=NO_MENU;
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

static const int agc_combo_values[] = {
  AGC_OFF,
  AGC_LONG,
  AGC_SLOW,
  AGC_MEDIUM,
  AGC_FAST
};

static void agc_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= (int)(sizeof(agc_combo_values) / sizeof(agc_combo_values[0]))) {
    return;
  }
  active_receiver->agc = agc_combo_values[i];
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_agc(client_socket,active_receiver->id,active_receiver->agc);
  } else {
#endif
    set_agc(active_receiver, active_receiver->agc);
    g_idle_add(ext_vfo_update, NULL);
#ifdef CLIENT_SERVER
  }
#endif
}

void agc_menu(GtkWidget *parent) {

  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  char title[64];
  sprintf(title,"piHPSDR - AGC (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);


  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *agc_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(agc_label), "<b>AGC</b>");
  gtk_widget_set_halign(agc_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),agc_label,0,1,1,1);

  GtkWidget *agc_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agc_combo), NULL, "Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agc_combo), NULL, "Long");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agc_combo), NULL, "Slow");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agc_combo), NULL, "Medium");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agc_combo), NULL, "Fast");
  {
    int sel = 0;
    int j;
    for (j = 0; j < (int)(sizeof(agc_combo_values) / sizeof(agc_combo_values[0])); j++) {
      if (active_receiver->agc == agc_combo_values[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(agc_combo), sel);
  }
  gtk_widget_set_hexpand(agc_combo, TRUE);
  gtk_widget_set_halign(agc_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),agc_combo,1,1,1,1);
  g_signal_connect(agc_combo, "changed", G_CALLBACK(agc_combo_changed), NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

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
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <wdsp.h>

#include "menu_page.h"
#include "new_menu.h"
#include "receiver.h"
#include "meter_menu.h"
#include "meter.h"
#include "radio.h"

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

static const unsigned int smeter_values[] = {
  RXA_S_PK,
  RXA_S_AV
};

static void smeter_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= (int)(sizeof(smeter_values) / sizeof(smeter_values[0]))) {
    return;
  }
  smeter = smeter_values[i];
}

static const unsigned int alc_values[] = {
  TXA_ALC_PK,
  TXA_ALC_AV,
  TXA_ALC_GAIN
};

static void alc_meter_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= (int)(sizeof(alc_values) / sizeof(alc_values[0]))) {
    return;
  }
  alc = alc_values[i];
}

static void analog_cb(GtkToggleButton *widget, gpointer data) {
  analog_meter=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

void meter_menu (GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Meter");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *analog_b=gtk_check_button_new_with_label("Analog meter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (analog_b), analog_meter);
  gtk_grid_attach(GTK_GRID(grid),analog_b,1,0,1,1);
  g_signal_connect(analog_b,"toggled",G_CALLBACK(analog_cb),NULL);

  GtkWidget *smeter_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(smeter_label), "<b>S-meter source</b>");
  gtk_widget_set_halign(smeter_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),smeter_label,0,1,1,1);

  GtkWidget *smeter_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(smeter_combo), NULL, "Peak");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(smeter_combo), NULL, "Average");
  {
    int sel = 0;
    int j;
    for (j = 0; j < (int)(sizeof(smeter_values) / sizeof(smeter_values[0])); j++) {
      if (smeter == smeter_values[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(smeter_combo), sel);
  }
  gtk_widget_set_hexpand(smeter_combo, TRUE);
  gtk_widget_set_halign(smeter_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),smeter_combo,1,1,1,1);
  g_signal_connect(smeter_combo, "changed", G_CALLBACK(smeter_combo_changed), NULL);

  GtkWidget *alc_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(alc_label), "<b>ALC meter source</b>");
  gtk_widget_set_halign(alc_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),alc_label,0,2,1,1);

  GtkWidget *alc_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alc_combo), NULL, "Peak");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alc_combo), NULL, "Average");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alc_combo), NULL, "Gain");
  {
    int sel = 0;
    int j;
    for (j = 0; j < (int)(sizeof(alc_values) / sizeof(alc_values[0])); j++) {
      if (alc == alc_values[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(alc_combo), sel);
  }
  gtk_widget_set_hexpand(alc_combo, TRUE);
  gtk_widget_set_halign(alc_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),alc_combo,1,2,1,1);
  g_signal_connect(alc_combo, "changed", G_CALLBACK(alc_meter_combo_changed), NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

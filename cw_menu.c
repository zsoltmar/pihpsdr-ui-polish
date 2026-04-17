/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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
#include "pa_menu.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "iambic.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *menu_b=NULL;

static GtkWidget *dialog=NULL;

void cw_changed() {
// inform the local keyer about CW parameter changes
// (only if LOCALCW is active).
// NewProtocol: rely on periodically sent HighPrio packets
#ifdef LOCALCW
  keyer_update();
#endif
//
// speed and side tone frequency are displayed in the VFO bar
//
  g_idle_add(ext_vfo_update, NULL);
}

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

static void cw_vfo_ref_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  cw_is_on_vfo_freq = (i == 0) ? 1 : 0;
}

static void cw_keyer_internal_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_internal=cw_keyer_internal==1?0:1;
  cw_changed();
}

static void cw_keyer_spacing_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_spacing=cw_keyer_spacing==1?0:1;
  cw_changed();
}

static void cw_keyer_speed_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_speed=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_breakin_cb(GtkWidget *widget, gpointer data) {
  cw_breakin=cw_breakin==1?0:1;
  cw_changed();
}

static void cw_keyer_hang_time_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_hang_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keyer_weight_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_weight=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keys_reversed_cb(GtkWidget *widget, gpointer data) {
  cw_keys_reversed=cw_keys_reversed==1?0:1;
  cw_changed();
}

static void cw_keyer_mode_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const unsigned int modes[] = {
    KEYER_STRAIGHT,
    KEYER_MODE_A,
    KEYER_MODE_B
  };
  if (i >= (int)(sizeof(modes) / sizeof(modes[0]))) {
    return;
  }
  cw_keyer_mode = modes[i];
  cw_changed();
}

static void cw_keyer_sidetone_level_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_sidetone_volume=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
}

static void cw_keyer_sidetone_frequency_value_changed_cb(GtkWidget *widget, gpointer data) {
  cw_keyer_sidetone_frequency=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  cw_changed();
  receiver_filter_changed(active_receiver);
  // changing the side tone frequency affects BFO frequency offsets
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

void cw_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - CW");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *cw_speed_label=gtk_label_new("CW Speed (WPM)");
  //gtk_widget_override_font(cw_speed_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(cw_speed_label);
  gtk_grid_attach(GTK_GRID(grid),cw_speed_label,0,1,1,1);

  GtkWidget *cw_keyer_speed_b=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  //gtk_widget_override_font(cw_keyer_speed_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_speed_b),(double)cw_keyer_speed);
    gtk_widget_show(cw_keyer_speed_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_speed_b,1,1,1,1);
  g_signal_connect(cw_keyer_speed_b,"value_changed",G_CALLBACK(cw_keyer_speed_value_changed_cb),NULL);

  GtkWidget *cw_breakin_b=gtk_check_button_new_with_label("CW Break In - Delay (ms)");
  //gtk_widget_override_font(cw_breakin_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_breakin_b), cw_breakin);
  gtk_widget_show(cw_breakin_b);
  gtk_grid_attach(GTK_GRID(grid),cw_breakin_b,0,2,1,1);
  g_signal_connect(cw_breakin_b,"toggled",G_CALLBACK(cw_breakin_cb),NULL);

  GtkWidget *cw_keyer_hang_time_b=gtk_spin_button_new_with_range(0.0,1000.0,1.0);
  //gtk_widget_override_font(cw_keyer_hang_time_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_hang_time_b),(double)cw_keyer_hang_time);
  gtk_widget_show(cw_keyer_hang_time_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_hang_time_b,1,2,1,1);
  g_signal_connect(cw_keyer_hang_time_b,"value_changed",G_CALLBACK(cw_keyer_hang_time_value_changed_cb),NULL);

  GtkWidget *cw_keyer_mode_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(cw_keyer_mode_label), "<b>Keyer mode</b>");
  gtk_widget_set_halign(cw_keyer_mode_label, GTK_ALIGN_START);
  gtk_widget_show(cw_keyer_mode_label);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_label,0,3,1,1);

  GtkWidget *cw_keyer_mode_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cw_keyer_mode_combo), NULL, "Straight key");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cw_keyer_mode_combo), NULL, "Mode A");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cw_keyer_mode_combo), NULL, "Mode B");
  {
    int sel = 0;
    if (cw_keyer_mode == KEYER_MODE_A) {
      sel = 1;
    } else if (cw_keyer_mode == KEYER_MODE_B) {
      sel = 2;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(cw_keyer_mode_combo), sel);
  }
  gtk_widget_set_hexpand(cw_keyer_mode_combo, TRUE);
  gtk_widget_set_halign(cw_keyer_mode_combo, GTK_ALIGN_FILL);
  gtk_widget_show(cw_keyer_mode_combo);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_mode_combo,1,3,1,1);
  g_signal_connect(cw_keyer_mode_combo, "changed", G_CALLBACK(cw_keyer_mode_combo_changed), NULL);

  GtkWidget *cw_vfo_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(cw_vfo_label), "<b>CW pitch reference</b>");
  gtk_widget_set_halign(cw_vfo_label, GTK_ALIGN_START);
  gtk_widget_show(cw_vfo_label);
  gtk_grid_attach(GTK_GRID(grid),cw_vfo_label,0,4,1,1);

  GtkWidget *cw_vfo_ref_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cw_vfo_ref_combo), NULL, "On VFO frequency");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cw_vfo_ref_combo), NULL, "VFO ± sidetone offset");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cw_vfo_ref_combo), cw_is_on_vfo_freq ? 0 : 1);
  gtk_widget_set_hexpand(cw_vfo_ref_combo, TRUE);
  gtk_widget_set_halign(cw_vfo_ref_combo, GTK_ALIGN_FILL);
  gtk_widget_show(cw_vfo_ref_combo);
  gtk_grid_attach(GTK_GRID(grid),cw_vfo_ref_combo,1,4,1,1);
  g_signal_connect(cw_vfo_ref_combo, "changed", G_CALLBACK(cw_vfo_ref_combo_changed), NULL);

  GtkWidget *cw_keys_reversed_b=gtk_check_button_new_with_label("Keys reversed");
  //gtk_widget_override_font(cw_keys_reversed_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keys_reversed_b), cw_keys_reversed);
  gtk_widget_show(cw_keys_reversed_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keys_reversed_b,0,5,1,1);
  g_signal_connect(cw_keys_reversed_b,"toggled",G_CALLBACK(cw_keys_reversed_cb),NULL);

  GtkWidget *cw_keyer_sidetone_level_label=gtk_label_new("Sidetone Level:");
  //gtk_widget_override_font(cw_keyer_sidetone_level_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(cw_keyer_sidetone_level_label);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_sidetone_level_label,0,6,1,1);

  GtkWidget *cw_keyer_sidetone_level_b=gtk_spin_button_new_with_range(0.0,protocol==NEW_PROTOCOL?255.0:127.0,1.0);
  //gtk_widget_override_font(cw_keyer_sidetone_level_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_sidetone_level_b),(double)cw_keyer_sidetone_volume);
  gtk_widget_show(cw_keyer_sidetone_level_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_sidetone_level_b,1,6,1,1);
  g_signal_connect(cw_keyer_sidetone_level_b,"value_changed",G_CALLBACK(cw_keyer_sidetone_level_value_changed_cb),NULL);

  GtkWidget *cw_keyer_sidetone_frequency_label=gtk_label_new("Sidetone Freq:");
  //gtk_widget_override_font(cw_keyer_sidetone_frequency_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(cw_keyer_sidetone_frequency_label);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_sidetone_frequency_label,0,7,1,1);

  GtkWidget *cw_keyer_sidetone_frequency_b=gtk_spin_button_new_with_range(100.0,1000.0,1.0);
  //gtk_widget_override_font(cw_keyer_sidetone_frequency_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_sidetone_frequency_b),(double)cw_keyer_sidetone_frequency);
  gtk_widget_show(cw_keyer_sidetone_frequency_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_sidetone_frequency_b,1,7,1,1);
  g_signal_connect(cw_keyer_sidetone_frequency_b,"value_changed",G_CALLBACK(cw_keyer_sidetone_frequency_value_changed_cb),NULL);

  GtkWidget *cw_keyer_weight_label=gtk_label_new("Weight:");
  //gtk_widget_override_font(cw_keyer_weight_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(cw_keyer_weight_label);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_weight_label,0,8,1,1);

  GtkWidget *cw_keyer_weight_b=gtk_spin_button_new_with_range(0.0,100.0,1.0);
  //gtk_widget_override_font(cw_keyer_weight_b, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cw_keyer_weight_b),(double)cw_keyer_weight);
  gtk_widget_show(cw_keyer_weight_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_weight_b,1,8,1,1);
  g_signal_connect(cw_keyer_weight_b,"value_changed",G_CALLBACK(cw_keyer_weight_value_changed_cb),NULL);

#ifdef LOCALCW
  GtkWidget *cw_keyer_internal_b=gtk_check_button_new_with_label("CW handled in Radio");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_internal_b), cw_keyer_internal);
  gtk_widget_show(cw_keyer_internal_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_internal_b,0,9,1,1);
  g_signal_connect(cw_keyer_internal_b,"toggled",G_CALLBACK(cw_keyer_internal_cb),NULL);

  GtkWidget *cw_keyer_spacing_b=gtk_check_button_new_with_label("CW enforce letter spacing");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw_keyer_spacing_b), cw_keyer_spacing);
  gtk_widget_show(cw_keyer_spacing_b);
  gtk_grid_attach(GTK_GRID(grid),cw_keyer_spacing_b,0,10,1,1);
  g_signal_connect(cw_keyer_spacing_b,"toggled",G_CALLBACK(cw_keyer_spacing_cb),NULL);
#endif

  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

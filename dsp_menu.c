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
#include "dsp_menu.h"
#include "agc.h"
#include "channel.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "wdsp.h"

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

static void agc_hang_threshold_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->agc_hang_threshold=(int)gtk_range_get_value(GTK_RANGE(widget));
  set_agc(active_receiver, active_receiver->agc);
}

static void nr_agc_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  active_receiver->nr_agc=(unsigned int)i;
  SetRXAEMNRPosition(active_receiver->id, active_receiver->nr_agc);
}

static void nr2_gain_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  active_receiver->nr2_gain_method=(unsigned int)i;
  SetRXAEMNRgainMethod(active_receiver->id, active_receiver->nr2_gain_method);
}

static void nr2_npe_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  active_receiver->nr2_npe_method=(unsigned int)i;
  SetRXAEMNRnpeMethod(active_receiver->id, active_receiver->nr2_npe_method);
}

static void ae_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    active_receiver->nr2_ae=1;
  } else {
    active_receiver->nr2_ae=0;
  }
  SetRXAEMNRaeRun(active_receiver->id, active_receiver->nr2_ae);
}

void dsp_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - DSP");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *agc_hang_threshold_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(agc_hang_threshold_label), "<b>AGC hang threshold</b>");
  gtk_widget_set_halign(agc_hang_threshold_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),agc_hang_threshold_label,0,1,1,1);

  GtkWidget *agc_hang_threshold_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
  gtk_range_set_increments (GTK_RANGE(agc_hang_threshold_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(agc_hang_threshold_scale),active_receiver->agc_hang_threshold);
  gtk_widget_set_hexpand(agc_hang_threshold_scale, TRUE);
  gtk_grid_attach(GTK_GRID(grid),agc_hang_threshold_scale,1,1,1,1);
  g_signal_connect(G_OBJECT(agc_hang_threshold_scale),"value_changed",G_CALLBACK(agc_hang_threshold_value_changed_cb),NULL);
  gtk_widget_set_name(agc_hang_threshold_scale, "pihpsdr_fill_scale");
  gtk_scale_set_draw_value(GTK_SCALE(agc_hang_threshold_scale), FALSE);
  pihpsdr_fill_scale_install(agc_hang_threshold_scale);

  GtkWidget *pre_post_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(pre_post_label), "<b>NR / NR2 / ANF position</b>");
  gtk_widget_set_halign(pre_post_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),pre_post_label,0,2,1,1);

  GtkWidget *pre_post_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(pre_post_combo), NULL, "Pre AGC");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(pre_post_combo), NULL, "Post AGC");
  gtk_combo_box_set_active(GTK_COMBO_BOX(pre_post_combo), active_receiver->nr_agc ? 1 : 0);
  gtk_widget_set_hexpand(pre_post_combo, TRUE);
  gtk_widget_set_halign(pre_post_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),pre_post_combo,1,2,1,1);
  g_signal_connect(pre_post_combo, "changed", G_CALLBACK(nr_agc_combo_changed), NULL);

  GtkWidget *nr2_gain_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(nr2_gain_label), "<b>NR2 gain method</b>");
  gtk_widget_set_halign(nr2_gain_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),nr2_gain_label,0,3,1,1);

  GtkWidget *nr2_gain_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr2_gain_combo), NULL, "Linear");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr2_gain_combo), NULL, "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr2_gain_combo), NULL, "Gamma");
  gtk_combo_box_set_active(GTK_COMBO_BOX(nr2_gain_combo), active_receiver->nr2_gain_method);
  gtk_widget_set_hexpand(nr2_gain_combo, TRUE);
  gtk_widget_set_halign(nr2_gain_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),nr2_gain_combo,1,3,1,1);
  g_signal_connect(nr2_gain_combo, "changed", G_CALLBACK(nr2_gain_combo_changed), NULL);

  GtkWidget *nr2_npe_method_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(nr2_npe_method_label), "<b>NR2 NPE method</b>");
  gtk_widget_set_halign(nr2_npe_method_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),nr2_npe_method_label,0,4,1,1);

  GtkWidget *nr2_npe_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr2_npe_combo), NULL, "OSMS");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr2_npe_combo), NULL, "MMSE");
  gtk_combo_box_set_active(GTK_COMBO_BOX(nr2_npe_combo), active_receiver->nr2_npe_method);
  gtk_widget_set_hexpand(nr2_npe_combo, TRUE);
  gtk_widget_set_halign(nr2_npe_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),nr2_npe_combo,1,4,1,1);
  g_signal_connect(nr2_npe_combo, "changed", G_CALLBACK(nr2_npe_combo_changed), NULL);

  GtkWidget *ae_b=gtk_check_button_new_with_label("NR2 AE filter");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ae_b), active_receiver->nr2_ae);
  gtk_grid_attach(GTK_GRID(grid),ae_b,0,5,2,1);
  g_signal_connect(ae_b,"toggled",G_CALLBACK(ae_cb),NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

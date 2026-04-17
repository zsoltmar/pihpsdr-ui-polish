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
#include <stdlib.h>
#include <string.h>

#include <wdsp.h>

#include "menu_page.h"
#include "new_menu.h"
#include "noise_menu.h"
#include "channel.h"
#include "band.h"
#include "bandstack.h"
#include "filter.h"
#include "mode.h"
#include "radio.h"
#include "vfo.h"
#include "button_text.h"
#include "ext.h"

static GtkWidget *parent_window=NULL;

static GtkWidget *dialog=NULL;

static GtkWidget *last_filter;

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

void set_noise() {
  SetEXTANBRun(active_receiver->id, active_receiver->nb);
  SetEXTNOBRun(active_receiver->id, active_receiver->nb2);
  SetRXAANRRun(active_receiver->id, active_receiver->nr);
  SetRXAEMNRRun(active_receiver->id, active_receiver->nr2);
  SetRXAANFRun(active_receiver->id, active_receiver->anf);
  SetRXASNBARun(active_receiver->id, active_receiver->snb);
  g_idle_add(ext_vfo_update,NULL);
}

void update_noise() {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_noise(client_socket,active_receiver->id,active_receiver->nb,active_receiver->nb2,active_receiver->nr,active_receiver->nr2,active_receiver->anf,active_receiver->snb);
  } else {
#endif
    set_noise();
#ifdef CLIENT_SERVER
  }
#endif
}

static void nb_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  switch (i) {
    case 0:
      active_receiver->nb=0;
      active_receiver->nb2=0;
      mode_settings[vfo[active_receiver->id].mode].nb=0;
      mode_settings[vfo[active_receiver->id].mode].nb2=0;
      break;
    case 1:
      active_receiver->nb=1;
      active_receiver->nb2=0;
      mode_settings[vfo[active_receiver->id].mode].nb=1;
      mode_settings[vfo[active_receiver->id].mode].nb2=0;
      break;
    case 2:
      active_receiver->nb=0;
      active_receiver->nb2=1;
      mode_settings[vfo[active_receiver->id].mode].nb=0;
      mode_settings[vfo[active_receiver->id].mode].nb2=1;
      break;
    default:
      return;
  }
  update_noise();
}

static void nr_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  switch (i) {
    case 0:
      active_receiver->nr=0;
      active_receiver->nr2=0;
      mode_settings[vfo[active_receiver->id].mode].nr=0;
      mode_settings[vfo[active_receiver->id].mode].nr2=0;
      break;
    case 1:
      active_receiver->nr=1;
      active_receiver->nr2=0;
      mode_settings[vfo[active_receiver->id].mode].nr=1;
      mode_settings[vfo[active_receiver->id].mode].nr2=0;
      break;
    case 2:
      active_receiver->nr=0;
      active_receiver->nr2=1;
      mode_settings[vfo[active_receiver->id].mode].nr=0;
      mode_settings[vfo[active_receiver->id].mode].nr2=1;
      break;
    default:
      return;
  }
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  active_receiver->anf=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  mode_settings[vfo[active_receiver->id].mode].anf=active_receiver->anf;
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  active_receiver->snb=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  mode_settings[vfo[active_receiver->id].mode].snb=active_receiver->snb;
  update_noise();
}

void noise_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Noise (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *nb_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(nb_label), "<b>Noise blanker</b>");
  gtk_widget_set_halign(nb_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),nb_label,0,1,1,1);

  GtkWidget *nb_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "NB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "NB2");
  {
    int sel = 0;
    if (active_receiver->nb2) {
      sel = 2;
    } else if (active_receiver->nb) {
      sel = 1;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(nb_combo), sel);
  }
  gtk_widget_set_hexpand(nb_combo, TRUE);
  gtk_widget_set_halign(nb_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),nb_combo,1,1,1,1);
  g_signal_connect(nb_combo, "changed", G_CALLBACK(nb_combo_changed), NULL);

  GtkWidget *nr_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(nr_label), "<b>Noise reduction</b>");
  gtk_widget_set_halign(nr_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),nr_label,0,2,1,1);

  GtkWidget *nr_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR2");
  {
    int sel = 0;
    if (active_receiver->nr2) {
      sel = 2;
    } else if (active_receiver->nr) {
      sel = 1;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(nr_combo), sel);
  }
  gtk_widget_set_hexpand(nr_combo, TRUE);
  gtk_widget_set_halign(nr_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),nr_combo,1,2,1,1);
  g_signal_connect(nr_combo, "changed", G_CALLBACK(nr_combo_changed), NULL);

  GtkWidget *b_snb=gtk_check_button_new_with_label("SNB");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), active_receiver->snb);
  gtk_grid_attach(GTK_GRID(grid),b_snb,0,3,1,1);
  g_signal_connect(b_snb,"toggled",G_CALLBACK(snb_cb),NULL);

  GtkWidget *b_anf=gtk_check_button_new_with_label("ANF");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), active_receiver->anf);
  gtk_grid_attach(GTK_GRID(grid),b_anf,1,3,1,1);
  g_signal_connect(b_anf,"toggled",G_CALLBACK(anf_cb),NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

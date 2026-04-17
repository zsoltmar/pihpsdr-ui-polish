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
#include <string.h>

#include "menu_page.h"
#include "new_menu.h"
#include "general_menu.h"
#include "band.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"

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

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

/*
static void toolbar_dialog_buttons_cb(GtkWidget *widget, gpointer data) {
  toolbar_dialog_buttons=toolbar_dialog_buttons==1?0:1;
  update_toolbar_labels();
}
*/

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled=mic_ptt_enabled==1?0:1;
}

static void ptt_wiring_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  mic_ptt_tip_bias_ring = i;
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled=mic_bias_enabled==1?0:1;
}

static void apollo_cb(GtkWidget *widget, gpointer data);

static void alex_cb(GtkWidget *widget, gpointer data) {
  if(filter_board==ALEX) {
    filter_board=NONE;
  } else if(filter_board==NONE) {
    filter_board=ALEX;
  } else if(filter_board==APOLLO) {
    GtkWidget *w=(GtkWidget *)data;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    filter_board=ALEX;
  }

  if(protocol==NEW_PROTOCOL) {
    filter_board_changed();
  }

  if(filter_board==ALEX) {
    BAND *band=band_get_current_band();
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(entry->frequency);
    //setMode(entry->mode);
    set_mode(active_receiver,entry->mode);
    FILTER* band_filters=filters[entry->mode];
    FILTER* band_filter=&band_filters[entry->filter];
    //setFilter(band_filter->low,band_filter->high);
    set_filter(active_receiver,band_filter->low,band_filter->high);
    if(active_receiver->id==0) {
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
    }
  }
}

static void apollo_cb(GtkWidget *widget, gpointer data) {
  if(filter_board==APOLLO) {
    filter_board=NONE;
  } else if(filter_board==NONE) {
    filter_board=APOLLO;
  } else if(filter_board==ALEX) {
    GtkWidget *w=(GtkWidget *)data;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), FALSE);
    filter_board=APOLLO;
  }
  if(protocol==NEW_PROTOCOL) {
    filter_board_changed();
  }

  if(filter_board==APOLLO) {
    BAND *band=band_get_current_band();
    BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    setFrequency(entry->frequency);
    //setMode(entry->mode);
    set_mode(active_receiver,entry->mode);
    FILTER* band_filters=filters[entry->mode];
    FILTER* band_filter=&band_filters[entry->filter];
    //setFilter(band_filter->low,band_filter->high);
    set_filter(active_receiver,band_filter->low,band_filter->high);
  }
}

static void general_sample_rate_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  if (protocol == NEW_PROTOCOL) {
#ifndef GPIO
    static const int rates[] = { 48000, 96000, 192000, 384000, 768000, 1536000 };
    if (i >= (int)(sizeof(rates) / sizeof(rates[0]))) {
      return;
    }
    radio_change_sample_rate(rates[i]);
#else
    static const int rates4[] = { 48000, 96000, 192000, 384000 };
    if (i >= (int)(sizeof(rates4) / sizeof(rates4[0]))) {
      return;
    }
    radio_change_sample_rate(rates4[i]);
#endif
    return;
  }
  if (protocol == ORIGINAL_PROTOCOL) {
    static const int rates[] = { 48000, 96000, 192000, 384000 };
    if (i >= (int)(sizeof(rates) / sizeof(rates[0]))) {
      return;
    }
    radio_change_sample_rate(rates[i]);
  }
}

#ifdef SOAPYSDR
static void general_soapy_sample_rate_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int soap_rates[] = { 1000000, 2000000 };
  radio_change_sample_rate(soap_rates[i]);
}
#endif

static void rit_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int steps[] = { 1, 10, 100 };
  rit_increment = steps[i];
}

void general_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - General");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *vfo_divisor_label=gtk_label_new("VFO Encoder Divisor: ");
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,4,1,1,1);

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,4,2,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL){

    if((protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION) ||
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION2) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION2)) {

      GtkWidget *ptt_wire_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(ptt_wire_label), "<b>Headset PTT wiring</b>");
      gtk_widget_set_halign(ptt_wire_label, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid),ptt_wire_label,3,1,1,1);

      GtkWidget *ptt_wire_combo=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ptt_wire_combo), NULL, "PTT on ring; mic + bias on tip");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ptt_wire_combo), NULL, "PTT on tip; mic + bias on ring");
      gtk_combo_box_set_active(GTK_COMBO_BOX(ptt_wire_combo), mic_ptt_tip_bias_ring ? 1 : 0);
      gtk_widget_set_hexpand(ptt_wire_combo, TRUE);
      gtk_widget_set_halign(ptt_wire_combo, GTK_ALIGN_FILL);
      gtk_grid_attach(GTK_GRID(grid),ptt_wire_combo,3,2,1,1);
      g_signal_connect(ptt_wire_combo, "changed", G_CALLBACK(ptt_wiring_combo_changed), NULL);

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_grid_attach(GTK_GRID(grid),ptt_b,3,3,1,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);

      GtkWidget *bias_b=gtk_check_button_new_with_label("Bias enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_grid_attach(GTK_GRID(grid),bias_b,3,4,1,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
    }
 
    GtkWidget *alex_b=gtk_check_button_new_with_label("ALEX");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alex_b), filter_board==ALEX);
    gtk_grid_attach(GTK_GRID(grid),alex_b,1,1,1,1);

    GtkWidget *apollo_b=gtk_check_button_new_with_label("APOLLO");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apollo_b), filter_board==APOLLO);
    gtk_grid_attach(GTK_GRID(grid),apollo_b,1,2,1,1);

    g_signal_connect(alex_b,"toggled",G_CALLBACK(alex_cb),apollo_b);
    g_signal_connect(apollo_b,"toggled",G_CALLBACK(apollo_cb),alex_b);
  }

  GtkWidget *sample_rate_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample rate</b>");
  gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),sample_rate_label,0,1,1,1);

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL){
    GtkWidget *sample_rate_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "48000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "96000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "192000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "384000");
    if(protocol==NEW_PROTOCOL) {
#ifndef GPIO
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "768000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "1536000");
#endif
    }
    {
      int sel = 0;
      int j;
      int nopts;
#ifndef GPIO
      nopts = (protocol == NEW_PROTOCOL) ? 6 : 4;
#else
      nopts = 4;
#endif
      static const int rates_lookup[] = { 48000, 96000, 192000, 384000, 768000, 1536000 };
      for (j = 0; j < nopts; j++) {
        if (sample_rate == rates_lookup[j]) {
          sel = j;
          break;
        }
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo), sel);
    }
    gtk_widget_set_hexpand(sample_rate_combo, TRUE);
    gtk_widget_set_halign(sample_rate_combo, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_combo,1,1,1,1);
    g_signal_connect(sample_rate_combo, "changed", G_CALLBACK(general_sample_rate_combo_changed), NULL);
  }

#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    GtkWidget *sample_rate_label_soapy=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sample_rate_label_soapy), "<b>Sample rate</b>");
    gtk_widget_set_halign(sample_rate_label_soapy, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_label_soapy,0,1,1,1);

    GtkWidget *sample_rate_soapy_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_soapy_combo), NULL, "1000000");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_soapy_combo), NULL, "2000000");
    gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_soapy_combo), sample_rate==2000000 ? 1 : 0);
    gtk_widget_set_hexpand(sample_rate_soapy_combo, TRUE);
    gtk_widget_set_halign(sample_rate_soapy_combo, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid),sample_rate_soapy_combo,1,1,1,1);
    g_signal_connect(sample_rate_soapy_combo, "changed", G_CALLBACK(general_soapy_sample_rate_combo_changed), NULL);
  }
#endif


  GtkWidget *rit_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rit_label), "<b>RIT / XIT step</b>");
  gtk_widget_set_halign(rit_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),rit_label,5,1,1,1);

  GtkWidget *rit_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "1 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "10 Hz");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "100 Hz");
  {
    int sel = 0;
    if (rit_increment == 10) {
      sel = 1;
    } else if (rit_increment == 100) {
      sel = 2;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(rit_combo), sel);
  }
  gtk_widget_set_hexpand(rit_combo, TRUE);
  gtk_widget_set_halign(rit_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),rit_combo,6,1,1,1);
  g_signal_connect(rit_combo, "changed", G_CALLBACK(rit_combo_changed), NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}


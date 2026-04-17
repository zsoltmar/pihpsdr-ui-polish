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

#include "main.h"
#include "discovered.h"
#include "menu_page.h"
#include "new_menu.h"
#include "radio_menu.h"
#include "adc.h"
#include "band.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"
#include "old_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "actions.h"
#ifdef GPIO
#include "gpio.h"
#endif
#include "vfo.h"
#include "ext.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *rx_gains[3];
static GtkWidget *tx_gain;
static GtkWidget *tx_gains[2];
static GtkWidget *sat_b;
static GtkWidget *rsat_b;

static GtkWidget *receivers_combo;
static GtkWidget *duplex_b;
static GtkWidget *mute_rx_b;

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

#ifdef SOAPYSDR
static void rf_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *adc=(ADC *)data;
  adc->gain=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_gain(receiver[0]);
  }
}

static void rx_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *adc=(ADC *)data;
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    adc->gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_gain_element(receiver[0],(char *)gtk_widget_get_name(widget),adc->gain);
/*
    for(int i=0;i<radio->info.soapy.rx_gains;i++) {
      if(strcmp(radio->info.soapy.rx_gain[i],(char *)gtk_widget_get_name(widget))==0) {
        adc[0].rx_gain[i]=gain;
        soapy_protocol_set_gain_element(receiver[0],(char *)gtk_widget_get_name(widget),gain);
        break;
      }
    }
*/
  }
}

static void drive_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  DAC *dac=(DAC *)data;
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    transmitter->drive=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_tx_gain(transmitter,(double)transmitter->drive);
/*
    for(int i=0;i<radio->info.soapy.tx_gains;i++) {
      int value=soapy_protocol_get_tx_gain_element(transmitter,radio->info.soapy.tx_gain[i]);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gains[i]),(double)value);
    }
*/
  }
}

static void tx_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  DAC *dac=(DAC *)data;
  int gain;
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_tx_gain_element(transmitter,(char *)gtk_widget_get_name(widget),gain);
/*
    for(int i=0;i<radio->info.soapy.tx_gains;i++) {
      if(strcmp(radio->info.soapy.tx_gain[i],(char *)gtk_widget_get_name(widget))==0) {
        dac[0].tx_gain[i]=gain;
        break;
      }
    }
*/
  }
}


static void agc_changed_cb(GtkWidget *widget, gpointer data) {
  ADC *adc=(ADC *)data;
  gboolean agc=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  soapy_protocol_set_automatic_gain(active_receiver,agc);
  if(!agc) {
    soapy_protocol_set_gain(active_receiver);
  } 
}

/*
static void dac0_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
  DAC *dac=(DAC *)data;
  int gain;
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    gain=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    soapy_protocol_set_tx_gain_element(radio->transmitter,(char *)gtk_widget_get_name(widget),gain);
    for(int i=0;i<radio->discovered->info.soapy.tx_gains;i++) {
      if(strcmp(radio->discovered->info.soapy.tx_gain[i],(char *)gtk_widget_get_name(widget))==0) {
        radio->dac[0].tx_gain[i]=gain;
        break;
      }
    }
  }
}
*/
#endif

static void calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  calibration_ppm=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

static void rx_gain_calibration_value_changed_cb(GtkWidget *widget, gpointer data) {
  rx_gain_calibration=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void vfo_divisor_value_changed_cb(GtkWidget *widget, gpointer data) {
  vfo_encoder_divisor=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

#ifdef GPIO
static void gpio_settle_value_changed_cb(GtkWidget *widget, gpointer data) {
  settle_time=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}
#endif

/*
static void toolbar_dialog_buttons_cb(GtkWidget *widget, gpointer data) {
  toolbar_dialog_buttons=toolbar_dialog_buttons==1?0:1;
  update_toolbar_labels();
}
*/

static void ptt_cb(GtkWidget *widget, gpointer data) {
  mic_ptt_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void ptt_ring_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=0;
  }
}

static void ptt_tip_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    mic_ptt_tip_bias_ring=1;
  }
}

static void bias_cb(GtkWidget *widget, gpointer data) {
  mic_bias_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void iqswap_cb(GtkWidget *widget, gpointer data) {
  iqswap=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void split_cb(GtkWidget *widget, gpointer data) {
  int new=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if (new != split) g_idle_add(ext_split_toggle, NULL);
}

//
// call-able from outside, e.g. toolbar or MIDI, through g_idle_add
//
void setDuplex() {
  if(duplex) {
    gtk_container_remove(GTK_CONTAINER(fixed),transmitter->panel);
    reconfigure_transmitter(transmitter,display_width/4,display_height/2);
    create_dialog(transmitter);
  } else {
    GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(transmitter->dialog));
    gtk_container_remove(GTK_CONTAINER(content),transmitter->panel);
    gtk_widget_destroy(transmitter->dialog);
    transmitter->dialog=NULL;
    reconfigure_transmitter(transmitter,display_width,rx_height);
  }
  g_idle_add(ext_vfo_update, NULL);
}

static void PA_enable_cb(GtkWidget *widget, gpointer data) {
  pa_enabled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void duplex_cb(GtkWidget *widget, gpointer data) {
  if (isTransmitting()) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
    return;
  }
  duplex=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  setDuplex();
}

static void mute_rx_cb(GtkWidget *widget, gpointer data) {
  mute_rx_while_transmitting=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void sat_cb(GtkWidget *widget, gpointer data) {
  sat_mode=gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  g_idle_add(ext_vfo_update, NULL);
}

void load_filters(void) {
  if(filter_board==N2ADR) {
    // set OC filters
    BAND *band;
    band=band_get_band(band160);
    band->OCrx=band->OCtx=1;
    band=band_get_band(band80);
    band->OCrx=band->OCtx=66;
    band=band_get_band(band60);
    band->OCrx=band->OCtx=68;
    band=band_get_band(band40);
    band->OCrx=band->OCtx=68;
    band=band_get_band(band30);
    band->OCrx=band->OCtx=72;
    band=band_get_band(band20);
    band->OCrx=band->OCtx=72;
    band=band_get_band(band17);
    band->OCrx=band->OCtx=80;
    band=band_get_band(band15);
    band->OCrx=band->OCtx=80;
    band=band_get_band(band12);
    band->OCrx=band->OCtx=96;
    band=band_get_band(band10);
    band->OCrx=band->OCtx=96;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    return;
  }

  if(protocol==NEW_PROTOCOL) {
    filter_board_changed();
  }

  if(filter_board==ALEX || filter_board==APOLLO) {
    BAND *band=band_get_current_band();
    // mode and filters have nothing to do with the filter board
    //BANDSTACK_ENTRY* entry=bandstack_entry_get_current();
    //setFrequency(entry->frequency);
    //setMode(entry->mode);
    //set_mode(active_receiver,entry->mode);
    //FILTER* band_filters=filters[entry->mode];
    //FILTER* band_filter=&band_filters[entry->filter];
    //set_filter(active_receiver,band_filter->low,band_filter->high);
    if(active_receiver->id==0) {
      set_alex_rx_antenna(band->alexRxAntenna);
      set_alex_tx_antenna(band->alexTxAntenna);
      set_alex_attenuation(band->alexAttenuation);
    }
  }
  att_type_changed();
}

static void none_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    filter_board = NONE;
    load_filters();
  }
}

static void alex_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    filter_board = ALEX;
    load_filters();
  }
}

static void apollo_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    filter_board = APOLLO;
    load_filters();
  }
}

static void charly25_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    filter_board = CHARLY25;
    load_filters();
  }
}

static void n2adr_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    filter_board = N2ADR;
    load_filters();
  }
}


static void radio_menu_apply_sample_rate(int rate) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_sample_rate(client_socket,-1,rate);
  } else {
#endif
    radio_change_sample_rate(rate);
#ifdef CLIENT_SERVER
  }
#endif
}

static void sample_rate_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    radio_menu_apply_sample_rate(GPOINTER_TO_INT(data));
  }
}

static void radio_menu_orig_sr_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int rates[] = { 48000, 96000, 192000, 384000 };
  if (i >= (int)(sizeof(rates) / sizeof(rates[0]))) {
    return;
  }
  radio_menu_apply_sample_rate(rates[i]);
}

static void receivers_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  if (isTransmitting()) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(receivers_combo), receivers == 1 ? 0 : 1);
    return;
  }
  int n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (n < 0) {
    return;
  }
  int val = n + 1;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_receivers(client_socket,val);
  } else {
#endif
    radio_change_receivers(val);
#ifdef CLIENT_SERVER
  }
#endif
}

static void region_cb(GtkWidget *widget, gpointer data) {
  radio_change_region(gtk_combo_box_get_active (GTK_COMBO_BOX(widget)));
}

static void rit_cb(GtkWidget *widget,gpointer data) {
  rit_increment=GPOINTER_TO_INT(data);
}

static void radio_menu_rit_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int steps[] = { 1, 10, 100 };
  rit_increment = steps[i];
}

static void ck10mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_10mhz = GPOINTER_TO_INT(data);
}

static void radio_menu_ck10mhz_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  atlas_clock_source_10mhz = i;
}

static void radio_menu_ptt_wiring_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  mic_ptt_tip_bias_ring = i;
}

static void radio_menu_filter_board_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int boards[] = { NONE, ALEX, APOLLO, CHARLY25, N2ADR };
  if (i >= (int)(sizeof(boards) / sizeof(boards[0]))) {
    return;
  }
  filter_board = boards[i];
  load_filters();
}

static void ck128mhz_cb(GtkWidget *widget, gpointer data) {
  atlas_clock_source_128mhz=atlas_clock_source_128mhz==1?0:1;
}

static void micsource_cb(GtkWidget *widget, gpointer data) {
  atlas_mic_source=atlas_mic_source==1?0:1;
}

static void penelopetx_cb(GtkWidget *widget, gpointer data) {
  atlas_penelope=atlas_penelope==1?0:1;
}

static void janus_cb(GtkWidget *widget, gpointer data) {
  atlas_janus=atlas_janus==1?0:1;
}

#ifdef SOAPYSDR
static void radio_menu_soapy_sr_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (idx < 0) {
    return;
  }
  int rates[32];
  int n_r = 0;
  rates[n_r++] = radio->info.soapy.sample_rate;
  {
    int r = rates[0] / 2;
    while (r >= 48000 && n_r < (int)(sizeof(rates) / sizeof(rates[0]))) {
      rates[n_r++] = r;
      r /= 2;
    }
  }
  if (idx >= n_r) {
    return;
  }
  radio_menu_apply_sample_rate(rates[idx]);
}
#endif

void radio_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Radio");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),5);
  gtk_grid_set_row_spacing (GTK_GRID(grid),5);
  gtk_grid_set_column_homogeneous (GTK_GRID(grid), FALSE);
  gtk_grid_set_row_homogeneous (GTK_GRID(grid), FALSE);

  int col=0;
  int row=0;
  int temp_row=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  col++;

  GtkWidget *region_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(region_label), "<b>Region:</b>");
  gtk_grid_attach(GTK_GRID(grid),region_label,col,row,1,1);
  
  col++;

  GtkWidget *region_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"Other");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"UK");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(region_combo),NULL,"WRC15");
  gtk_combo_box_set_active(GTK_COMBO_BOX(region_combo),region);
  gtk_grid_attach(GTK_GRID(grid),region_combo,col,row,1,1);
  g_signal_connect(region_combo,"changed",G_CALLBACK(region_cb),NULL);

  col++;


  row++;
  col=0;

  GtkWidget *receivers_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(receivers_label), "<b>Receivers:</b>");
  gtk_label_set_justify(GTK_LABEL(receivers_label),GTK_JUSTIFY_LEFT);
  gtk_grid_attach(GTK_GRID(grid),receivers_label,col,row,1,1);

  row++;

  receivers_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo), NULL, "1");
  if(radio->supported_receivers>1) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(receivers_combo), NULL, "2");
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(receivers_combo), receivers == 1 ? 0 : 1);
  gtk_widget_set_hexpand(receivers_combo, TRUE);
  gtk_widget_set_halign(receivers_combo, GTK_ALIGN_FILL);
  gtk_grid_attach(GTK_GRID(grid),receivers_combo,col,row,1,1);
  g_signal_connect(receivers_combo,"changed",G_CALLBACK(receivers_combo_changed),NULL);
  row++;

  col++;
  if(row>temp_row) temp_row=row;

  row=1;

  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample rate</b>");
      gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,col,row,1,1);
      row++;

      GtkWidget *sample_rate_combo=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "48000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "96000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "192000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "384000");
      {
        static const int rates[] = { 48000, 96000, 192000, 384000 };
        int sel = 0;
        int j;
        for (j = 0; j < (int)(sizeof(rates) / sizeof(rates[0])); j++) {
          if (active_receiver->sample_rate == rates[j]) {
            sel = j;
            break;
          }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo), sel);
      }
      gtk_widget_set_hexpand(sample_rate_combo, TRUE);
      gtk_widget_set_halign(sample_rate_combo, GTK_ALIGN_FILL);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_combo,col,row,1,1);
      g_signal_connect(sample_rate_combo, "changed", G_CALLBACK(radio_menu_orig_sr_combo_changed), NULL);
      row++;

      col++;
      }
      break;
  
#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      if(strcmp(radio->name,"sdrplay")==0) {
        GtkWidget *sample_rate_combo_box=gtk_combo_box_text_new();
//        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"96000");
//        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"192000");
//        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"384000");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_box),NULL,"768000");
        switch(radio_sample_rate) {
          case 96000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),0);
            break;
          case 192000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),1);
            break;
          case 384000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),2);
            break;
          case 768000:
            gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_box),3);
            break;
        }
        g_signal_connect(sample_rate_combo_box,"changed",G_CALLBACK(sample_rate_cb),radio);
        gtk_grid_attach(GTK_GRID(grid),sample_rate_combo_box,col,row,1,1);
	row++;
      } else {
        GtkWidget *sample_rate_label=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample rate</b>");
        gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid),sample_rate_label,col,row,1,1);
        row++;

        GtkWidget *sample_rate_soapy_combo=gtk_combo_box_text_new();
        {
          char rate_string[16];
          int sel = 0;
          int j = 0;
          int rate = radio->info.soapy.sample_rate;
          sprintf(rate_string,"%d",rate);
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_soapy_combo), NULL, rate_string);
          if (active_receiver->sample_rate == rate) {
            sel = j;
          }
          j++;
          rate = rate / 2;
          while (rate >= 48000) {
            sprintf(rate_string,"%d",rate);
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_soapy_combo), NULL, rate_string);
            if (active_receiver->sample_rate == rate) {
              sel = j;
            }
            j++;
            rate = rate / 2;
          }
          gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_soapy_combo), sel);
        }
        gtk_widget_set_hexpand(sample_rate_soapy_combo, TRUE);
        gtk_widget_set_halign(sample_rate_soapy_combo, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid),sample_rate_soapy_combo,col,row,1,1);
        g_signal_connect(sample_rate_soapy_combo, "changed", G_CALLBACK(radio_menu_soapy_sr_combo_changed), NULL);

        col++;
      }
      break;
#endif

  }
  row++;
  if(row>temp_row) temp_row=row;


  row=1;

  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {

    if((protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION) ||
       (protocol==NEW_PROTOCOL && device==NEW_DEVICE_ORION2) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION) ||
       (protocol==ORIGINAL_PROTOCOL && device==DEVICE_ORION2)) {

      GtkWidget *ptt_wire_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(ptt_wire_label), "<b>Headset PTT wiring</b>");
      gtk_widget_set_halign(ptt_wire_label, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid),ptt_wire_label,col,row,1,1);
      row++;

      GtkWidget *ptt_wire_combo=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ptt_wire_combo), NULL, "PTT ring; mic + bias tip");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ptt_wire_combo), NULL, "PTT tip; mic + bias ring");
      gtk_combo_box_set_active(GTK_COMBO_BOX(ptt_wire_combo), mic_ptt_tip_bias_ring ? 1 : 0);
      gtk_widget_set_hexpand(ptt_wire_combo, TRUE);
      gtk_widget_set_halign(ptt_wire_combo, GTK_ALIGN_FILL);
      gtk_grid_attach(GTK_GRID(grid),ptt_wire_combo,col,row,1,1);
      g_signal_connect(ptt_wire_combo, "changed", G_CALLBACK(radio_menu_ptt_wiring_combo_changed), NULL);
      row++;

      GtkWidget *ptt_b=gtk_check_button_new_with_label("PTT enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ptt_b), mic_ptt_enabled);
      gtk_grid_attach(GTK_GRID(grid),ptt_b,col,row,1,1);
      g_signal_connect(ptt_b,"toggled",G_CALLBACK(ptt_cb),NULL);
      row++;

      GtkWidget *bias_b=gtk_check_button_new_with_label("Bias enabled");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bias_b), mic_bias_enabled);
      gtk_grid_attach(GTK_GRID(grid),bias_b,col,row,1,1);
      g_signal_connect(bias_b,"toggled",G_CALLBACK(bias_cb),NULL);
      row++;

      if(row>temp_row) temp_row=row;
      col++;
    }

    row=1;

    GtkWidget *filter_board_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(filter_board_label), "<b>Filter board</b>");
    gtk_widget_set_halign(filter_board_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),filter_board_label,col,row,1,1);
    row++;

    GtkWidget *filter_board_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_board_combo), NULL, "None");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_board_combo), NULL, "Alex");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_board_combo), NULL, "Apollo");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_board_combo), NULL, "Charly25");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(filter_board_combo), NULL, "N2ADR");
    {
      int sel = 0;
      static const int boards[] = { NONE, ALEX, APOLLO, CHARLY25, N2ADR };
      int j;
      for (j = 0; j < (int)(sizeof(boards) / sizeof(boards[0])); j++) {
        if (filter_board == boards[j]) {
          sel = j;
          break;
        }
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(filter_board_combo), sel);
    }
    gtk_widget_set_hexpand(filter_board_combo, TRUE);
    gtk_widget_set_halign(filter_board_combo, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), filter_board_combo, col, row, 1, 1);
    g_signal_connect(filter_board_combo, "changed", G_CALLBACK(radio_menu_filter_board_combo_changed), NULL);
    row++;

    if(row>temp_row) temp_row=row;
    col++;
  }

  row=1;

  GtkWidget *rit_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(rit_label), "<b>RIT / XIT step (Hz)</b>");
  gtk_widget_set_halign(rit_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid),rit_label,col,row,1,1);
  row++;

  GtkWidget *rit_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "1");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "10");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rit_combo), NULL, "100");
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
  gtk_grid_attach(GTK_GRID(grid),rit_combo,col,row,1,1);
  g_signal_connect(rit_combo, "changed", G_CALLBACK(radio_menu_rit_combo_changed), NULL);
  row++;

  if(row>temp_row) temp_row=row;
  col++;
  row=1;

#ifdef GPIO
  GtkWidget *vfo_divisor_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(vfo_divisor_label), "<b>VFO Encoder Divisor:</b>");
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor_label,col,row,1,1);
  row++;

  GtkWidget *vfo_divisor=gtk_spin_button_new_with_range(1.0,60.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(vfo_divisor),(double)vfo_encoder_divisor);
  gtk_grid_attach(GTK_GRID(grid),vfo_divisor,col,row,1,1);
  g_signal_connect(vfo_divisor,"value_changed",G_CALLBACK(vfo_divisor_value_changed_cb),NULL);
  row++;
#endif
   
  GtkWidget *iqswap_b=gtk_check_button_new_with_label("Swap IQ");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (iqswap_b), iqswap);
  gtk_grid_attach(GTK_GRID(grid),iqswap_b,col,row,1,1);
  g_signal_connect(iqswap_b,"toggled",G_CALLBACK(iqswap_cb),NULL);
  row++;

  if(row>temp_row) temp_row=row;

  col++;

#ifdef USBOZY
  if (protocol==ORIGINAL_PROTOCOL && (device == DEVICE_OZY || device == DEVICE_METIS))
#else
  if (protocol==ORIGINAL_PROTOCOL && radio->device == DEVICE_METIS)
#endif
  {
    row=1;
    GtkWidget *atlas_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(atlas_label), "<b>Atlas bus:</b>");
    gtk_grid_attach(GTK_GRID(grid),atlas_label,col,row,1,1);
    row++;

    GtkWidget *ck10mhz_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ck10mhz_label), "<b>10 MHz clock source</b>");
    gtk_widget_set_halign(ck10mhz_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),ck10mhz_label,col,row,1,1);
    row++;

    GtkWidget *ck10mhz_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Atlas");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Penelope");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ck10mhz_combo), NULL, "Mercury");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ck10mhz_combo), atlas_clock_source_10mhz);
    gtk_widget_set_hexpand(ck10mhz_combo, TRUE);
    gtk_widget_set_halign(ck10mhz_combo, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid),ck10mhz_combo,col,row,1,1);
    g_signal_connect(ck10mhz_combo, "changed", G_CALLBACK(radio_menu_ck10mhz_combo_changed), NULL);
    row++;

    GtkWidget *ck128_b=gtk_check_button_new_with_label("122.88MHz ck=Mercury");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ck128_b), atlas_clock_source_128mhz);
    gtk_grid_attach(GTK_GRID(grid),ck128_b,col,row,1,1);
    g_signal_connect(ck128_b,"toggled",G_CALLBACK(ck128mhz_cb),NULL);
    row++;

    GtkWidget *mic_src_b=gtk_check_button_new_with_label("Mic src=Penelope");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mic_src_b), atlas_mic_source);
    gtk_grid_attach(GTK_GRID(grid),mic_src_b,col,row,1,1);
    g_signal_connect(mic_src_b,"toggled",G_CALLBACK(micsource_cb),NULL);
    row++;

    GtkWidget *pene_tx_b=gtk_check_button_new_with_label("Penelope TX");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pene_tx_b), atlas_penelope);
    gtk_grid_attach(GTK_GRID(grid),pene_tx_b,col,row,1,1);
    g_signal_connect(pene_tx_b,"toggled",G_CALLBACK(penelopetx_cb),NULL);
    row++;

    GtkWidget *janus_b=gtk_check_button_new_with_label("Janus");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (janus_b), atlas_janus);
    gtk_grid_attach(GTK_GRID(grid),janus_b,col,row,1,1);
    g_signal_connect(janus_b,"toggled",G_CALLBACK(janus_cb),NULL);
    row++;

    if(row>temp_row) temp_row=row;
  }

  row=temp_row;
  col=0;

  GtkWidget *split_b=gtk_check_button_new_with_label("Split");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (split_b), split);
  gtk_grid_attach(GTK_GRID(grid),split_b,col,row,1,1);
  g_signal_connect(split_b,"toggled",G_CALLBACK(split_cb),NULL);

  col++;

  duplex_b=gtk_check_button_new_with_label("Duplex");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (duplex_b), duplex);
  gtk_grid_attach(GTK_GRID(grid),duplex_b,col,row,1,1);
  g_signal_connect(duplex_b,"toggled",G_CALLBACK(duplex_cb),NULL);

  col++;

  
  GtkWidget *sat_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"SAT Off");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"SAT");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sat_combo),NULL,"RSAT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(sat_combo),sat_mode);
  gtk_grid_attach(GTK_GRID(grid),sat_combo,col,row,1,1);
  g_signal_connect(sat_combo,"changed",G_CALLBACK(sat_cb),NULL);

  col++;

  mute_rx_b=gtk_check_button_new_with_label("Mute RX when TX");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_rx_b), mute_rx_while_transmitting);
  gtk_grid_attach(GTK_GRID(grid),mute_rx_b,col,row,1,1);
  g_signal_connect(mute_rx_b,"toggled",G_CALLBACK(mute_rx_cb),NULL);

  row++;
  col=0;
 
  GtkWidget *calibration_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(calibration_label), "<b>Frequency\nCalibration(PPM):</b>");
  gtk_grid_attach(GTK_GRID(grid),calibration_label,col,row,1,1);
  col++;

  GtkWidget *calibration_b=gtk_spin_button_new_with_range(-200.0,200.0,0.1);
  gtk_spin_button_set_digits(GTK_SPIN_BUTTON(calibration_b),1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(calibration_b),calibration_ppm);
  gtk_grid_attach(GTK_GRID(grid),calibration_b,col,row,1,1);
  g_signal_connect(calibration_b,"value_changed",G_CALLBACK(calibration_value_changed_cb),NULL);

  if(have_rx_gain) {
    col++;
    GtkWidget *rx_gain_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(rx_gain_label), "<b>RX Gain Calibration:</b>");
    gtk_grid_attach(GTK_GRID(grid),rx_gain_label,col,row,1,1);

    col++;
    GtkWidget *rx_gain_calibration_b=gtk_spin_button_new_with_range(-50.0,50.0,1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gain_calibration_b),(double)rx_gain_calibration);
    gtk_grid_attach(GTK_GRID(grid),rx_gain_calibration_b,col,row,1,1);
    g_signal_connect(rx_gain_calibration_b,"value_changed",G_CALLBACK(rx_gain_calibration_value_changed_cb),NULL);
  }

  col++;
  GtkWidget *PA_enable_b=gtk_check_button_new_with_label("PA enable");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (PA_enable_b), pa_enabled);
  gtk_grid_attach(GTK_GRID(grid),PA_enable_b,col,row,1,1);
  g_signal_connect(PA_enable_b,"toggled",G_CALLBACK(PA_enable_cb),NULL);

  row++;

  if(row>temp_row) temp_row=row;

#ifdef SOAPYSDR
  col=0;
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    int i;
    if(radio->info.soapy.rx_gains>0) {
      GtkWidget *rx_gain=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(rx_gain), "<b>RX Gains:</b>");
      gtk_label_set_justify(GTK_LABEL(rx_gain),GTK_JUSTIFY_LEFT);
      gtk_grid_attach(GTK_GRID(grid),rx_gain,col,row,1,1);
    }

    if(can_transmit) {
      if(radio->info.soapy.tx_gains>0) {
        col=2;
        GtkWidget *tx_gain=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(tx_gain), "<b>TX Gains:</b>");
        gtk_grid_attach(GTK_GRID(grid),tx_gain,col,row,1,1);
      }
    }

    row++;
    temp_row=row;
    col=0;
    if(strcmp(radio->name,"sdrplay")==0 || strcmp(radio->name,"rtlsdr")==0) {
      for(i=0;i<radio->info.soapy.rx_gains;i++) {
        col=0;
        GtkWidget *rx_gain_label=gtk_label_new(radio->info.soapy.rx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid),rx_gain_label,col,row,1,1);
        col++;
        SoapySDRRange range=radio->info.soapy.rx_range[i];
        if(range.step==0.0) {
          range.step=1.0;
        }
        rx_gains[i]=gtk_spin_button_new_with_range(range.minimum,range.maximum,range.step);
        gtk_widget_set_name (rx_gains[i], radio->info.soapy.rx_gain[i]);
        int value=soapy_protocol_get_gain_element(active_receiver,radio->info.soapy.rx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(rx_gains[i]),(double)value);
        gtk_grid_attach(GTK_GRID(grid),rx_gains[i],col,row,1,1);
        g_signal_connect(rx_gains[i],"value_changed",G_CALLBACK(rx_gain_value_changed_cb),&adc[0]);
  
        row++;
      }
    } else {
      // used single gain control - LimeSDR works out best setting for the 3 rx gains
      col=0;
      GtkWidget *rf_gain_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(rf_gain_label), "<b>RF Gain</b>");
      gtk_grid_attach(GTK_GRID(grid),rf_gain_label,col,row,1,1);
      col++;
      double max=100;
      if(strcmp(radio->name,"lime")==0) {
        max=60.0;
      } else if(strcmp(radio->name,"plutosdr")==0) {
        max=73.0;
      }
      GtkWidget *rf_gain_b=gtk_spin_button_new_with_range(0.0,max,1.0);
      //gtk_spin_button_set_value(GTK_SPIN_BUTTON(rf_gain_b),active_receiver->rf_gain);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(rf_gain_b),adc[active_receiver->id].gain);
      gtk_grid_attach(GTK_GRID(grid),rf_gain_b,col,row,1,1);
      g_signal_connect(rf_gain_b,"value_changed",G_CALLBACK(rf_gain_value_changed_cb),&adc[0]);

      row++;
    }

    if(radio->info.soapy.rx_has_automatic_gain) {
      GtkWidget *agc=gtk_check_button_new_with_label("Hardware AGC: ");
      gtk_grid_attach(GTK_GRID(grid),agc,col,row,1,1);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(agc),adc[0].agc);
      g_signal_connect(agc,"toggled",G_CALLBACK(agc_changed_cb),&adc[0]);
      row++;
    }

    row=temp_row;

    if(can_transmit) {
/*
      //tx_gains=g_new(GtkWidget*,radio->info.soapy.tx_gains);
      for(i=0;i<radio->info.soapy.tx_gains;i++) {
        col=2;
        GtkWidget *tx_gain_label=gtk_label_new(radio->info.soapy.tx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid),tx_gain_label,col,row,1,1);
        col++;
        SoapySDRRange range=radio->info.soapy.tx_range[i];
        if(range.step==0.0) {
          range.step=1.0;
        }
        tx_gains[i]=gtk_spin_button_new_with_range(range.minimum,range.maximum,range.step);
        gtk_widget_set_name (tx_gains[i], radio->info.soapy.tx_gain[i]);
        int value=soapy_protocol_get_tx_gain_element(transmitter,radio->info.soapy.tx_gain[i]);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gains[i]),(double)value);
        //gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gains[i]),(double)dac[0].tx_gain[i]);
        gtk_grid_attach(GTK_GRID(grid),tx_gains[i],col,row,1,1);
        g_signal_connect(tx_gains[i],"value_changed",G_CALLBACK(tx_gain_value_changed_cb),&dac[0]);

        gtk_widget_set_sensitive(tx_gains[i], FALSE);

        row++;
      }
*/
      // used single gain control - LimeSDR works out best setting for the 3 rx gains
      col=2;
      GtkWidget *tx_gain_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(tx_gain_label), "<b>TX Gain</b>");
      gtk_grid_attach(GTK_GRID(grid),tx_gain_label,col,row,1,1);
      col++;
      double max=100;
      if(strcmp(radio->name,"lime")==0) {
        max=64.0;
      } else if(strcmp(radio->name,"plutosdr")==0) {
        max=89.0;
      }
      tx_gain=gtk_spin_button_new_with_range(0.0,max,1.0);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(tx_gain),transmitter->drive);
      gtk_grid_attach(GTK_GRID(grid),tx_gain,col,row,1,1);
      g_signal_connect(tx_gain,"value_changed",G_CALLBACK(drive_gain_value_changed_cb),&adc[0]);
    }
  }
#endif


  gtk_container_add(GTK_CONTAINER(content),grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}

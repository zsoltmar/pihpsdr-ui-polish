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
#include "ant_menu.h"
#include "band.h"
#include "radio.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif


static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *grid=NULL;
static GtkWidget *adc0_antenna_combo_box;
static GtkWidget *dac0_antenna_combo_box;

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

static void rx_ant_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    int b=(GPOINTER_TO_UINT(data))>>4;
    int ant=(GPOINTER_TO_UINT(data))&0xF;
    BAND *band=band_get_band(b);
    band->alexRxAntenna=ant;
    if(active_receiver->id==0) {
      set_alex_rx_antenna(ant);
    }
  }
}

static void rx_ant_band_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)combo;
  int band_idx = GPOINTER_TO_INT(data);
  int ant = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (ant < 0) {
    return;
  }
  BAND *band = band_get_band(band_idx);
  band->alexRxAntenna = ant;
  if (active_receiver->id == 0) {
    set_alex_rx_antenna(ant);
  }
}

static void adc0_antenna_cb(GtkComboBox *widget,gpointer data) {
  ADC *adc=(ADC *)data;
  adc->antenna=gtk_combo_box_get_active(widget);
  if(radio->protocol==NEW_PROTOCOL) {
    schedule_high_priority();
#ifdef SOAPYSDR
  } else if(radio->device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_rx_antenna(receiver[0],adc[0].antenna);
#endif
  }
}

static void dac0_antenna_cb(GtkComboBox *widget,gpointer data) {
  DAC *dac=(DAC *)data;
  dac->antenna=gtk_combo_box_get_active(widget);
  if(radio->protocol==NEW_PROTOCOL) {
    schedule_high_priority();
#ifdef SOAPYSDR
  } else if(radio->device==SOAPYSDR_USB_DEVICE) {
    soapy_protocol_set_tx_antenna(transmitter,dac->antenna);
#endif
  }
}

static void tx_ant_cb(GtkToggleButton *widget, gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    int b=(GPOINTER_TO_UINT(data))>>4;
    int ant=(GPOINTER_TO_UINT(data))&0xF;
    BAND *band=band_get_band(b);
    band->alexTxAntenna=ant;
    if(active_receiver->id==0) {
      set_alex_tx_antenna(ant);
    }
  }
}

static void tx_ant_band_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)combo;
  int band_idx = GPOINTER_TO_INT(data);
  int ant = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (ant < 0) {
    return;
  }
  BAND *band = band_get_band(band_idx);
  band->alexTxAntenna = ant;
  if (active_receiver->id == 0) {
    set_alex_tx_antenna(ant);
  }
}

static void show_hf() {
  int i;
  int bands=BANDS;
  switch(protocol) {
    case ORIGINAL_PROTOCOL:
      switch(device) {
        case DEVICE_HERMES_LITE:
        case DEVICE_HERMES_LITE2:
          bands=band10+1;
          break;
        default:
          bands=band6+1;
          break;
      }
      break;
    case NEW_PROTOCOL:
      switch(device) {
        case NEW_DEVICE_HERMES_LITE:
        case NEW_DEVICE_HERMES_LITE2:
          bands=band10+1;
          break;
        default:
          bands=band6+1;
          break;
      }
      break;
  }
    for(i=0;i<bands;i++) {
      BAND *band=band_get_band(i);
      if(strlen(band->title)>0) {
        GtkWidget *band_label=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(band_label), band->title);
        gtk_widget_show(band_label);
        gtk_grid_attach(GTK_GRID(grid),band_label,0,i+2,1,1);

        GtkWidget *rx_combo=gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "3");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "Ext1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "Ext2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "XVTR");
        gtk_combo_box_set_active(GTK_COMBO_BOX(rx_combo), band->alexRxAntenna);
        gtk_widget_set_hexpand(rx_combo, TRUE);
        gtk_widget_show(rx_combo);
        gtk_grid_attach(GTK_GRID(grid),rx_combo,1,i+2,6,1);
        g_signal_connect(rx_combo,"changed",G_CALLBACK(rx_ant_band_combo_changed),GINT_TO_POINTER(i));

        GtkWidget *tx_combo=gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "3");
        gtk_combo_box_set_active(GTK_COMBO_BOX(tx_combo), band->alexTxAntenna);
        gtk_widget_set_hexpand(tx_combo, TRUE);
        gtk_widget_show(tx_combo);
        gtk_grid_attach(GTK_GRID(grid),tx_combo,8,i+2,3,1);
        g_signal_connect(tx_combo,"changed",G_CALLBACK(tx_ant_band_combo_changed),GINT_TO_POINTER(i));
      }
    }
}

static void show_xvtr() {
  int i;
    for(i=0;i<XVTRS;i++) {
      BAND *band=band_get_band(BANDS+i);
      if(strlen(band->title)>0) {
        GtkWidget *band_label=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(band_label), band->title);
        gtk_widget_show(band_label);
        gtk_grid_attach(GTK_GRID(grid),band_label,0,i+2,1,1);

        {
        int bi = i + BANDS;
        GtkWidget *rx_combo=gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "3");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "Ext1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "Ext2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(rx_combo), NULL, "XVTR");
        gtk_combo_box_set_active(GTK_COMBO_BOX(rx_combo), band->alexRxAntenna);
        gtk_widget_set_hexpand(rx_combo, TRUE);
        gtk_widget_show(rx_combo);
        gtk_grid_attach(GTK_GRID(grid),rx_combo,1,i+2,6,1);
        g_signal_connect(rx_combo,"changed",G_CALLBACK(rx_ant_band_combo_changed),GINT_TO_POINTER(bi));

        GtkWidget *tx_combo=gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "2");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tx_combo), NULL, "3");
        gtk_combo_box_set_active(GTK_COMBO_BOX(tx_combo), band->alexTxAntenna);
        gtk_widget_set_hexpand(tx_combo, TRUE);
        gtk_widget_show(tx_combo);
        gtk_grid_attach(GTK_GRID(grid),tx_combo,8,i+2,3,1);
        g_signal_connect(tx_combo,"changed",G_CALLBACK(tx_ant_band_combo_changed),GINT_TO_POINTER(bi));
        }
      }
    }
}

static void band_scope_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  int i;
  if (sel == 0) {
    for(i=XVTRS-1;i>=0;i--) {
      gtk_grid_remove_row (GTK_GRID(grid),i+2);
    }
    show_hf();
  } else if (sel == 1) {
    for(i=BANDS-1;i>=0;i--) {
      gtk_grid_remove_row (GTK_GRID(grid),i+2);
    }
    show_xvtr();
  }
}

static void newpa_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    new_pa_board=1;
  } else {
    new_pa_board=0;
  }
}

void ant_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - ANT");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  GtkWidget *scope_combo=NULL;

#ifdef SOAPYSDR
  if(radio->device!=SOAPYSDR_USB_DEVICE) {
#endif
    scope_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(scope_combo), NULL, "HF bands");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(scope_combo), NULL, "XVTR");
    gtk_combo_box_set_active(GTK_COMBO_BOX(scope_combo), 0);
    gtk_widget_set_hexpand(scope_combo, TRUE);
    gtk_grid_attach(GTK_GRID(grid),scope_combo,1,0,2,1);

#ifdef SOAPYSDR
  }
#endif

  if ((protocol == NEW_PROTOCOL      && (device == NEW_DEVICE_HERMES || device == NEW_DEVICE_ANGELIA || device == NEW_DEVICE_ORION)) ||
      (protocol == ORIGINAL_PROTOCOL && (device == DEVICE_HERMES     || device == DEVICE_ANGELIA     || device == DEVICE_ORION))) {

      //
      // ANAN-100/200: There is an "old" (Rev. 15/16) and "new" (Rev. 24) PA board
      //               around which differs in relay settings for using EXT1,2 and
      //               differs in how to do PS feedback.
      //
      GtkWidget *new_pa_b = gtk_check_button_new_with_label("ANAN 100/200 new PA board");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(new_pa_b), new_pa_board);
      gtk_grid_attach(GTK_GRID(grid), new_pa_b, 3, 0, 1, 1);
      g_signal_connect(new_pa_b, "toggled", G_CALLBACK(newpa_cb), NULL);
  }


  if(protocol==ORIGINAL_PROTOCOL || protocol==NEW_PROTOCOL) {
    GtkWidget *band_col_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(band_col_label), "<b>Band</b>");
    gtk_widget_show(band_col_label);
    gtk_grid_attach(GTK_GRID(grid),band_col_label,0,1,1,1);

    GtkWidget *rx_hdr=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(rx_hdr), "<b>RX antenna</b>");
    gtk_widget_show(rx_hdr);
    gtk_grid_attach(GTK_GRID(grid),rx_hdr,1,1,6,1);

    GtkWidget *tx_hdr=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tx_hdr), "<b>TX antenna</b>");
    gtk_widget_show(tx_hdr);
    gtk_grid_attach(GTK_GRID(grid),tx_hdr,8,1,3,1);

    show_hf();
  }

  if (scope_combo != NULL) {
    g_signal_connect(scope_combo,"changed",G_CALLBACK(band_scope_combo_changed),NULL);
  }

#ifdef SOAPYSDR
  if(radio->device==SOAPYSDR_USB_DEVICE) {
    int i;

g_print("rx_antennas=%ld\n",radio->info.soapy.rx_antennas);
    if(radio->info.soapy.rx_antennas>0) {
      GtkWidget *antenna_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(antenna_label), "<b>RX Antenna:</b>");
      gtk_grid_attach(GTK_GRID(grid),antenna_label,0,1,1,1);
      adc0_antenna_combo_box=gtk_combo_box_text_new();

      for(i=0;i<radio->info.soapy.rx_antennas;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(adc0_antenna_combo_box),NULL,radio->info.soapy.rx_antenna[i]);
      }

      gtk_combo_box_set_active(GTK_COMBO_BOX(adc0_antenna_combo_box),adc[0].antenna);
      g_signal_connect(adc0_antenna_combo_box,"changed",G_CALLBACK(adc0_antenna_cb),&adc[0]);
      gtk_grid_attach(GTK_GRID(grid),adc0_antenna_combo_box,1,1,1,1);
    }

    if(can_transmit) {
      g_print("tx_antennas=%ld\n",radio->info.soapy.tx_antennas);
      if(radio->info.soapy.tx_antennas>0) {
        GtkWidget *antenna_label=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(antenna_label), "<b>TX Antenna:</b>");
        gtk_grid_attach(GTK_GRID(grid),antenna_label,0,2,1,1);
        dac0_antenna_combo_box=gtk_combo_box_text_new();
  
        for(i=0;i<radio->info.soapy.tx_antennas;i++) {
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dac0_antenna_combo_box),NULL,radio->info.soapy.tx_antenna[i]);
        }
  
        gtk_combo_box_set_active(GTK_COMBO_BOX(dac0_antenna_combo_box),dac[0].antenna);
        g_signal_connect(dac0_antenna_combo_box,"changed",G_CALLBACK(dac0_antenna_cb),&dac[0]);
        gtk_grid_attach(GTK_GRID(grid),dac0_antenna_combo_box,1,2,1,1);
      }
    }

  }
#endif

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}


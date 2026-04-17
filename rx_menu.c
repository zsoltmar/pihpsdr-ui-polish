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
#include <string.h>

#include "audio.h"
#include "menu_page.h"
#include "new_menu.h"
#include "rx_menu.h"
#include "band.h"
#include "discovered.h"
#include "filter.h"
#include "radio.h"
#include "receiver.h"
#include "sliders.h"
#include "new_protocol.h"

static GtkWidget *parent_window=NULL;
static GtkWidget *menu_b=NULL;
static GtkWidget *dialog=NULL;
static GtkWidget *local_audio_b=NULL;
static GtkWidget *output=NULL;

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

static void dither_cb(GtkWidget *widget, gpointer data) {
  active_receiver->dither=active_receiver->dither==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void random_cb(GtkWidget *widget, gpointer data) {
  active_receiver->random=active_receiver->random==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void preamp_cb(GtkWidget *widget, gpointer data) {
  active_receiver->preamp=active_receiver->preamp==1?0:1;
  if (protocol == NEW_PROTOCOL) {
    schedule_high_priority();
  }
}

static void alex_att_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    set_alex_attenuation((intptr_t) data);
    update_att_preamp();
  }
}

static void alex_att_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  set_alex_attenuation(i);
  update_att_preamp();
}

static void sample_rate_cb(GtkToggleButton *widget,gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    receiver_change_sample_rate(active_receiver,GPOINTER_TO_INT(data));
  }
}

static void rx_new_protocol_sr_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
#ifndef raspberrypi
  static const int rates[] = { 48000, 96000, 192000, 384000, 768000, 1536000 };
  if (i >= (int)(sizeof(rates) / sizeof(rates[0]))) {
    return;
  }
#else
  static const int rates[] = { 48000, 96000, 192000, 384000 };
  if (i >= (int)(sizeof(rates) / sizeof(rates[0]))) {
    return;
  }
#endif
  receiver_change_sample_rate(active_receiver, rates[i]);
}

#ifdef SOAPYSDR
static void rx_soapy_sr_combo_changed(GtkComboBox *combo, gpointer data) {
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
  receiver_change_sample_rate(active_receiver, rates[idx]);
}
#endif

static void adc_cb(GtkToggleButton *widget,gpointer data) {
  if(gtk_toggle_button_get_active(widget)) {
    receiver_change_adc(active_receiver,GPOINTER_TO_INT(data));
  }
}

static void adc_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  receiver_change_adc(active_receiver, i);
}

static void local_audio_cb(GtkWidget *widget, gpointer data) {
fprintf(stderr,"local_audio_cb: rx=%d\n",active_receiver->id);

  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }

  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(output));
  active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
  strcpy(active_receiver->audio_name,output_devices[i].name);

  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    if(audio_open_output(active_receiver)==0) {
      active_receiver->local_audio=1;
    } else {
fprintf(stderr,"local_audio_cb: audio_open_output failed\n");
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  } else {
    if(active_receiver->local_audio) {
      active_receiver->local_audio=0;
      audio_close_output(active_receiver);
    }
  }
fprintf(stderr,"local_audio_cb: local_audio=%d\n",active_receiver->local_audio);
}

static void mute_audio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_when_not_active=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void mute_radio_cb(GtkWidget *widget, gpointer data) {
  active_receiver->mute_radio=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

//
// possible the device has been changed:
// call audo_close_output with old device, audio_open_output with new one
//
static void local_output_changed_cb(GtkWidget *widget, gpointer data) {
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  fprintf(stderr,"local_output_changed rx=%d %s\n",active_receiver->id,output_devices[i].name);
  if(active_receiver->local_audio) {
    audio_close_output(active_receiver);                     // audio_close with OLD device
  }
    
  if(active_receiver->audio_name!=NULL) {
    g_free(active_receiver->audio_name);
    active_receiver->audio_name=NULL;
  }
  
  if(i>=0) {
    active_receiver->audio_name=g_new(gchar,strlen(output_devices[i].name)+1);
    strcpy(active_receiver->audio_name,output_devices[i].name);
    //active_receiver->audio_device=output_devices[i].index;  // update rx to NEW device
  }

  if(active_receiver->local_audio) {
    if(audio_open_output(active_receiver)<0) {              // audio_open with NEW device
      active_receiver->local_audio=0;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (local_audio_b),FALSE);
    }
  }
  fprintf(stderr,"local_output_changed rx=%d local_audio=%d\n",active_receiver->id,active_receiver->local_audio);
}

static void audio_channel_cb(GtkWidget *widget, gpointer data) {
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    active_receiver->audio_channel=GPOINTER_TO_INT(data);
  }
}

static void audio_channel_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0) {
    return;
  }
  static const int chmap[] = { STEREO, LEFT, RIGHT };
  if (i >= (int)(sizeof(chmap) / sizeof(chmap[0]))) {
    return;
  }
  active_receiver->audio_channel = chmap[i];
}

void rx_menu(GtkWidget *parent) {
  char label[32];
  int i;
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  char title[64];
  sprintf(title,"piHPSDR - Receive (RX %d VFO %s)",active_receiver->id,active_receiver->id==0?"A":"B");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);
  gtk_grid_set_column_spacing (GTK_GRID(grid),10);

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "button_press_event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,0,0,1,1);

  int x=0;

  switch(protocol) {
    case NEW_PROTOCOL:
      {
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample rate</b>");
      gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,1,1,1);

      GtkWidget *sample_rate_combo=gtk_combo_box_text_new();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "48000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "96000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "192000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "384000");
#ifndef raspberrypi
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "768000");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo), NULL, "1536000");
#endif
      {
        int sel = 0;
#ifndef raspberrypi
        static const int rates[] = { 48000, 96000, 192000, 384000, 768000, 1536000 };
        int nopts = (int)(sizeof(rates) / sizeof(rates[0]));
#else
        static const int rates[] = { 48000, 96000, 192000, 384000 };
        int nopts = (int)(sizeof(rates) / sizeof(rates[0]));
#endif
        int j;
        for (j = 0; j < nopts; j++) {
          if (active_receiver->sample_rate == rates[j]) {
            sel = j;
            break;
          }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo), sel);
      }
      gtk_widget_set_hexpand(sample_rate_combo, TRUE);
      gtk_widget_set_halign(sample_rate_combo, GTK_ALIGN_FILL);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_combo,x,2,1,1);
      g_signal_connect(sample_rate_combo, "changed", G_CALLBACK(rx_new_protocol_sr_combo_changed), NULL);
      }
      x++;
      break;

#ifdef SOAPYSDR
    case SOAPYSDR_PROTOCOL:
      {
      int row=1;
      GtkWidget *sample_rate_label=gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(sample_rate_label), "<b>Sample rate</b>");
      gtk_widget_set_halign(sample_rate_label, GTK_ALIGN_START);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_label,x,row,1,1);
      row++;

      GtkWidget *sample_rate_combo_soapy=gtk_combo_box_text_new();
      {
        char rate_string[16];
        int sel = 0;
        int j = 0;
        int rate = radio->info.soapy.sample_rate;
        sprintf(rate_string,"%d",rate);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_soapy), NULL, rate_string);
        if (active_receiver->sample_rate == rate) {
          sel = j;
        }
        j++;
        rate = rate / 2;
        while (rate >= 48000) {
          sprintf(rate_string,"%d",rate);
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sample_rate_combo_soapy), NULL, rate_string);
          if (active_receiver->sample_rate == rate) {
            sel = j;
          }
          j++;
          rate = rate / 2;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(sample_rate_combo_soapy), sel);
      }
      gtk_widget_set_hexpand(sample_rate_combo_soapy, TRUE);
      gtk_widget_set_halign(sample_rate_combo_soapy, GTK_ALIGN_FILL);
      gtk_grid_attach(GTK_GRID(grid),sample_rate_combo_soapy,x,row,1,1);
      g_signal_connect(sample_rate_combo_soapy, "changed", G_CALLBACK(rx_soapy_sr_combo_changed), NULL);
      }
      x++;
      break;
#endif

  }
 
  //
  // The CHARLY25 board (with RedPitaya) has no support for dither or random,
  // so those are left out. For Charly25, PreAmps and Alex Attenuator are controlled via
  // the sliders menu.
  //
  // Preamps are not switchable on all SDR hardware I know of, so this is commented out
  //
  // We assume Alex attenuators are present if we have an ALEX board and no Orion2
  // (ANAN-7000/8000 do not have these), and if the RX is fed by the first ADC.
  //
  if (filter_board != CHARLY25) {
      GtkWidget *dither_b=gtk_check_button_new_with_label("Dither");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dither_b), active_receiver->dither);
      gtk_grid_attach(GTK_GRID(grid),dither_b,x,2,1,1);
      g_signal_connect(dither_b,"toggled",G_CALLBACK(dither_cb),NULL);

      GtkWidget *random_b=gtk_check_button_new_with_label("Random");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (random_b), active_receiver->random);
      gtk_grid_attach(GTK_GRID(grid),random_b,x,3,1,1);
      g_signal_connect(random_b,"toggled",G_CALLBACK(random_cb),NULL);

      if((protocol==ORIGINAL_PROTOCOL && device == DEVICE_METIS) ||
#ifdef USBOZY
         (protocol==ORIGINAL_PROTOCOL && device == DEVICE_OZY) ||
#endif
	 (protocol==NEW_PROTOCOL && device == NEW_DEVICE_ATLAS)) {
        GtkWidget *preamp_b=gtk_check_button_new_with_label("Preamp");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preamp_b), active_receiver->preamp);
        gtk_grid_attach(GTK_GRID(grid),preamp_b,x,4,1,1);
        g_signal_connect(preamp_b,"toggled",G_CALLBACK(preamp_cb),NULL);
      }

      if (filter_board == ALEX && active_receiver->adc == 0
          && ((protocol==ORIGINAL_PROTOCOL && device != DEVICE_ORION2) || (protocol==NEW_PROTOCOL && device != NEW_DEVICE_ORION2))) {
  
        GtkWidget *alex_att_label=gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(alex_att_label), "<b>Alex attenuator</b>");
        gtk_widget_set_halign(alex_att_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), alex_att_label, x, 5, 1, 1);
        GtkWidget *alex_att_combo=gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo), NULL, "0 dB");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo), NULL, "10 dB");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo), NULL, "20 dB");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(alex_att_combo), NULL, "30 dB");
        gtk_combo_box_set_active(GTK_COMBO_BOX(alex_att_combo), active_receiver->alex_attenuation);
        gtk_widget_set_hexpand(alex_att_combo, TRUE);
        gtk_widget_set_halign(alex_att_combo, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid), alex_att_combo, x, 6, 1, 1);
        g_signal_connect(alex_att_combo, "changed", G_CALLBACK(alex_att_combo_changed), NULL);
    }
    x++;
  }

  // If there is more than one ADC, let the user associate an ADC
  // with the current receiver.
  if(n_adc>1) {
    GtkWidget *adc_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(adc_label), "<b>ADC</b>");
    gtk_widget_set_halign(adc_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),adc_label,x,7,1,1);
    GtkWidget *adc_combo=gtk_combo_box_text_new();
    for(i=0;i<n_adc;i++) {
      sprintf(label,"ADC-%d",i);
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(adc_combo), NULL, label);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(adc_combo), active_receiver->adc);
    gtk_widget_set_hexpand(adc_combo, TRUE);
    gtk_widget_set_halign(adc_combo, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid),adc_combo,x,8,1,1);
    g_signal_connect(adc_combo, "changed", G_CALLBACK(adc_combo_changed), NULL);
    x++;
  }


  int row=0;
  if(n_output_devices>0) {
    local_audio_b=gtk_check_button_new_with_label("Local Audio Output");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (local_audio_b), active_receiver->local_audio);
    gtk_widget_show(local_audio_b);
    gtk_grid_attach(GTK_GRID(grid),local_audio_b,x,++row,1,1);
    g_signal_connect(local_audio_b,"toggled",G_CALLBACK(local_audio_cb),NULL);

    if(active_receiver->audio_device==-1) active_receiver->audio_device=0;

    output=gtk_combo_box_text_new();
    for(i=0;i<n_output_devices;i++) {
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(output),NULL,output_devices[i].description);
      if(active_receiver->audio_name!=NULL) {
        if(strcmp(active_receiver->audio_name,output_devices[i].name)==0) {
          gtk_combo_box_set_active(GTK_COMBO_BOX(output),i);
        }
      }
    }

    i=gtk_combo_box_get_active(GTK_COMBO_BOX(output));
    if (i < 0) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(output),0);
    }

    gtk_grid_attach(GTK_GRID(grid),output,x,++row,1,1);
    g_signal_connect(output,"changed",G_CALLBACK(local_output_changed_cb),NULL);

    /*
    row=0;
    x++;
    */
    row++;

    GtkWidget *ch_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ch_label), "<b>Audio channel</b>");
    gtk_widget_set_halign(ch_label, GTK_ALIGN_START);
    gtk_widget_show(ch_label);
    gtk_grid_attach(GTK_GRID(grid),ch_label,x,++row,1,1);

    GtkWidget *audio_ch_combo=gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(audio_ch_combo), NULL, "Stereo");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(audio_ch_combo), NULL, "Left");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(audio_ch_combo), NULL, "Right");
    {
      int sel = 0;
      if (active_receiver->audio_channel == LEFT) {
        sel = 1;
      } else if (active_receiver->audio_channel == RIGHT) {
        sel = 2;
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(audio_ch_combo), sel);
    }
    gtk_widget_set_hexpand(audio_ch_combo, TRUE);
    gtk_widget_set_halign(audio_ch_combo, GTK_ALIGN_FILL);
    gtk_widget_show(audio_ch_combo);
    gtk_grid_attach(GTK_GRID(grid),audio_ch_combo,x,++row,1,1);
    g_signal_connect(audio_ch_combo, "changed", G_CALLBACK(audio_channel_combo_changed), NULL);
  }

  GtkWidget *mute_audio_b=gtk_check_button_new_with_label("Mute when not active");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_audio_b), active_receiver->mute_when_not_active);
  gtk_widget_show(mute_audio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_audio_b,x,++row,1,1);
  g_signal_connect(mute_audio_b,"toggled",G_CALLBACK(mute_audio_cb),NULL);
  
  row++;

  GtkWidget *mute_radio_b=gtk_check_button_new_with_label("Mute audio to radio");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_radio_b), active_receiver->mute_radio);
  gtk_widget_show(mute_radio_b);
  gtk_grid_attach(GTK_GRID(grid),mute_radio_b,x,++row,1,1);
  g_signal_connect(mute_radio_b,"toggled",G_CALLBACK(mute_radio_cb),NULL);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}


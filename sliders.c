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

//
// DL1YCF:
// uncomment the #define line following, then you will get
// a "TX compression" slider with an enabling checkbox
// in the bottom right of the sliders area, instead of the
// sequelch slider and checkbox.
// This option can also be passed to the compiler with "-D"
// and thus be activated through the Makefile.
//
//#define COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH 1
//

#include <gtk/gtk.h>
#include <semaphore.h>

#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "receiver.h"
#include "sliders.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "transmitter.h"
#include "property.h"
#include "main.h"
#include "ext.h"
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif
#include "actions.h"

static int width;
static int height;

static GtkWidget *sliders;

gint scale_timer;
int scale_status=NO_FUNCTION;
int scale_rx=0;
GtkWidget *scale_dialog;

static GtkWidget *af_gain_label;
static GtkWidget *af_gain_scale;
static GtkWidget *rf_gain_label;
static GtkWidget *rf_gain_scale;
static GtkWidget *agc_gain_label;
static GtkWidget *agc_scale;
static GtkWidget *attenuation_label;
static GtkWidget *attenuation_scale;
static GtkWidget *c25_att_preamp_label;
static GtkWidget *c25_att_combobox;
static GtkWidget *c25_preamp_combobox;
static GtkWidget *mic_gain_label;
static GtkWidget *mic_gain_scale;
static GtkWidget *drive_label;
static GtkWidget *drive_scale;
static GtkWidget *squelch_label;
static GtkWidget *squelch_scale;
static GtkWidget *squelch_enable;
static GtkWidget *comp_label;
static GtkWidget *comp_scale;
static GtkWidget *comp_enable;
static GtkWidget *dummy_label;
static GtkWidget *filter_width_scale;
static GtkWidget *filter_shift_scale;
static GtkWidget *diversity_gain_scale;
static GtkWidget *diversity_phase_scale;

static GtkWidget *rx3_stack;

/* Cairo fill; same chroma as @ph_slider_fill in css.c (#3e4652). */
#define PIHPSDR_SCALE_FILL_R (62.0 / 255.0)
#define PIHPSDR_SCALE_FILL_G (70.0 / 255.0)
#define PIHPSDR_SCALE_FILL_B (82.0 / 255.0)

/*
 * Trough CSS uses border-radius 9px; inner fill uses a smaller fixed radius so it
 * reads as a bar inside the shell, not a second full-height pill.
 */
/*
 * Trough border-radius is 9 px (see css.c) and the fill is inset 2 px from
 * the trough edge, so 9 - 2 = 7 keeps the inner and outer corners perfectly
 * concentric.
 */
#define PIHPSDR_SCALE_INNER_CORNER_R 7.0

static void
pihpsdr_cairo_rounded_rect(cairo_t *cr, gdouble x, gdouble y, gdouble w, gdouble h,
    gdouble corner_r)
{
  gdouble r;

  if (w <= 0.0 || h <= 0.0) {
    return;
  }
  r = corner_r;
  if (r > w / 2.0) {
    r = w / 2.0;
  }
  if (r > h / 2.0) {
    r = h / 2.0;
  }
  cairo_new_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2.0, 0.0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0.0, G_PI / 2.0);
  cairo_arc(cr, x + r, y + h - r, r, G_PI / 2.0, G_PI);
  cairo_arc(cr, x + r, y + r, r, G_PI, 3.0 * G_PI / 2.0);
  cairo_close_path(cr);
}

static gboolean
pihpsdr_fill_scale_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  GtkRange *range;
  GtkAdjustment *adj;
  GdkRectangle rr;
  gdouble lo;
  gdouble hi;
  gdouble span;
  gdouble v;
  gdouble frac;
  gdouble inset;
  gdouble usable;
  gdouble w_fill;
  gdouble min_fill;
  gdouble x0;
  gdouble y0;
  gdouble h_inner;
  gboolean rtl;

  (void)user_data;
  range = GTK_RANGE(widget);
  if (!gtk_widget_get_realized(widget)) {
    return FALSE;
  }
  if (gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) != GTK_ORIENTATION_HORIZONTAL) {
    return FALSE;
  }
  if (gtk_scale_get_has_origin(GTK_SCALE(widget))) {
    return FALSE;
  }

  gtk_range_get_range_rect(range, &rr);
  adj = gtk_range_get_adjustment(range);
  lo = gtk_adjustment_get_lower(adj);
  hi = gtk_adjustment_get_upper(adj);
  span = hi - lo;
  v = gtk_range_get_value(range);
  if (span <= 0.0) {
    return FALSE;
  }
  frac = (v - lo) / span;
  if (frac <= 1e-12) {
    return FALSE;
  }

  inset = 2.0;
  usable = (gdouble)rr.width - 2.0 * inset;
  if (usable <= 1.0) {
    return FALSE;
  }
  w_fill = frac * usable;
  min_fill = 14.0;
  if (w_fill > 0.0 && w_fill < min_fill) {
    w_fill = min_fill;
  }
  if (w_fill > usable) {
    w_fill = usable;
  }

  rtl = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);
  if (rtl) {
    x0 = (gdouble)rr.x + (gdouble)rr.width - inset - w_fill;
  } else {
    x0 = (gdouble)rr.x + inset;
  }
  y0 = (gdouble)rr.y + inset;
  h_inner = (gdouble)rr.height - 2.0 * inset;
  if (h_inner < 4.0) {
    h_inner = (gdouble)rr.height * 0.65;
    y0 = (gdouble)rr.y + ((gdouble)rr.height - h_inner) / 2.0;
  }

  cairo_save(cr);
  cairo_rectangle(cr, (gdouble)rr.x, (gdouble)rr.y, (gdouble)rr.width, (gdouble)rr.height);
  cairo_clip(cr);
  cairo_set_source_rgb(cr, PIHPSDR_SCALE_FILL_R, PIHPSDR_SCALE_FILL_G, PIHPSDR_SCALE_FILL_B);
  pihpsdr_cairo_rounded_rect(cr, x0, y0, w_fill, h_inner, PIHPSDR_SCALE_INNER_CORNER_R);
  cairo_fill(cr);
  cairo_restore(cr);

  return FALSE;
}

void
pihpsdr_fill_scale_install(GtkWidget *scale)
{
  gtk_scale_set_has_origin(GTK_SCALE(scale), FALSE);
  g_signal_connect_after(scale, "draw", G_CALLBACK(pihpsdr_fill_scale_draw_cb), NULL);
}

static void
pihpsdr_scale_value_label_refresh(GtkRange *range, gpointer user_data)
{
  GtkWidget *vl = user_data;
  int digits;
  double v;
  char buf[48];

  digits = gtk_scale_get_digits(GTK_SCALE(range));
  v = gtk_range_get_value(range);
  if (digits <= 0) {
    g_snprintf(buf, sizeof(buf), "%.0f", v);
  } else {
    g_snprintf(buf, sizeof(buf), "%.*f", digits, v);
  }
  gtk_label_set_text(GTK_LABEL(vl), buf);
}

GtkWidget *
pihpsdr_labeled_scale_row(const char *text, GtkWidget *scale, GtkWidget **label_out,
    gboolean show_value_on_right)
{
  GtkWidget *overlay;
  GtkWidget *lbl;
  GtkWidget *val_lbl;

  overlay = gtk_overlay_new();
  gtk_widget_set_name(overlay, "pihpsdr_slider_overlay_row");
  gtk_widget_set_hexpand(overlay, TRUE);

  lbl = gtk_label_new(text);
  if (label_out != NULL) {
    *label_out = lbl;
  }
  gtk_widget_set_name(lbl, "pihpsdr_slider_inline_left");
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(lbl, 8);

  gtk_widget_set_name(scale, "pihpsdr_fill_scale");
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);

  gtk_container_add(GTK_CONTAINER(overlay), scale);

  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), lbl);
  gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), lbl, TRUE);

  if (show_value_on_right) {
    val_lbl = gtk_label_new("");
    gtk_widget_set_name(val_lbl, "pihpsdr_slider_inline_value");
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 1.0);
    gtk_widget_set_halign(val_lbl, GTK_ALIGN_END);
    gtk_widget_set_valign(val_lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end(val_lbl, 10);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), val_lbl);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), val_lbl, TRUE);
    g_signal_connect(scale, "value-changed", G_CALLBACK(pihpsdr_scale_value_label_refresh), val_lbl);
    pihpsdr_scale_value_label_refresh(GTK_RANGE(scale), val_lbl);
    gtk_widget_show(val_lbl);
  }

  pihpsdr_fill_scale_install(scale);

  gtk_widget_show(scale);
  gtk_widget_show(lbl);
  return overlay;
}

static GtkWidget *
pihpsdr_squelch_like_row(GtkWidget **label_out, GtkWidget *scale, GtkWidget *toggle, const char *text)
{
  GtkWidget *outer;
  GtkWidget *overlay;
  GtkWidget *lbl;
  GtkWidget *val_lbl;

  outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_name(outer, "pihpsdr_dashboard_row");

  overlay = gtk_overlay_new();
  gtk_widget_set_name(overlay, "pihpsdr_slider_overlay_row");
  gtk_widget_set_hexpand(overlay, TRUE);

  lbl = gtk_label_new(text);
  if (label_out != NULL) {
    *label_out = lbl;
  }
  gtk_widget_set_name(lbl, "pihpsdr_slider_inline_left");
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(lbl, 8);

  gtk_widget_set_name(scale, "pihpsdr_fill_scale");
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);

  gtk_container_add(GTK_CONTAINER(overlay), scale);

  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), lbl);
  gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), lbl, TRUE);

  val_lbl = gtk_label_new("");
  gtk_widget_set_name(val_lbl, "pihpsdr_slider_inline_value");
  gtk_label_set_xalign(GTK_LABEL(val_lbl), 1.0);
  gtk_widget_set_halign(val_lbl, GTK_ALIGN_END);
  gtk_widget_set_valign(val_lbl, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end(val_lbl, 10);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), val_lbl);
  gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), val_lbl, TRUE);

  g_signal_connect(scale, "value-changed", G_CALLBACK(pihpsdr_scale_value_label_refresh), val_lbl);
  pihpsdr_scale_value_label_refresh(GTK_RANGE(scale), val_lbl);
  pihpsdr_fill_scale_install(scale);

  gtk_box_pack_start(GTK_BOX(outer), overlay, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(outer), toggle, FALSE, FALSE, 0);

  gtk_widget_show(scale);
  gtk_widget_show(lbl);
  gtk_widget_show(val_lbl);
  gtk_widget_show(overlay);
  gtk_widget_show(toggle);
  gtk_widget_show(outer);
  return outer;
}

void sliders_update() {
  if(display_sliders) {
    if(can_transmit) {
      if(mic_linein) {
        gtk_label_set_text(GTK_LABEL(mic_gain_label),"Linein:");
        gtk_range_set_range(GTK_RANGE(mic_gain_scale),0.0,31.0);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      } else {
        gtk_label_set_text(GTK_LABEL(mic_gain_label),"Mic:");
        gtk_range_set_range(GTK_RANGE(mic_gain_scale),-12.0,50.0);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
      }
    }
  }
}

int sliders_active_receiver_changed(void *data) {
  if(display_sliders) {
    gtk_range_set_value(GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
    gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
    if (filter_board == CHARLY25) {
      update_att_preamp();
    } else {
      if(attenuation_scale!=NULL) gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    }
    sliders_update();
  }
  return FALSE;
}

int scale_timeout_cb(gpointer data) {
  gtk_widget_destroy(scale_dialog);
  scale_status=NO_FUNCTION;
  return FALSE;
}

static void attenuation_value_changed_cb(GtkWidget *widget, gpointer data) {
  adc[active_receiver->adc].gain=gtk_range_get_value(GTK_RANGE(attenuation_scale));
  adc[active_receiver->adc].attenuation=(int)adc[active_receiver->adc].gain;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_attenuation(client_socket,active_receiver->id,(int)adc[active_receiver->adc].attenuation);
  } else {
#endif
    set_attenuation(adc[active_receiver->adc].attenuation);
#ifdef CLIENT_SERVER
  }
#endif
}

void set_attenuation_value(double value) {
  g_print("%s\n",__FUNCTION__);
  adc[active_receiver->adc].attenuation=(int)value;
  set_attenuation(adc[active_receiver->adc].attenuation);
  if(display_sliders) {
    if (have_rx_gain) {
	gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    } else {
        gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
    }
  } else {
    if(scale_status!=ATTENUATION) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      char title[64];
      if (have_rx_gain) {
	  sprintf(title,"RX GAIN - ADC-%d (dB)",active_receiver->adc);
      } else {
          sprintf(title,"Attenuation - ADC-%d (dB)",active_receiver->adc);
      }
      scale_status=ATTENUATION;
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      if (have_rx_gain) {
        attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 48.0, 1.00);
      } else {
        attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      }
      gtk_widget_set_size_request (attenuation_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
      gtk_widget_show(attenuation_scale);
      gtk_container_add(GTK_CONTAINER(content),attenuation_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(attenuation_scale),(double)adc[active_receiver->adc].attenuation);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_att_preamp(void) {
  // CHARLY25: update the ATT/Pre buttons to the values of the active RX
  // We should also set the attenuation for use in meter.c
  if (filter_board == CHARLY25) {
    char id[] = "x";
    if (active_receiver->adc != 0) {
      active_receiver->alex_attenuation=0;
      active_receiver->preamp=0;
      active_receiver->dither=0;
      adc[active_receiver->adc].attenuation = 0;
    }
    sprintf(id, "%d", active_receiver->alex_attenuation);
    adc[active_receiver->adc].attenuation = 12*active_receiver->alex_attenuation;
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_att_combobox), id);
    sprintf(id, "%d", active_receiver->preamp + active_receiver->dither);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_preamp_combobox), id);
  }
}

void att_type_changed(void) {
  g_print("%s\n",__FUNCTION__);
  if (rx3_stack == NULL) {
    return;
  }
  if (filter_board == CHARLY25) {
    gtk_stack_set_visible_child_name(GTK_STACK(rx3_stack), "c25");
    update_att_preamp();
  } else {
    gtk_stack_set_visible_child_name(GTK_STACK(rx3_stack), "alex");
  }
}

static gboolean load_att_type_cb(gpointer data) {
  att_type_changed();
  return G_SOURCE_REMOVE;
}

static void c25_att_combobox_changed(GtkWidget *widget, gpointer data) {
  int val = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
  if (active_receiver->adc == 0) {
    // this button is only valid for the first receiver
    // store attenuation, such that in meter.c the correct level is displayed
    adc[active_receiver->adc].attenuation = 12*val;
    set_alex_attenuation(val);
  } else {
    // always show "0 dB" on the button if the second RX is active
    if (val != 0) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_att_combobox), "0");
    }
  }
}

static void c25_preamp_combobox_changed(GtkWidget *widget, gpointer data) {
  int val = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
  if (active_receiver->id == 0) {
    // This button is only valid for the first receiver
    // dither and preamp are "misused" to store the PreAmp value.
    // this has to be exploited in meter.c
    active_receiver->dither = (val >= 2);  // second preamp ON
    active_receiver->preamp = (val >= 1);  // first  preamp ON
  } else{
    // always show "0 dB" on the button if the second RX is active
    if (val != 0) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(c25_preamp_combobox), "0");
    }
  }
}

static void agcgain_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->agc_gain=gtk_range_get_value(GTK_RANGE(agc_scale));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_agc_gain(client_socket,active_receiver->id,(int)active_receiver->agc_gain,(int)active_receiver->agc_hang,(int)active_receiver->agc_thresh);
  } else {
#endif
    SetRXAAGCTop(active_receiver->id, active_receiver->agc_gain);
    GetRXAAGCHangLevel(active_receiver->id, &active_receiver->agc_hang);
    GetRXAAGCThresh(active_receiver->id, &active_receiver->agc_thresh, 4096.0, (double)active_receiver->sample_rate);
#ifdef CLIENT_SERVER
  }
#endif
}

void set_agc_gain(int rx,double value) {
  g_print("%s\n",__FUNCTION__);
  receiver[rx]->agc_gain=value;
  SetRXAAGCTop(receiver[rx]->id, receiver[rx]->agc_gain);
  GetRXAAGCHangLevel(receiver[rx]->id, &receiver[rx]->agc_hang);
  GetRXAAGCThresh(receiver[rx]->id, &receiver[rx]->agc_thresh, 4096.0, (double)receiver[rx]->sample_rate);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
  } else {
    if(scale_status!=AGC_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=AGC_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"AGC Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.00);
      gtk_widget_set_size_request (agc_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
      gtk_widget_show(agc_scale);
      gtk_container_add(GTK_CONTAINER(content),agc_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(agc_scale),receiver[rx]->agc_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void update_agc_gain(double gain) {
  set_agc_gain(active_receiver->id,gain);
}

static void afgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    active_receiver->volume=gtk_range_get_value(GTK_RANGE(af_gain_scale))/100.0;

#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      int v=(int)(active_receiver->volume*100.0);
      send_volume(client_socket,active_receiver->id,v);
    } else {
#endif
      SetRXAPanelGain1 (active_receiver->id, active_receiver->volume);
#ifdef CLIENT_SERVER
    }
#endif
}

void update_af_gain() {
  set_af_gain(active_receiver->id,active_receiver->volume);
}

void set_af_gain(int rx,double value) {
  receiver[rx]->volume=value;
  SetRXAPanelGain1 (receiver[rx]->id, receiver[rx]->volume);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
  } else {
    if(scale_status!=AF_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=AF_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"AF Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (af_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
      gtk_widget_show(af_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),af_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(af_gain_scale),receiver[rx]->volume*100.0);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void rf_gain_value_changed_cb(GtkWidget *widget, gpointer data) {
    adc[active_receiver->adc].gain=gtk_range_get_value(GTK_RANGE(rf_gain_scale));
#ifdef CLIENT_SERVER
    if (radio_is_remote) {
      send_rfgain(client_socket, active_receiver->id, adc[active_receiver->adc].gain);
    }
    return;
#endif
    switch(protocol) {
#ifdef SOAPYSDR
      case SOAPYSDR_PROTOCOL:
        soapy_protocol_set_gain(active_receiver);
	break;
#endif
      default:
	break;
    }
}

void update_rf_gain() {
  //set_rf_gain(active_receiver->id,active_receiver->rf_gain);
  set_rf_gain(active_receiver->id,adc[active_receiver->id].gain);
}

void set_rf_gain(int rx,double value) {
  g_print("%s\n",__FUNCTION__);
  adc[receiver[rx]->id].gain=value;
#ifdef SOAPYSDR
  if(protocol==SOAPYSDR_PROTOCOL) {
    soapy_protocol_set_gain(receiver[rx]);
  }
#endif
  if(display_sliders) {
    //gtk_range_set_value (GTK_RANGE(attenuation_scale),receiver[rx]->rf_gain);
    gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
  } else {
    if(scale_status!=RF_GAIN || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=RF_GAIN;
      scale_rx=rx;
      char title[64];
      sprintf(title,"RF Gain RX %d",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_set_size_request (rf_gain_scale, 400, 30);
      //gtk_range_set_value (GTK_RANGE(rf_gain_scale),receiver[rx]->rf_gain);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
      gtk_widget_show(rf_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),rf_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      //gtk_range_set_value (GTK_RANGE(rf_gain_scale),receiver[rx]->rf_gain);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[receiver[rx]->id].gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

void set_filter_width(int rx,int width) {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=IF_WIDTH || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=IF_WIDTH;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Filter Width RX %d (Hz)",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      filter_width_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 4000.0, 1.00);
      gtk_widget_set_size_request (filter_width_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(filter_width_scale),(double)width);
      gtk_widget_show(filter_width_scale);
      gtk_container_add(GTK_CONTAINER(content),filter_width_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(filter_width_scale),(double)width);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

void set_filter_shift(int rx,int shift) {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=IF_SHIFT || scale_rx!=rx) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=IF_SHIFT;
      scale_rx=rx;
      char title[64];
      sprintf(title,"Filter SHIFT RX %d (Hz)",rx);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      filter_shift_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-1000.0, 1000.0, 1.00);
      gtk_widget_set_size_request (filter_shift_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(filter_shift_scale),(double)shift);
      gtk_widget_show(filter_shift_scale);
      gtk_container_add(GTK_CONTAINER(content),filter_shift_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(filter_shift_scale),(double)shift);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

static void micgain_value_changed_cb(GtkWidget *widget, gpointer data) {
    if(mic_linein) {
      linein_gain=(int)gtk_range_get_value(GTK_RANGE(widget));
    } else {
      mic_gain=gtk_range_get_value(GTK_RANGE(widget));
      SetTXAPanelGain1(transmitter->id,pow(10.0, mic_gain/20.0));
    }
}

void set_mic_gain(double value) {
  g_print("%s\n",__FUNCTION__);
  if(can_transmit) {
    mic_gain=value;
    SetTXAPanelGain1(transmitter->id,pow(10.0, mic_gain/20.0));
    if(display_sliders) {
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
    } else {
      if(scale_status!=MIC_GAIN) {
        if(scale_status!=NO_FUNCTION) {
          g_source_remove(scale_timer);
          gtk_widget_destroy(scale_dialog);
          scale_status=NO_FUNCTION;
        }
      }
      if(scale_status==NO_FUNCTION) {
        scale_status=MIC_GAIN;
        scale_dialog=gtk_dialog_new_with_buttons("Mic Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
        GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
        mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 50.0, 1.00);
        gtk_widget_set_size_request (mic_gain_scale, 400, 30);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
        gtk_widget_show(mic_gain_scale);
        gtk_container_add(GTK_CONTAINER(content),mic_gain_scale);
        scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
        //gtk_widget_show_all(scale_dialog);
        gtk_dialog_run(GTK_DIALOG(scale_dialog));
      } else {
        g_source_remove(scale_timer);
        gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_gain);
        scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      }
    }
  }
}

void set_linein_gain(int value) {
  g_print("%s\n",__FUNCTION__);
  linein_gain=value;
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
  } else {
    if(scale_status!=LINEIN_GAIN) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=LINEIN_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("Linein Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.00);
      gtk_widget_set_size_request (mic_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      gtk_widget_show(mic_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),mic_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(mic_gain_scale),linein_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

int update_linein_gain(void *data) {
  set_linein_gain(*(int*)data);
  free(data);
  return 0;
}

void set_drive(double value) {
  g_print("%s\n",__FUNCTION__);
  setDrive(value);
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(drive_scale),value);
  } else {
    if(scale_status!=DRIVE) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=DRIVE;
      scale_dialog=gtk_dialog_new_with_buttons("Drive",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, drive_max, 1.00);
      gtk_widget_override_font(drive_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_widget_set_size_request (drive_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(drive_scale),value);
      gtk_widget_show(drive_scale);
      gtk_container_add(GTK_CONTAINER(content),drive_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(drive_scale),value);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
  }
}

static void drive_value_changed_cb(GtkWidget *widget, gpointer data) {
  setDrive(gtk_range_get_value(GTK_RANGE(drive_scale)));
}

int update_drive(void *data) {
  set_drive(*(double *)data);
  free(data);
  return 0;
}

static void squelch_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch=gtk_range_get_value(GTK_RANGE(widget));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_squelch(client_socket,active_receiver->id,active_receiver->squelch_enable,active_receiver->squelch);
  } else {
#endif
    setSquelch(active_receiver);
#ifdef CLIENT_SERVER
  }
#endif
}

static void squelch_enable_cb(GtkWidget *widget, gpointer data) {
  active_receiver->squelch_enable=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_squelch(client_socket,active_receiver->id,active_receiver->squelch_enable,active_receiver->squelch);
  } else {
#endif
    setSquelch(active_receiver);
#ifdef CLIENT_SERVER
  }
#endif
}

static void compressor_value_changed_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor_level(transmitter,gtk_range_get_value(GTK_RANGE(widget)));
  // This value is now also reflected in the VFO panel
  g_idle_add(ext_vfo_update, NULL);

}

static void compressor_enable_cb(GtkWidget *widget, gpointer data) {
  transmitter_set_compressor(transmitter,gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
  // This value is now also reflected in the VFO panel
  g_idle_add(ext_vfo_update, NULL);
}

void set_squelch() {
  g_print("%s\n",__FUNCTION__);
  setSquelch(active_receiver);
#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
  } else {
#endif
    if(scale_status!=SQUELCH) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=SQUELCH;
      char title[64];
      sprintf(title,"Squelch RX %d (Hz)",active_receiver->id);
      scale_dialog=gtk_dialog_new_with_buttons(title,GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
      gtk_widget_override_font(squelch_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
      gtk_widget_set_size_request (squelch_scale, 400, 30);
      gtk_widget_show(squelch_scale);
      gtk_container_add(GTK_CONTAINER(content),squelch_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  }
#endif
}

void set_compression(TRANSMITTER* tx) {
  g_print("%s\n",__FUNCTION__);
  // Update VFO panel to reflect changed value
  g_idle_add(ext_vfo_update, NULL);
#ifdef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  if(display_sliders) {
    gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
  } else {
#endif
    if(scale_status!=COMPRESSION) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=COMPRESSION;
      scale_dialog=gtk_dialog_new_with_buttons("COMP",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      comp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 20.0, 1.00);
      gtk_widget_override_font(comp_scale, pango_font_description_from_string(SLIDERS_FONT));
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      gtk_widget_set_size_request (comp_scale, 400, 30);
      gtk_widget_show(comp_scale);
      gtk_container_add(GTK_CONTAINER(content),comp_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(comp_scale),tx->compressor_level);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
#ifdef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  }
#endif
}

void show_diversity_gain() {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=DIV_GAIN) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=DIV_GAIN;
      scale_dialog=gtk_dialog_new_with_buttons("Diversity Gain",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      diversity_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-27.0, 27.0, 0.01);
      gtk_widget_set_size_request (diversity_gain_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(diversity_gain_scale),div_gain);
      gtk_widget_show(diversity_gain_scale);
      gtk_container_add(GTK_CONTAINER(content),diversity_gain_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(diversity_gain_scale),div_gain);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}

void show_diversity_phase() {
  g_print("%s\n",__FUNCTION__);
    if(scale_status!=DIV_PHASE) {
      if(scale_status!=NO_FUNCTION) {
        g_source_remove(scale_timer);
        gtk_widget_destroy(scale_dialog);
        scale_status=NO_FUNCTION;
      }
    }
    if(scale_status==NO_FUNCTION) {
      scale_status=DIV_PHASE;
      scale_dialog=gtk_dialog_new_with_buttons("Diversity Phase",GTK_WINDOW(top_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);
      pihpsdr_style_modal_dialog(scale_dialog);
      GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
      diversity_phase_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -180.0, 180.0, 0.1);
      gtk_widget_set_size_request (diversity_phase_scale, 400, 30);
      gtk_range_set_value (GTK_RANGE(diversity_phase_scale),div_phase);
      gtk_widget_show(diversity_phase_scale);
      gtk_container_add(GTK_CONTAINER(content),diversity_phase_scale);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
      //gtk_widget_show_all(scale_dialog);
      gtk_dialog_run(GTK_DIALOG(scale_dialog));
    } else {
      g_source_remove(scale_timer);
      gtk_range_set_value (GTK_RANGE(diversity_phase_scale),div_phase);
      scale_timer=g_timeout_add(2000,scale_timeout_cb,NULL);
    }
}


GtkWidget *sliders_init(int my_width, int my_height) {
  GtkWidget *af_row;
  GtkWidget *agc_row;
  GtkWidget *alex_row;
  GtkWidget *c25_row;
  GtkWidget *mic_row;
  GtkWidget *drive_row;
  GtkWidget *sq_row;

  width=my_width;
  height=my_height;

  fprintf(stderr,"sliders_init: width=%d height=%d\n", width,height);

  sliders=gtk_grid_new();
  gtk_widget_set_size_request(sliders, width, height);
  gtk_widget_set_name(sliders, "pihpsdr_sliders_grid");
  gtk_grid_set_row_homogeneous(GTK_GRID(sliders), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(sliders), TRUE);
  gtk_grid_set_row_spacing(GTK_GRID(sliders), 0);
  gtk_grid_set_column_spacing(GTK_GRID(sliders), 3);

  af_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.00);
  gtk_scale_set_digits(GTK_SCALE(af_gain_scale), 0);
  gtk_range_set_increments (GTK_RANGE(af_gain_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(af_gain_scale),active_receiver->volume*100.0);
  g_signal_connect(G_OBJECT(af_gain_scale),"value_changed",G_CALLBACK(afgain_value_changed_cb),NULL);
  af_row=pihpsdr_labeled_scale_row("AF", af_gain_scale, &af_gain_label, TRUE);
  gtk_grid_attach(GTK_GRID(sliders),af_row,0,0,3,1);

  agc_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-20.0, 120.0, 1.0);
  gtk_scale_set_digits(GTK_SCALE(agc_scale), 0);
  gtk_range_set_increments (GTK_RANGE(agc_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(agc_scale),active_receiver->agc_gain);
  g_signal_connect(G_OBJECT(agc_scale),"value_changed",G_CALLBACK(agcgain_value_changed_cb),NULL);
  agc_row=pihpsdr_labeled_scale_row("AGC", agc_scale, &agc_gain_label, TRUE);
  gtk_grid_attach(GTK_GRID(sliders),agc_row,3,0,3,1);

  c25_att_preamp_label = gtk_label_new("Att/PreAmp");
  gtk_widget_set_name(c25_att_preamp_label, "pihpsdr_slider_label");

  c25_att_combobox = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "0", "0 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "1", "-12 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "2", "-24 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_att_combobox), "3", "-36 dB");
  g_signal_connect(G_OBJECT(c25_att_combobox), "changed", G_CALLBACK(c25_att_combobox_changed), NULL);

  c25_preamp_combobox = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "0", "0 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "1", "18 dB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c25_preamp_combobox), "2", "36 dB");
  g_signal_connect(G_OBJECT(c25_preamp_combobox), "changed", G_CALLBACK(c25_preamp_combobox_changed), NULL);

  c25_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_name(c25_row, "pihpsdr_dashboard_row");
  gtk_box_pack_start(GTK_BOX(c25_row), c25_att_preamp_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(c25_row), c25_att_combobox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(c25_row), c25_preamp_combobox, TRUE, TRUE, 0);
  gtk_widget_show(c25_att_preamp_label);
  gtk_widget_show(c25_att_combobox);
  gtk_widget_show(c25_preamp_combobox);

  rx3_stack = gtk_stack_new();
  gtk_widget_set_name(rx3_stack, "pihpsdr_rx3_stack");

  if(have_rx_gain) {
#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,adc[0].min_gain, adc[0].max_gain, 1.0);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[0].gain);
    } else {
#endif
      rf_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-12.0, 48.0, 1.0);
      gtk_range_set_value (GTK_RANGE(rf_gain_scale),adc[active_receiver->adc].attenuation);
#ifdef SOAPYSDR
    }
#endif
    gtk_range_set_increments (GTK_RANGE(rf_gain_scale),1.0,1.0);
#ifdef SOAPYSDR
    if(protocol==SOAPYSDR_PROTOCOL) {
      gtk_scale_set_digits(GTK_SCALE(rf_gain_scale), 1);
    } else {
      gtk_scale_set_digits(GTK_SCALE(rf_gain_scale), 0);
    }
#else
    gtk_scale_set_digits(GTK_SCALE(rf_gain_scale), 0);
#endif
    g_signal_connect(G_OBJECT(rf_gain_scale),"value_changed",G_CALLBACK(rf_gain_value_changed_cb),NULL);
    alex_row=pihpsdr_labeled_scale_row("RX", rf_gain_scale, &rf_gain_label, TRUE);
  } else {
    attenuation_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 31.0, 1.0);
    gtk_scale_set_digits(GTK_SCALE(attenuation_scale), 0);
    gtk_range_set_value (GTK_RANGE(attenuation_scale),adc[active_receiver->adc].attenuation);
    gtk_range_set_increments (GTK_RANGE(attenuation_scale),1.0,1.0);
    g_signal_connect(G_OBJECT(attenuation_scale),"value_changed",G_CALLBACK(attenuation_value_changed_cb),NULL);
    alex_row=pihpsdr_labeled_scale_row("ATT", attenuation_scale, &attenuation_label, TRUE);
  }

  gtk_stack_add_named(GTK_STACK(rx3_stack), alex_row, "alex");
  gtk_stack_add_named(GTK_STACK(rx3_stack), c25_row, "c25");
  gtk_grid_attach(GTK_GRID(sliders), rx3_stack, 6, 0, 3, 1);

  if (filter_board == CHARLY25) {
    gtk_stack_set_visible_child_name(GTK_STACK(rx3_stack), "c25");
  } else {
    gtk_stack_set_visible_child_name(GTK_STACK(rx3_stack), "alex");
  }

  g_idle_add(load_att_type_cb, NULL);

  if(can_transmit) {

    mic_gain_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,mic_linein?0.0:-12.0,mic_linein?31.0:50.0, 1.0);
    gtk_scale_set_digits(GTK_SCALE(mic_gain_scale), 0);
    gtk_range_set_increments (GTK_RANGE(mic_gain_scale),1.0,1.0);
    gtk_range_set_value (GTK_RANGE(mic_gain_scale),mic_linein?linein_gain:mic_gain);
    g_signal_connect(G_OBJECT(mic_gain_scale),"value_changed",G_CALLBACK(micgain_value_changed_cb),NULL);
    mic_row=pihpsdr_labeled_scale_row(mic_linein?"Linein":"Mic", mic_gain_scale, &mic_gain_label, TRUE);
    gtk_grid_attach(GTK_GRID(sliders),mic_row,0,1,3,1);

    drive_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, drive_max, 1.00);
    gtk_scale_set_digits(GTK_SCALE(drive_scale), 0);
    gtk_range_set_increments (GTK_RANGE(drive_scale),1.0,1.0);
    gtk_range_set_value (GTK_RANGE(drive_scale),getDrive());
    g_signal_connect(G_OBJECT(drive_scale),"value_changed",G_CALLBACK(drive_value_changed_cb),NULL);
    drive_row=pihpsdr_labeled_scale_row("Drive", drive_scale, &drive_label, TRUE);
    gtk_grid_attach(GTK_GRID(sliders),drive_row,3,1,3,1);
  }

#ifndef COMPRESSION_SLIDER_INSTEAD_OF_SQUELCH
  squelch_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 100.0, 1.0);
  gtk_scale_set_digits(GTK_SCALE(squelch_scale), 0);
  gtk_range_set_increments (GTK_RANGE(squelch_scale),1.0,1.0);
  gtk_range_set_value (GTK_RANGE(squelch_scale),active_receiver->squelch);
  g_signal_connect(G_OBJECT(squelch_scale),"value_changed",G_CALLBACK(squelch_value_changed_cb),NULL);

  squelch_enable=gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(squelch_enable),active_receiver->squelch_enable);
  g_signal_connect(squelch_enable,"toggled",G_CALLBACK(squelch_enable_cb),NULL);

  sq_row=pihpsdr_squelch_like_row(&squelch_label, squelch_scale, squelch_enable, "SQL");
  gtk_grid_attach(GTK_GRID(sliders),sq_row,6,1,3,1);
#else
  if(can_transmit) {
    comp_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0.0, 20.0, 1.0);
    gtk_scale_set_digits(GTK_SCALE(comp_scale), 0);
    gtk_range_set_increments (GTK_RANGE(comp_scale),1.0,1.0);
    gtk_range_set_value (GTK_RANGE(comp_scale),transmitter->compressor_level);
    g_signal_connect(G_OBJECT(comp_scale),"value_changed",G_CALLBACK(compressor_value_changed_cb),NULL);

    comp_enable=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(comp_enable),transmitter->compressor);
    g_signal_connect(comp_enable,"toggled",G_CALLBACK(compressor_enable_cb),NULL);

    sq_row=pihpsdr_squelch_like_row(&comp_label, comp_scale, comp_enable, "COMP");
    gtk_grid_attach(GTK_GRID(sliders),sq_row,6,1,3,1);
  }
#endif

  return sliders;
}

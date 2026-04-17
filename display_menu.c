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
#include "menu_page.h"
#include "new_menu.h"
#include "display_menu.h"
#include "channel.h"
#include "radio.h"
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

static void detector_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  static const int modes[] = {
    DETECTOR_MODE_PEAK,
    DETECTOR_MODE_ROSENFELL,
    DETECTOR_MODE_AVERAGE,
    DETECTOR_MODE_SAMPLE
  };
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= (int)(sizeof(modes) / sizeof(modes[0]))) {
    return;
  }
  display_detector_mode = modes[i];
  SetDisplayDetectorMode(active_receiver->id, 0, display_detector_mode);
}

static void average_combo_changed(GtkComboBox *combo, gpointer data) {
  (void)data;
  static const int modes[] = {
    AVERAGE_MODE_NONE,
    AVERAGE_MODE_RECURSIVE,
    AVERAGE_MODE_TIME_WINDOW,
    AVERAGE_MODE_LOG_RECURSIVE
  };
  int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (i < 0 || i >= (int)(sizeof(modes) / sizeof(modes[0]))) {
    return;
  }
  display_average_mode = modes[i];
  SetDisplayAverageMode(active_receiver->id, 0, display_average_mode);
}

static void time_value_changed_cb(GtkWidget *widget, gpointer data) {
  display_average_time=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  calculate_display_average(active_receiver);
}

static void filled_cb(GtkWidget *widget, gpointer data) {
  display_filled=display_filled==1?0:1;
}

static void gradient_cb(GtkWidget *widget, gpointer data) {
  display_gradient=display_gradient==1?0:1;
}

static void frames_per_second_value_changed_cb(GtkWidget *widget, gpointer data) {
  updates_per_second=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  active_receiver->fps=updates_per_second;
  calculate_display_average(active_receiver);
  set_displaying(active_receiver, 1);
}

static void panadapter_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void panadapter_step_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->panadapter_step=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_high_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_high=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_low_value_changed_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_low=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void waterfall_automatic_cb(GtkWidget *widget, gpointer data) {
  active_receiver->waterfall_automatic=active_receiver->waterfall_automatic==1?0:1;
}

static void display_panadapter_cb(GtkWidget *widget, gpointer data) {
  active_receiver->display_panadapter=active_receiver->display_panadapter==1?0:1;
  reconfigure_radio();
}

static void display_waterfall_cb(GtkWidget *widget, gpointer data) {
  active_receiver->display_waterfall=active_receiver->display_waterfall==1?0:1;
  reconfigure_radio();
}

static void display_zoompan_cb(GtkWidget *widget, gpointer data) {
  display_zoompan=display_zoompan==1?0:1;
  reconfigure_radio();
}

static void display_sliders_cb(GtkWidget *widget, gpointer data) {
  display_sliders=display_sliders==1?0:1;
  reconfigure_radio();
}


static void display_toolbar_cb(GtkWidget *widget, gpointer data) {
  display_toolbar=display_toolbar==1?0:1;
  reconfigure_radio();
}

static void display_sequence_errors_cb(GtkWidget *widget, gpointer data) {
  display_sequence_errors=display_sequence_errors==1?0:1;
}

static void menu_animations_cb(GtkWidget *widget, gpointer data) {
  pihpsdr_menu_animations = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

void display_menu(GtkWidget *parent) {
  parent_window=parent;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent_window));
  //gtk_window_set_decorated(GTK_WINDOW(dialog),FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog),"piHPSDR - Display");
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  pihpsdr_style_menu_page(dialog);

  GtkWidget *grid=gtk_grid_new();
  pihpsdr_modal_grid_defaults(grid);

  int col=0;
  int row=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect (close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid),close_b,col,row,1,1);

  row++;
  col=0;

  GtkWidget *filled_b=gtk_check_button_new_with_label("Fill Panadapter");
  //gtk_widget_override_font(filled_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (filled_b), display_filled);
  gtk_widget_show(filled_b);
  gtk_grid_attach(GTK_GRID(grid),filled_b,col,row,1,1);
  g_signal_connect(filled_b,"toggled",G_CALLBACK(filled_cb),NULL);

  col++;

  GtkWidget *gradient_b=gtk_check_button_new_with_label("Gradient");
  //gtk_widget_override_font(filled_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gradient_b), display_gradient);
  gtk_widget_show(gradient_b);
  gtk_grid_attach(GTK_GRID(grid),gradient_b,col,row,1,1);
  g_signal_connect(gradient_b,"toggled",G_CALLBACK(gradient_cb),NULL);

  row++;
  col=0;

  GtkWidget *frames_per_second_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(frames_per_second_label), "<b>Frames Per Second:</b>");
  //gtk_widget_override_font(frames_per_second_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(frames_per_second_label);
  gtk_grid_attach(GTK_GRID(grid),frames_per_second_label,col,row,1,1);

  col++;

  GtkWidget *frames_per_second_r=gtk_spin_button_new_with_range(1.0,100.0,1.0);
  //gtk_widget_override_font(frames_per_second_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(frames_per_second_r),(double)updates_per_second);
  gtk_widget_show(frames_per_second_r);
  gtk_grid_attach(GTK_GRID(grid),frames_per_second_r,col,row,1,1);
  g_signal_connect(frames_per_second_r,"value_changed",G_CALLBACK(frames_per_second_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_high_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_high_label), "<b>Panadapter High: </b>");
  //gtk_widget_override_font(panadapter_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_high_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_high_r),(double)active_receiver->panadapter_high);
  gtk_widget_show(panadapter_high_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_high_r,col,row,1,1);
  g_signal_connect(panadapter_high_r,"value_changed",G_CALLBACK(panadapter_high_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_low_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_low_label), "<b>Panadapter Low: </b>");
  //gtk_widget_override_font(panadapter_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_low_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(panadapter_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_low_r),(double)active_receiver->panadapter_low);
  gtk_widget_show(panadapter_low_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_low_r,col,row,1,1);
  g_signal_connect(panadapter_low_r,"value_changed",G_CALLBACK(panadapter_low_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *panadapter_step_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(panadapter_step_label), "<b>Panadapter Step: </b>");
  //gtk_widget_override_font(panadapter_step_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(panadapter_step_label);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_label,col,row,1,1);

  col++;

  GtkWidget *panadapter_step_r=gtk_spin_button_new_with_range(1.0,20.0,1.0);
  //gtk_widget_override_font(panadapter_step_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(panadapter_step_r),(double)active_receiver->panadapter_step);
  gtk_widget_show(panadapter_step_r);
  gtk_grid_attach(GTK_GRID(grid),panadapter_step_r,col,row,1,1);
  g_signal_connect(panadapter_step_r,"value_changed",G_CALLBACK(panadapter_step_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_automatic_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_automatic_label), "<b>Waterfall Automatic: </b>");
  //gtk_widget_override_font(waterfall_automatic_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_automatic_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_automatic_b=gtk_check_button_new();
  //gtk_widget_override_font(waterfall_automatic_b, pango_font_description_from_string("Arial 18"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (waterfall_automatic_b), active_receiver->waterfall_automatic);
  gtk_widget_show(waterfall_automatic_b);
  gtk_grid_attach(GTK_GRID(grid),waterfall_automatic_b,col,row,1,1);
  g_signal_connect(waterfall_automatic_b,"toggled",G_CALLBACK(waterfall_automatic_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_high_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_high_label), "<b>Waterfall High: </b>");
  //gtk_widget_override_font(waterfall_high_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_high_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_high_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_high_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_high_r),(double)active_receiver->waterfall_high);
  gtk_widget_show(waterfall_high_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_high_r,col,row,1,1);
  g_signal_connect(waterfall_high_r,"value_changed",G_CALLBACK(waterfall_high_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *waterfall_low_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(waterfall_low_label), "<b>Waterfall Low: </b>");
  //gtk_widget_override_font(waterfall_low_label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(waterfall_low_label);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_label,col,row,1,1);

  col++;

  GtkWidget *waterfall_low_r=gtk_spin_button_new_with_range(-220.0,100.0,1.0);
  //gtk_widget_override_font(waterfall_low_r, pango_font_description_from_string("Arial 18"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(waterfall_low_r),(double)active_receiver->waterfall_low);
  gtk_widget_show(waterfall_low_r);
  gtk_grid_attach(GTK_GRID(grid),waterfall_low_r,col,row,1,1);
  g_signal_connect(waterfall_low_r,"value_changed",G_CALLBACK(waterfall_low_value_changed_cb),NULL);

  row++;
  col=0;

  GtkWidget *detector_mode_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(detector_mode_label), "<b>Detector</b>");
  gtk_widget_set_halign(detector_mode_label, GTK_ALIGN_START);
  gtk_widget_show(detector_mode_label);
  gtk_grid_attach(GTK_GRID(grid),detector_mode_label,col,row,1,1);

  col++;

  GtkWidget *detector_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(detector_combo), NULL, "Peak");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(detector_combo), NULL, "Rosenfell");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(detector_combo), NULL, "Average");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(detector_combo), NULL, "Sample");
  {
    static const int dmodes[] = {
      DETECTOR_MODE_PEAK,
      DETECTOR_MODE_ROSENFELL,
      DETECTOR_MODE_AVERAGE,
      DETECTOR_MODE_SAMPLE
    };
    int sel = 0;
    int j;
    for (j = 0; j < (int)(sizeof(dmodes) / sizeof(dmodes[0])); j++) {
      if (display_detector_mode == dmodes[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(detector_combo), sel);
  }
  gtk_widget_set_hexpand(detector_combo, TRUE);
  gtk_widget_set_halign(detector_combo, GTK_ALIGN_FILL);
  gtk_widget_show(detector_combo);
  gtk_grid_attach(GTK_GRID(grid),detector_combo,col,row,1,1);
  g_signal_connect(detector_combo, "changed", G_CALLBACK(detector_combo_changed), NULL);

  row++;
  col=0;

  GtkWidget *average_mode_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(average_mode_label), "<b>Averaging</b>");
  gtk_widget_set_halign(average_mode_label, GTK_ALIGN_START);
  gtk_widget_show(average_mode_label);
  gtk_grid_attach(GTK_GRID(grid),average_mode_label,col,row,1,1);

  col++;

  GtkWidget *average_combo=gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(average_combo), NULL, "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(average_combo), NULL, "Recursive");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(average_combo), NULL, "Time window");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(average_combo), NULL, "Log recursive");
  {
    static const int amodes[] = {
      AVERAGE_MODE_NONE,
      AVERAGE_MODE_RECURSIVE,
      AVERAGE_MODE_TIME_WINDOW,
      AVERAGE_MODE_LOG_RECURSIVE
    };
    int sel = 0;
    int j;
    for (j = 0; j < (int)(sizeof(amodes) / sizeof(amodes[0])); j++) {
      if (display_average_mode == amodes[j]) {
        sel = j;
        break;
      }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(average_combo), sel);
  }
  gtk_widget_set_hexpand(average_combo, TRUE);
  gtk_widget_set_halign(average_combo, GTK_ALIGN_FILL);
  gtk_widget_show(average_combo);
  gtk_grid_attach(GTK_GRID(grid),average_combo,col,row,1,1);
  g_signal_connect(average_combo, "changed", G_CALLBACK(average_combo_changed), NULL);

  row++;
  col=0;

  GtkWidget *time_label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(time_label), "<b>Average time (ms)</b>");
  gtk_widget_set_halign(time_label, GTK_ALIGN_START);
  gtk_widget_show(time_label);
  gtk_grid_attach(GTK_GRID(grid),time_label,col,row,1,1);

  col++;

  GtkWidget *time_r=gtk_spin_button_new_with_range(1.0,9999.0,1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(time_r),(double)display_average_time);
  gtk_widget_show(time_r);
  gtk_grid_attach(GTK_GRID(grid),time_r,col,row,1,1);
  g_signal_connect(time_r,"value_changed",G_CALLBACK(time_value_changed_cb),NULL);

  row++;
  col=0;

  /*
   * Display-flag checkboxes.  Putting all five on a single homogeneous
   * row was forcing the grid's natural width to 3 * (widest checkbox)
   * which pushed the whole page off-center compared to the otherwise
   * narrow two-column form.
   *
   * A GtkFlowBox is the right tool: its natural width is just one
   * child wide, so it doesn't widen the grid at all.  At layout time
   * it wraps to fit the viewport -- on the typical Pi screen that
   * comes out as 3 on the first row and 2 on the second, evenly
   * spaced within the row by the column-spacing.  max_children caps
   * it at 3 per line so we never collapse to a single 5-across row
   * even when the window is very wide.
   */
  GtkWidget *check_flow=gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(check_flow), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(check_flow), TRUE);
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(check_flow), 2);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(check_flow), 3);
  gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(check_flow), 4);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(check_flow), 12);
  gtk_widget_set_halign(check_flow, GTK_ALIGN_FILL);

  GtkWidget *b_display_waterfall=gtk_check_button_new_with_label("Display Waterfall");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_waterfall), active_receiver->display_waterfall);
  gtk_container_add(GTK_CONTAINER(check_flow), b_display_waterfall);
  g_signal_connect(b_display_waterfall,"toggled",G_CALLBACK(display_waterfall_cb),(gpointer *)NULL);

  GtkWidget *b_display_zoompan=gtk_check_button_new_with_label("Display Zoom/Pan");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_zoompan), display_zoompan);
  gtk_container_add(GTK_CONTAINER(check_flow), b_display_zoompan);
  g_signal_connect(b_display_zoompan,"toggled",G_CALLBACK(display_zoompan_cb),(gpointer *)NULL);

  GtkWidget *b_display_sliders=gtk_check_button_new_with_label("Display Sliders");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sliders), display_sliders);
  gtk_container_add(GTK_CONTAINER(check_flow), b_display_sliders);
  g_signal_connect(b_display_sliders,"toggled",G_CALLBACK(display_sliders_cb),(gpointer *)NULL);

  GtkWidget *b_display_toolbar=gtk_check_button_new_with_label("Display Toolbar");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_toolbar), display_toolbar);
  gtk_container_add(GTK_CONTAINER(check_flow), b_display_toolbar);
  g_signal_connect(b_display_toolbar,"toggled",G_CALLBACK(display_toolbar_cb),(gpointer *)NULL);

  GtkWidget *b_display_sequence_errors=gtk_check_button_new_with_label("Display Seq Errs");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_display_sequence_errors), display_sequence_errors);
  gtk_container_add(GTK_CONTAINER(check_flow), b_display_sequence_errors);
  g_signal_connect(b_display_sequence_errors,"toggled",G_CALLBACK(display_sequence_errors_cb),(gpointer *)NULL);

  GtkWidget *b_menu_anim=gtk_check_button_new_with_label("Menu Animations");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_menu_anim), pihpsdr_menu_animations);
  gtk_container_add(GTK_CONTAINER(check_flow), b_menu_anim);
  g_signal_connect(b_menu_anim,"toggled",G_CALLBACK(menu_animations_cb),(gpointer *)NULL);

  gtk_grid_attach(GTK_GRID(grid), check_flow, 0, row, 2, 1);

  pihpsdr_dialog_fit_parent(dialog, parent_window);
  pihpsdr_dialog_pack_scrolled_body(dialog, grid);

  sub_menu=dialog;

  gtk_widget_show_all(dialog);

}


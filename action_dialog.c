/*
 * Action picker -- renders inside the menu page system.
 *
 * Historically this was a stand-alone modal GtkDialog run via
 * gtk_dialog_run().  Now that the rest of the menus live as pages in
 * the main window, popping a modal window on top of a page breaks the
 * unified navigation model and (on touch) drops the action picker on
 * the wrong surface.
 *
 * The flow below keeps the synchronous int return signature so the
 * call sites in toolbar_menu / encoder_menu / switch_menu / midi_menu
 * do not have to be rewritten:
 *
 *   1. Build the toggle-button grid in a body box that owns its own
 *      OK / Cancel row.
 *   2. Push the body on the menu-page stack (as a child page above
 *      whichever menu called us).
 *   3. Spin a nested GMainLoop so the caller blocks exactly like
 *      gtk_dialog_run would.
 *   4. OK / Cancel / the page-header Close all quit the loop; we then
 *      pop the page (if it's still on the stack) and return the
 *      selected action, or the original one on cancel.
 */

#include <gtk/gtk.h>

#include "actions.h"
#include "css.h"
#include "menu_page.h"

#define GRID_WIDTH 6

typedef struct _choice {
  int action;
  GtkWidget *button;
  gulong signal_id;
  struct _choice *previous;
} CHOICE;

static GtkWidget *previous_button;
static gulong     previous_signal_id;
static int        selected_action;

/*
 * Nested-loop state.  Only one action picker can be open at a time
 * (the only call sites block until it returns), so these can stay
 * static.
 */
static GMainLoop *action_loop = NULL;
static int        action_response = GTK_RESPONSE_NONE;

static void action_select_cb(GtkWidget *widget, gpointer data) {
  CHOICE *choice = (CHOICE *)data;
  g_signal_handler_block(G_OBJECT(previous_button), previous_signal_id);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(previous_button),
                               widget == previous_button);
  g_signal_handler_unblock(G_OBJECT(previous_button), previous_signal_id);
  previous_button    = widget;
  previous_signal_id = choice->signal_id;
  selected_action    = choice->action;
}

static void action_ok_cb(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  action_response = GTK_RESPONSE_ACCEPT;
  if (action_loop != NULL) {
    g_main_loop_quit(action_loop);
  }
}

static void action_cancel_cb(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  action_response = GTK_RESPONSE_REJECT;
  if (action_loop != NULL) {
    g_main_loop_quit(action_loop);
  }
}

/*
 * If the user closes the page via the header (Back / Close) while
 * the picker is open, we still have to break out of the nested loop
 * or the caller hangs forever.  Treat that as a Cancel.
 */
static void action_page_destroyed_cb(GtkWidget *page, gpointer data) {
  (void)page;
  (void)data;
  if (action_loop != NULL) {
    /* Response stays REJECT unless OK quit us first. */
    if (action_response == GTK_RESPONSE_NONE) {
      action_response = GTK_RESPONSE_REJECT;
    }
    g_main_loop_quit(action_loop);
  }
}

int action_dialog(GtkWidget *parent, int filter, int currentAction) {
  int i;
  int j;
  CHOICE *previous = NULL;
  CHOICE *choice   = NULL;
  GtkWidget *body;
  GtkWidget *grid;
  GtkWidget *btn_row;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *page;

  (void)parent;

  selected_action    = currentAction;
  previous_button    = NULL;
  previous_signal_id = 0;
  action_response    = GTK_RESPONSE_NONE;

  body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_name(body, "pihpsdr_action_page");

  grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

  j = 0;
  for (i = 0; i < ACTIONS; i++) {
    if ((ActionTable[i].type & filter) || (ActionTable[i].type == TYPE_NONE)) {
      GtkWidget *button = gtk_toggle_button_new_with_label(ActionTable[i].str);
      gtk_widget_set_name(button, "small_toggle_button");
      gtk_grid_attach(GTK_GRID(grid), button, j % GRID_WIDTH, j / GRID_WIDTH, 1, 1);
      if (ActionTable[i].action == currentAction) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
      }
      choice = g_new0(CHOICE, 1);
      choice->action    = i;
      choice->button    = button;
      choice->signal_id = g_signal_connect(button, "toggled",
                                           G_CALLBACK(action_select_cb), choice);
      choice->previous  = previous;
      previous          = choice;

      if (ActionTable[i].action == currentAction) {
        previous_button    = button;
        previous_signal_id = choice->signal_id;
      }
      j++;
    }
  }
  gtk_box_pack_start(GTK_BOX(body), grid, TRUE, TRUE, 0);

  /*
   * OK/Cancel row pinned to the bottom of the page body.  Cancel is
   * on the left (secondary); OK on the right (primary, where the
   * thumb lands first on a touch screen).
   */
  btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(btn_row, GTK_ALIGN_CENTER);

  cancel_btn = gtk_button_new_with_label("Cancel");
  gtk_widget_set_name(cancel_btn, "pihpsdr_action_cancel");
  g_signal_connect(cancel_btn, "clicked",
                   G_CALLBACK(action_cancel_cb), NULL);
  gtk_box_pack_start(GTK_BOX(btn_row), cancel_btn, FALSE, FALSE, 0);

  ok_btn = gtk_button_new_with_label("OK");
  gtk_widget_set_name(ok_btn, "pihpsdr_action_ok");
  g_signal_connect(ok_btn, "clicked",
                   G_CALLBACK(action_ok_cb), NULL);
  gtk_box_pack_start(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(body), btn_row, FALSE, FALSE, 0);

  page = pihpsdr_menu_page_push_widget("Action", body);

  if (page != NULL) {
    /*
     * Drive a nested main loop so this function stays synchronous for
     * legacy callers.  The loop exits on OK, Cancel, or page destroy.
     */
    gulong destroy_sig;

    destroy_sig = g_signal_connect(page, "destroy",
                                   G_CALLBACK(action_page_destroyed_cb), NULL);

    action_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(action_loop);
    g_main_loop_unref(action_loop);
    action_loop = NULL;

    /*
     * If the page is still alive (typical: user clicked OK/Cancel),
     * pop it off the stack.  If the page is already being destroyed
     * (user hit Back or Close), it's already on its way out and pop
     * would be redundant/racy -- disconnect instead.
     */
    if (GTK_IS_WIDGET(page) && !gtk_widget_in_destruction(page)) {
      g_signal_handler_disconnect(page, destroy_sig);
      pihpsdr_menu_page_pop(page);
    }
  } else {
    /*
     * Host not installed yet -- drop the orphan widget tree.  In
     * practice this can't happen because every caller runs inside an
     * already-active menu page, but defend against it anyway so this
     * function never leaks or returns uninitialised state.
     */
    g_object_ref_sink(body);
    gtk_widget_destroy(body);
    g_object_unref(body);
    action_response = GTK_RESPONSE_REJECT;
  }

  if (action_response != GTK_RESPONSE_ACCEPT) {
    selected_action = currentAction;
  }

  while (previous != NULL) {
    choice   = previous;
    previous = choice->previous;
    g_free(choice);
  }

  return selected_action;
}

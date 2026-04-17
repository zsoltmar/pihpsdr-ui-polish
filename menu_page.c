/* Copyright (C) 2026
 *
 * Page-based menu navigation for piHPSDR.
 *
 * See menu_page.h for the public API.  Design notes:
 *
 *  - A single GtkStack ("host") becomes the top_window's child.
 *    The radio UI sits on page "radio"; menu pages are added with
 *    generated names "menu-N" and shown with a slide transition.
 *
 *  - A LIFO stack of page widgets mirrors Gtk's visible-child state so
 *    that Back pops exactly one level.  The Close button clears the
 *    stack and returns to radio view in one step.
 *
 *  - Legacy *_menu.c files continue to create a GtkDialog.  Calling
 *    pihpsdr_style_menu_page(dialog) sets the dialog no-show-all and
 *    schedules an idle that, once the menu finishes populating, moves
 *    the dialog's content area onto a freshly pushed page.  The dialog
 *    window never maps; when the menu code later destroys it, the
 *    destroy signal pops the page.
 */

#include <gtk/gtk.h>
#include <stdio.h>

#include "main.h"
#include "menu_page.h"
#include "css.h"

#define DATA_KEY_DIALOG_PAGE   "pihpsdr-menu-page"
#define DATA_KEY_PAGE_DIALOG   "pihpsdr-page-dialog"
#define DATA_KEY_MARKED        "pihpsdr-menu-page-marked"

typedef struct {
  GtkWidget *stack;           /* GtkStack hosting "radio" + menu pages */
  GtkWidget *radio_page;      /* The reparented radio UI (fixed container) */
  GList     *nav;             /* LIFO: most recent page at head */
  guint      next_page_id;
} PageHost;

static PageHost host = {0};

gboolean pihpsdr_menu_animations = TRUE;

/* ------------------------------------------------------------------ */
/* internals                                                           */
/* ------------------------------------------------------------------ */

static const char *
trim_window_title(const char *raw)
{
  static const char *prefix = "piHPSDR - ";
  if (raw == NULL) {
    return "Menu";
  }
  if (g_str_has_prefix(raw, prefix)) {
    return raw + strlen(prefix);
  }
  return raw;
}

static gboolean
page_back_cb(GtkWidget *btn, GdkEventButton *event, gpointer data)
{
  (void)btn;
  (void)event;
  pihpsdr_menu_page_pop(GTK_WIDGET(data));
  return TRUE;
}

static gboolean
page_close_cb(GtkWidget *btn, GdkEventButton *event, gpointer data)
{
  (void)btn;
  (void)event;
  (void)data;
  pihpsdr_menu_page_close_all();
  return TRUE;
}

/*
 * Recursively hide any "Close"/"Cancel" buttons that legacy menu code
 * attached to the body, since the page header now owns dismissal.
 * Hidden + no-show-all means a future gtk_widget_show_all on an
 * ancestor won't pop them back into existence.  We don't destroy them
 * because some menus connect cleanup logic to those button signals;
 * leaving the widget alive keeps the signal targets valid.
 */
static void
hide_inline_dismiss_buttons(GtkWidget *w, gpointer ud)
{
  (void)ud;
  if (GTK_IS_BUTTON(w)) {
    const char *label = gtk_button_get_label(GTK_BUTTON(w));
    if (label != NULL &&
        (g_strcmp0(label, "Close")  == 0 ||
         g_strcmp0(label, "Cancel") == 0)) {
      gtk_widget_set_no_show_all(w, TRUE);
      gtk_widget_hide(w);
      return;
    }
  }
  if (GTK_IS_CONTAINER(w)) {
    gtk_container_foreach(GTK_CONTAINER(w), hide_inline_dismiss_buttons, NULL);
  }
}

/*
 * Some menus call pihpsdr_dialog_pack_scrolled_body which puts their
 * grid inside a GtkScrolledWindow.  Stacking that under our page-level
 * scroll causes two problems:
 *   - vexpand=TRUE on the inner scroll forces it to fill the viewport,
 *     defeating our valign=CENTER on short forms;
 *   - we end up with two scrollbars when the content overflows.
 *
 * Pull the real grid out so the page owns scrolling.  Returns a
 * widget the caller takes a strong reference to (the original `body`
 * is unref'd if we replaced it).
 */
/*
 * Replace any nested GtkScrolledWindow (typically the
 * `pihpsdr_modal_scroll` that pihpsdr_dialog_pack_scrolled_body created)
 * inside `container` with its real inner widget, in place.  Recursive.
 *
 * This is what handles the display_menu / ant_menu / oc_menu case where
 * the dialog's content_area ends up with *multiple* children (the sw
 * plus the dialog's action area).  unwrap_inner_scrolled_top only
 * unwraps when body itself IS the scrolled window; this helper catches
 * every other level.
 */
static void
replace_nested_scrolled(GtkWidget *container)
{
  GList *kids;
  GList *l;

  if (!GTK_IS_CONTAINER(container)) {
    return;
  }
  kids = gtk_container_get_children(GTK_CONTAINER(container));
  for (l = kids; l != NULL; l = l->next) {
    GtkWidget *child = GTK_WIDGET(l->data);

    if (GTK_IS_SCROLLED_WINDOW(child)) {
      GtkWidget *inner = gtk_bin_get_child(GTK_BIN(child));
      GtkWidget *real  = NULL;

      if (inner != NULL) {
        if (GTK_IS_VIEWPORT(inner)) {
          real = gtk_bin_get_child(GTK_BIN(inner));
          if (real != NULL) {
            g_object_ref(real);
            gtk_container_remove(GTK_CONTAINER(inner), real);
          }
        } else {
          real = inner;
          g_object_ref(real);
          gtk_container_remove(GTK_CONTAINER(child), real);
        }
      }

      if (real != NULL) {
        /*
         * Pull the sw out, put real in its place.  For GtkBox we have
         * to preserve pack-position; gtk_container_add won't.  So we
         * record the index, remove sw, and re-insert real at that
         * index.  For non-box containers (GtkGrid etc.) we fall back
         * to gtk_container_add.
         */
        if (GTK_IS_BOX(container)) {
          gtk_container_remove(GTK_CONTAINER(container), child);
          gtk_box_pack_start(GTK_BOX(container), real, TRUE, TRUE, 0);
        } else {
          gtk_container_remove(GTK_CONTAINER(container), child);
          gtk_container_add(GTK_CONTAINER(container), real);
        }
        g_object_unref(real);
        /* Recurse into the newly-placed widget in case it also
         * contains nested scrolled windows. */
        replace_nested_scrolled(real);
      }
    } else {
      replace_nested_scrolled(child);
    }
  }
  g_list_free(kids);
}

static GtkWidget *
unwrap_inner_scrolled(GtkWidget *body)
{
  GtkWidget *child;
  GtkWidget *real;

  if (!GTK_IS_SCROLLED_WINDOW(body)) {
    /* body is not a scroll itself, but may CONTAIN one -- handle that. */
    replace_nested_scrolled(body);
    return body;
  }
  child = gtk_bin_get_child(GTK_BIN(body));
  if (child == NULL) {
    return body;
  }
  /* GtkScrolledWindow auto-inserts a GtkViewport when its child does
   * not natively support scrolling. Either case is handled here. */
  if (GTK_IS_VIEWPORT(child)) {
    real = gtk_bin_get_child(GTK_BIN(child));
    if (real == NULL) {
      return body;
    }
    g_object_ref(real);
    gtk_container_remove(GTK_CONTAINER(child), real);
  } else {
    real = child;
    g_object_ref(real);
    gtk_container_remove(GTK_CONTAINER(body), real);
  }
  g_object_unref(body);
  /* And recurse into real in case it has nested scrolled windows too. */
  replace_nested_scrolled(real);
  return real;
}

/*
 * Build a page wrapper (vertical box) with a header bar and a scrolled
 * content region.  `content` is added to the scrolled region.
 *
 * Header chrome layout:
 *   - Every page: "Close" in the top-right (closes the entire menu
 *                 stack and returns to the radio view).
 *   - Sub-pages:  additionally "‹ Back" in the top-left (pops a single
 *                 page, returning to the parent menu).
 *
 *   The hub has nothing to go back to so its Back slot is omitted.
 */
static GtkWidget *
build_page(const char *title, GtkWidget *content, gboolean is_hub)
{
  GtkWidget *page;
  GtkWidget *header;
  GtkWidget *title_lbl;
  GtkWidget *close;
  GtkWidget *scroll;
  char      *markup;

  page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name(page, "pihpsdr_menu_page");
  gtk_widget_set_hexpand(page, TRUE);
  gtk_widget_set_vexpand(page, TRUE);

  header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name(header, "pihpsdr_menu_page_header");
  gtk_widget_set_hexpand(header, TRUE);

  /*
   * Title first, left-aligned.  Hexpands to eat all the leftover header
   * width, pushing the navigation buttons to the opposite edge.
   */
  title_lbl = gtk_label_new(NULL);
  gtk_widget_set_name(title_lbl, "pihpsdr_menu_page_title");
  markup = g_markup_printf_escaped("<b>%s</b>", title ? title : "Menu");
  gtk_label_set_markup(GTK_LABEL(title_lbl), markup);
  g_free(markup);
  gtk_widget_set_hexpand(title_lbl, TRUE);
  gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
  gtk_box_pack_start(GTK_BOX(header), title_lbl, TRUE, TRUE, 0);

  /*
   * Navigation cluster on the right.  Pack order with pack_end is
   * right-to-left: Back is packed LAST so it ends up rightmost, which
   * puts the most-used action under the thumb for touch.  Close sits
   * just to its left and is the secondary (full-dismiss) action.
   */
  close = gtk_button_new_with_label("Close");
  gtk_widget_set_name(close, "pihpsdr_menu_page_close");
  g_signal_connect(close, "button-press-event",
                   G_CALLBACK(page_close_cb), NULL);
  gtk_box_pack_end(GTK_BOX(header), close, FALSE, FALSE, 0);

  if (!is_hub) {
    GtkWidget *back = gtk_button_new_with_label("\u2039 Back");
    gtk_widget_set_name(back, "pihpsdr_menu_page_back");
    g_signal_connect(back, "button-press-event",
                     G_CALLBACK(page_back_cb), page);
    gtk_box_pack_end(GTK_BOX(header), back, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(page), header, FALSE, FALSE, 0);

  scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_name(scroll, "pihpsdr_menu_page_scroll");
  /*
   * AUTOMATIC: scrollbars appear only when the current page actually
   * overflows -- no gratuitous empty bars on short pages.  Non-overlay
   * so, when they DO appear, they take real layout space at the outer
   * scroll's edge (= the page edge) instead of floating ghost-like
   * over the content and fading in/out as the user moves their hands.
   */
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                      GTK_SHADOW_NONE);
  gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
  gtk_widget_set_hexpand(scroll, TRUE);
  gtk_widget_set_vexpand(scroll, TRUE);

  if (content != NULL) {
    GtkWidget *wrap;

    /*
     * Layout:
     *   scroll -> (auto viewport) -> wrap (vbox) -> content
     *
     * Use the auto-inserted GtkViewport that gtk_container_add() on a
     * GtkScrolledWindow creates for non-Scrollable children.  It is
     * sized to the scroll's inner area, so putting a vbox inside it
     * with halign=CENTER content lands narrow forms (Display, PA,
     * XVTR) in the middle of a full-width viewport rather than glued
     * to the left.  The key is that `wrap` itself must fill the
     * viewport horizontally -- otherwise the viewport shrinks to the
     * wrap's natural width (= content's natural width) and the
     * vertical scrollbar ends up hugging the content's right edge
     * rather than sitting at the page edge, which was exactly the bug
     * on OC and Display.
     */
    wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(wrap, "pihpsdr_menu_page_wrap");
    gtk_widget_set_hexpand(wrap, TRUE);
    gtk_widget_set_vexpand(wrap, TRUE);
    gtk_widget_set_halign(wrap, GTK_ALIGN_FILL);
    gtk_widget_set_valign(wrap, GTK_ALIGN_FILL);

    gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(content, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(content, FALSE);
    gtk_widget_set_vexpand(content, FALSE);
    gtk_widget_set_margin_start(content, 10);
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_top(content, 10);
    gtk_widget_set_margin_bottom(content, 14);

    gtk_box_pack_start(GTK_BOX(wrap), content, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(scroll), wrap);
  }
  gtk_box_pack_start(GTK_BOX(page), scroll, TRUE, TRUE, 0);

  /*
   * Belt-and-braces: force the scroll surface to be at least as wide
   * as the main window.  GtkScrolledWindow's own natural width is
   * tiny when its child is narrow; combined with theme-dependent
   * allocation quirks this can leave the scroll clamped to the
   * child's natural width, putting the vertical scrollbar next to
   * the content instead of at the window edge.  Pinning the minimum
   * request to display_width guarantees the scroll (and therefore
   * its scrollbar) spans the full page width regardless of what the
   * page's content wants to be.
   */
  if (display_width > 0) {
    gtk_widget_set_size_request(scroll, display_width, -1);
  }

  return page;
}

/*
 * When a page is removed from the stack we MUST also tear down the
 * associated GtkDialog so the menu module's own cleanup runs (it stops
 * background timers, clears static `sub_menu` and `dialog` pointers in
 * new_menu.c / *_menu.c, etc.).  Skipping this leaves stale widget
 * pointers in those statics which crash hard on the next access — the
 * classic example being PS's `info_thread` and the `sub_menu` check at
 * the top of new_menu().
 *
 * We do NOT just call gtk_widget_destroy(dialog) directly: most menus
 * connect their cleanup logic to "delete-event", not "destroy".  Going
 * through delete-event lets each module run its proper teardown path
 * (e.g. ps_menu sets running=0, sleeps, then destroys the dialog and
 * clears sub_menu).  We still fall back to destroy if the menu's
 * handler didn't dispose of the dialog itself.
 *
 * Cycle prevention: both data-key links are cleared first so
 * dialog_destroyed_pop_cb (wired on the same dialog) becomes a no-op
 * when the dialog ultimately fires its destroy signal.
 */
static void
page_destroyed_destroy_dialog_cb(GtkWidget *page, gpointer ud)
{
  GtkWidget *dialog;
  (void)ud;
  dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(page), DATA_KEY_PAGE_DIALOG));
  if (dialog == NULL) {
    return;
  }
  g_object_set_data(G_OBJECT(page),   DATA_KEY_PAGE_DIALOG, NULL);
  g_object_set_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE, NULL);

  if (!gtk_widget_in_destruction(dialog)) {
    gboolean  handled = FALSE;
    GdkEvent *evt     = gdk_event_new(GDK_DELETE);
    g_signal_emit_by_name(dialog, "delete-event", evt, &handled);
    gdk_event_free(evt);
  }
  if (GTK_IS_WIDGET(dialog) && !gtk_widget_in_destruction(dialog)) {
    gtk_widget_destroy(dialog);
  }
}

/*
 * Defensive walk that strips height/width caps from any inner
 * GtkScrolledWindow that managed to survive the unwrap step (e.g. menus
 * that nest a scrolled window deeper than top-level).  Without this,
 * pihpsdr_dialog_pack_scrolled_body's max_content_height of 380 px will
 * pin the entire menu inside a tiny clipped pane, exactly the symptom
 * the user reported on RX/Ant/RIGCTL/DSP.
 */
static void
relax_inner_scrolls(GtkWidget *w, gpointer ud)
{
  (void)ud;
  if (GTK_IS_SCROLLED_WINDOW(w)) {
    GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(w);
    /* Ensure any inner scrolls that happen to stay visible don't use
     * the auto-hiding overlay-indicator style either -- consistency
     * with the outer page scroll. */
    gtk_scrolled_window_set_overlay_scrolling(sw, FALSE);
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_scrolled_window_set_propagate_natural_height(sw, TRUE);
    gtk_scrolled_window_set_propagate_natural_width(sw, TRUE);
    gtk_scrolled_window_set_max_content_height(sw, -1);
    gtk_scrolled_window_set_max_content_width(sw, -1);
    gtk_scrolled_window_set_min_content_height(sw, -1);
    gtk_scrolled_window_set_min_content_width(sw, -1);
#endif
  }
  if (GTK_IS_CONTAINER(w)) {
    gtk_container_foreach(GTK_CONTAINER(w), relax_inner_scrolls, NULL);
  }
}

/*
 * GtkNotebook tab centering.
 *
 * GTK3's notebook header has three child regions (packed-start actions,
 * the tabs box, packed-end actions). To center the tabs we install empty
 * hexpand=TRUE boxes into the two action slots; the hbox layout then
 * distributes the leftover header width equally to either side and the
 * tab row ends up dead-centered.
 *
 * Applied universally (from adapt_dialog_idle) so *every* menu page that
 * contains a notebook gets the same tab-centering treatment without each
 * menu having to opt in.
 *
 * Idempotent: skip if an action widget is already present, so re-entering
 * a menu never double-installs spacers.
 */
static void
center_inner_notebooks(GtkWidget *w, gpointer ud)
{
  (void)ud;
  if (GTK_IS_NOTEBOOK(w)) {
    GtkNotebook *nb = GTK_NOTEBOOK(w);
    if (gtk_notebook_get_action_widget(nb, GTK_PACK_START) == NULL) {
      GtkWidget *lspacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_hexpand(lspacer, TRUE);
      gtk_widget_show(lspacer);
      gtk_notebook_set_action_widget(nb, lspacer, GTK_PACK_START);
    }
    if (gtk_notebook_get_action_widget(nb, GTK_PACK_END) == NULL) {
      GtkWidget *rspacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_hexpand(rspacer, TRUE);
      gtk_widget_show(rspacer);
      gtk_notebook_set_action_widget(nb, rspacer, GTK_PACK_END);
    }
  }
  if (GTK_IS_CONTAINER(w)) {
    gtk_container_foreach(GTK_CONTAINER(w), center_inner_notebooks, NULL);
  }
}

/*
 * Transition the stack to whatever page is at the head of `host.nav`,
 * or to the radio page if the nav list is empty.
 */
static void
present_top(void)
{
  GtkStackTransitionType pop_t;
  GtkStackTransitionType push_t;

  if (host.stack == NULL) {
    return;
  }
  pop_t  = pihpsdr_menu_animations ? GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT
                                   : GTK_STACK_TRANSITION_TYPE_NONE;
  push_t = pihpsdr_menu_animations ? GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT
                                   : GTK_STACK_TRANSITION_TYPE_NONE;
  if (host.nav == NULL) {
    gtk_stack_set_transition_type(GTK_STACK(host.stack), pop_t);
    gtk_stack_set_visible_child_name(GTK_STACK(host.stack), "radio");
    return;
  }
  {
    GtkWidget *top = GTK_WIDGET(host.nav->data);
    gtk_stack_set_transition_type(GTK_STACK(host.stack), push_t);
    gtk_stack_set_visible_child(GTK_STACK(host.stack), top);
  }
}

static void
remove_page_from_stack(GtkWidget *page)
{
  if (page == NULL || host.stack == NULL) {
    return;
  }
  /* gtk_container_remove triggers a destroy on floating children; we
   * only hold the page via our nav list, so this cleans it up. */
  if (gtk_widget_get_parent(page) == host.stack) {
    gtk_container_remove(GTK_CONTAINER(host.stack), page);
  }
}

/* ------------------------------------------------------------------ */
/* Public API: host install                                           */
/* ------------------------------------------------------------------ */

void
pihpsdr_menu_page_host_install(GtkWidget *top_window, GtkWidget *radio_root)
{
  if (host.stack != NULL) {
    return; /* already installed */
  }
  if (top_window == NULL || radio_root == NULL) {
    return;
  }

  host.stack = gtk_stack_new();
  gtk_widget_set_name(host.stack, "pihpsdr_page_host");
  gtk_stack_set_transition_duration(GTK_STACK(host.stack), 180);
  gtk_stack_set_homogeneous(GTK_STACK(host.stack), TRUE);
  gtk_widget_set_hexpand(host.stack, TRUE);
  gtk_widget_set_vexpand(host.stack, TRUE);

  /*
   * Move the radio UI from top_window -> stack.radio.
   * We hold a ref during the reparent so the widget survives the
   * brief unparent/add dance.
   */
  g_object_ref(radio_root);
  if (gtk_widget_get_parent(radio_root) == top_window) {
    gtk_container_remove(GTK_CONTAINER(top_window), radio_root);
  }
  gtk_stack_add_named(GTK_STACK(host.stack), radio_root, "radio");
  g_object_unref(radio_root);
  host.radio_page = radio_root;

  gtk_container_add(GTK_CONTAINER(top_window), host.stack);
  gtk_widget_show_all(host.stack);
}

gboolean
pihpsdr_menu_page_host_is_ready(void)
{
  return host.stack != NULL;
}

gboolean
pihpsdr_menu_page_is_showing(void)
{
  return host.nav != NULL;
}

/* ------------------------------------------------------------------ */
/* Public API: push / pop                                             */
/* ------------------------------------------------------------------ */

GtkWidget *
pihpsdr_menu_page_push_widget(const char *title, GtkWidget *content)
{
  GtkWidget *page;
  char       name[32];
  gboolean   is_hub;

  if (!pihpsdr_menu_page_host_is_ready()) {
    return NULL;
  }

  /* The hub is whichever page is at the bottom of the nav stack.
   * If we have nothing on the stack yet, this push is the hub. */
  is_hub = (host.nav == NULL);
  page = build_page(title, content, is_hub);

  g_snprintf(name, sizeof name, "menu-%u", ++host.next_page_id);
  gtk_stack_add_named(GTK_STACK(host.stack), page, name);
  gtk_widget_show_all(page);

  host.nav = g_list_prepend(host.nav, page);
  present_top();

  return page;
}

void
pihpsdr_menu_page_pop(GtkWidget *page)
{
  GList *to_remove = NULL;
  GList *l;

  if (host.nav == NULL) {
    return;
  }

  if (page == NULL) {
    /* Pop just the top. */
    to_remove = g_list_prepend(NULL, host.nav->data);
    host.nav = g_list_delete_link(host.nav, host.nav);
  } else {
    /*
     * Pop everything from head up to and including `page`.
     * If `page` isn't on the stack, do nothing.
     */
    if (g_list_find(host.nav, page) == NULL) {
      return;
    }
    while (host.nav != NULL) {
      GtkWidget *top = GTK_WIDGET(host.nav->data);
      to_remove = g_list_prepend(to_remove, top);
      host.nav = g_list_delete_link(host.nav, host.nav);
      if (top == page) {
        break;
      }
    }
  }

  present_top();

  /*
   * Defer the actual widget destruction until after the transition so
   * the user doesn't see the outgoing page collapse mid-slide.
   */
  for (l = to_remove; l != NULL; l = l->next) {
    remove_page_from_stack(GTK_WIDGET(l->data));
  }
  g_list_free(to_remove);
}

void
pihpsdr_menu_page_close_all(void)
{
  GList *to_remove;

  if (host.nav == NULL) {
    return;
  }
  to_remove = host.nav;
  host.nav = NULL;

  present_top();

  {
    GList *l;
    for (l = to_remove; l != NULL; l = l->next) {
      remove_page_from_stack(GTK_WIDGET(l->data));
    }
  }
  g_list_free(to_remove);
}

/* ------------------------------------------------------------------ */
/* Dialog -> page adapter                                             */
/* ------------------------------------------------------------------ */

static void
dialog_destroyed_pop_cb(GtkWidget *dialog, gpointer user_data)
{
  GtkWidget *page;
  (void)user_data;
  page = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE));
  if (page != NULL) {
    /* Avoid recursion: clear first, then pop. */
    g_object_set_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE, NULL);
    pihpsdr_menu_page_pop(page);
  }
}

/*
 * Move the dialog's populated content area into a new page on the host.
 * Runs on idle so all populating code in the menu builder has finished.
 */
static gboolean
adapt_dialog_idle(gpointer data)
{
  GtkWidget *dialog = GTK_WIDGET(data);
  GtkWidget *content_area;
  GList     *children;
  GtkWidget *body;
  const char *title;
  GtkWidget *page;

  if (!GTK_IS_DIALOG(dialog)) {
    return G_SOURCE_REMOVE;
  }
  if (!pihpsdr_menu_page_host_is_ready()) {
    /* No host yet: let the dialog behave as a legacy modal. */
    gtk_widget_set_no_show_all(dialog, FALSE);
    gtk_widget_show_all(dialog);
    return G_SOURCE_REMOVE;
  }

  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  children = gtk_container_get_children(GTK_CONTAINER(content_area));
  if (children == NULL) {
    return G_SOURCE_REMOVE;
  }

  /*
   * Usually there is a single body child (a grid or a scrolled window
   * from pihpsdr_dialog_pack_scrolled_body).  If there are multiple,
   * stuff them all into a vertical box so nothing is lost.
   */
  if (children->next == NULL) {
    body = GTK_WIDGET(children->data);
    g_object_ref(body);
    gtk_container_remove(GTK_CONTAINER(content_area), body);
  } else {
    GList *l;
    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    for (l = children; l != NULL; l = l->next) {
      GtkWidget *child = GTK_WIDGET(l->data);
      g_object_ref(child);
      gtk_container_remove(GTK_CONTAINER(content_area), child);
      gtk_box_pack_start(GTK_BOX(body), child, TRUE, TRUE, 0);
      g_object_unref(child);
    }
    g_object_ref_sink(body);
  }
  g_list_free(children);

  /* Drop a redundant inner GtkScrolledWindow before we wrap with our
   * own page-level scroll, so vexpand/valign behave predictably. */
  body = unwrap_inner_scrolled(body);

  /* Strip the now-redundant inline Close/Cancel buttons that legacy
   * menu code put inside the body grid. */
  hide_inline_dismiss_buttons(body, NULL);

  /* Belt-and-braces: relax any nested scrolled-window constraints that
   * survived the top-level unwrap (e.g. legacy max_content_height=380
   * caps from pihpsdr_dialog_pack_scrolled_body). */
  relax_inner_scrolls(body, NULL);

  /* Center any GtkNotebook tab strips in their header. */
  center_inner_notebooks(body, NULL);

  title = trim_window_title(gtk_window_get_title(GTK_WINDOW(dialog)));
  page = pihpsdr_menu_page_push_widget(title, body);
  g_object_unref(body);

  if (page != NULL) {
    /*
     * Two-way link between dialog and page so destroying either side
     * tears down the other.  The signal-connect-after-set-data order
     * matters: data must be present before destroy fires.
     */
    g_object_set_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE, page);
    g_object_set_data(G_OBJECT(page),   DATA_KEY_PAGE_DIALOG, dialog);
    g_signal_connect(dialog, "destroy",
                     G_CALLBACK(dialog_destroyed_pop_cb), NULL);
    g_signal_connect(page,   "destroy",
                     G_CALLBACK(page_destroyed_destroy_dialog_cb), NULL);
  }

  return G_SOURCE_REMOVE;
}

void
pihpsdr_style_menu_page(GtkWidget *dialog)
{
  if (dialog == NULL || !GTK_IS_DIALOG(dialog)) {
    return;
  }
  /* Preserve the legacy dark styling for any off-screen dialog state. */
  pihpsdr_style_modal_dialog(dialog);

  if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), DATA_KEY_MARKED))) {
    return;
  }
  g_object_set_data(G_OBJECT(dialog), DATA_KEY_MARKED, GINT_TO_POINTER(1));

  /*
   * Prevent the GtkDialog toplevel from ever mapping.  Setting
   * no-show-all on the dialog short-circuits gtk_widget_show_all(dialog)
   * that the menu builders call at the end.
   */
  gtk_widget_set_no_show_all(dialog, TRUE);

  /*
   * If the host isn't ready yet (extremely early-startup dialogs), bail
   * out gracefully and let the dialog behave modally.
   */
  if (!pihpsdr_menu_page_host_is_ready()) {
    gtk_widget_set_no_show_all(dialog, FALSE);
    return;
  }

  g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                  adapt_dialog_idle, g_object_ref(dialog), g_object_unref);
}

void
pihpsdr_menu_page_pop_for_dialog(GtkWidget *dialog)
{
  GtkWidget *page;
  if (dialog == NULL) {
    return;
  }
  page = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE));
  if (page != NULL) {
    g_object_set_data(G_OBJECT(dialog), DATA_KEY_DIALOG_PAGE, NULL);
    pihpsdr_menu_page_pop(page);
  }
}

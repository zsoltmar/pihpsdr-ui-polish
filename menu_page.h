/* Copyright (C) 2026
 *
 * Page-based menu navigation for piHPSDR.
 *
 * Replaces the legacy modal-dialog menu system with a GtkStack-driven page
 * flow living inside the main window.  The radio UI sits on one stack page;
 * menus slide in as additional pages, with a touch-friendly header bar
 * providing Back / Close navigation.
 *
 * Most legacy *_menu.c files opt in simply by calling
 * pihpsdr_style_menu_page(dialog) in place of pihpsdr_style_modal_dialog(dialog).
 * The dialog is never mapped; its body is reparented into a page on idle.
 */

#ifndef PIHPSDR_MENU_PAGE_H
#define PIHPSDR_MENU_PAGE_H

#include <gtk/gtk.h>

/*
 * Install the page host as the child of `top_window`.
 * `radio_root` is the existing radio UI (typically the `fixed` container).
 * The radio_root is reparented onto the first stack page.
 * Safe to call once, early during create_visual().
 */
void pihpsdr_menu_page_host_install(GtkWidget *top_window, GtkWidget *radio_root);

/*
 * TRUE once the host has been installed.
 */
gboolean pihpsdr_menu_page_host_is_ready(void);

/*
 * Push a menu page with `title` containing `content`.
 * Transitions to the new page.  Returns the page wrapper widget; the caller
 * may later destroy it (or any widget returned as its host) to pop.
 * `content` is wrapped in a scrolled area.
 */
GtkWidget *pihpsdr_menu_page_push_widget(const char *title, GtkWidget *content);

/*
 * Pop `page` (and any pages above it in the navigation stack).
 * If `page` is NULL, pops the topmost.
 */
void pihpsdr_menu_page_pop(GtkWidget *page);

/*
 * Dismiss all menu pages and return the host to the radio view.
 */
void pihpsdr_menu_page_close_all(void);

/*
 * TRUE when the host is currently showing a menu page (not the radio view).
 */
gboolean pihpsdr_menu_page_is_showing(void);

/*
 * Style + adapt a GtkDialog so that it behaves as a menu page.
 *
 * The dialog window itself is prevented from ever mapping; on the next idle
 * its content area is reparented into a fresh page on the host and the
 * dialog's destroy signal is wired to pop that page.
 *
 * Call in place of pihpsdr_style_modal_dialog() in menu builders.  Existing
 * menu code (close buttons that gtk_widget_destroy(dialog), gtk_widget_show_all
 * on the dialog, pihpsdr_dialog_pack_scrolled_body, ...) keeps working.
 */
void pihpsdr_style_menu_page(GtkWidget *dialog);

/*
 * Pop the page that was adapted for `dialog`, if any.
 * Usually not needed directly; gtk_widget_destroy(dialog) already pops.
 */
void pihpsdr_menu_page_pop_for_dialog(GtkWidget *dialog);

/*
 * When TRUE (default), page pushes/pops use a slide transition.
 * When FALSE, transitions are instantaneous.  User-controllable from
 * the Display menu; persisted as "pihpsdr_menu_animations".
 */
extern gboolean pihpsdr_menu_animations;

#endif /* PIHPSDR_MENU_PAGE_H */

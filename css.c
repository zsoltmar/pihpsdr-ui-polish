#include <gtk/gtk.h>
#include <cairo.h>
#include "css.h"

/*
 * piHPSDR UI: Fira Sans / Fira Code, dark iOS-inspired chrome.
 * apt install fonts-firasans fonts-firacode
 */

char *css =
"@define-color ph_bg #0a0c10;\n"
"@define-color ph_surface #141820;\n"
"@define-color ph_elev #1c212c;\n"
"@define-color ph_border #2a3140;\n"
"@define-color ph_fg #f0f3f9;\n"
"@define-color ph_muted #8e98ab;\n"
"@define-color ph_accent #0a84ff;\n"
"@define-color ph_accent_press #0066cc;\n"
"@define-color ph_accent_dim #2563b8;\n"
"@define-color ph_track #252b38;\n"
"@define-color ph_fill #0a84ff;\n"
"/* Slider fill bar (Cairo draw in sliders.c — keep RGB in sync) */\n"
"@define-color ph_slider_fill #3e4652;\n"
"@define-color TOGGLE_ON rgb(86%,24%,30%);\n"
"@define-color TOGGLE_OFF rgb(28%,31%,38%);\n"

"* {\n"
"  font-family: \"Fira Sans\", \"FiraGO\", \"Segoe UI\", Roboto, \"Noto Sans\", sans-serif;\n"
"}\n"

"window {\n"
"  background-color: @ph_bg;\n"
"  color: @ph_fg;\n"
"}\n"

"#pihpsdr_splash_root {\n"
"  background-color: @ph_bg;\n"
"}\n"

"#pihpsdr_splash_title {\n"
"  font-size: 16px;\n"
"  font-weight: 600;\n"
"  color: @ph_fg;\n"
"}\n"

"#pihpsdr_splash_subtitle {\n"
"  font-size: 11px;\n"
"  color: @ph_muted;\n"
"}\n"

"#pihpsdr_splash_status {\n"
"  font-size: 11px;\n"
"  color: @ph_muted;\n"
"}\n"

"dialog,\n"
"messagedialog,\n"
"#pihpsdr_modal_dialog {\n"
"  background-color: @ph_surface;\n"
"  color: @ph_fg;\n"
"  border-radius: 12px;\n"
"}\n"

"#pihpsdr_modal_scroll {\n"
"  background-color: @ph_surface;\n"
"  border: none;\n"
"  padding: 4px 2px;\n"
"}\n"

"/* --- Page host + menu pages ---------------------------------------- */\n"
"#pihpsdr_page_host,\n"
"#pihpsdr_menu_page {\n"
"  background-color: @ph_bg;\n"
"  color: @ph_fg;\n"
"}\n"

"/*\n"
" * Header strip is sized to comfortably fit ~34 px touch targets --\n"
" * matching the Hide/Menu buttons on the radio side of the UI.  The\n"
" * 6px vertical padding + 28px button min-height + 1px bottom border\n"
" * gives a header of about 41px, which reads as a proper title bar.\n"
" */\n"
"#pihpsdr_menu_page_header {\n"
"  background-color: @ph_surface;\n"
"  border-bottom: 1px solid @ph_border;\n"
"  padding: 6px 10px;\n"
"  min-height: 34px;\n"
"}\n"

"#pihpsdr_menu_page_title {\n"
"  font-size: 14px;\n"
"  font-weight: 600;\n"
"  color: @ph_fg;\n"
"  padding: 0 4px;\n"
"}\n"

"/*\n"
" * Both chrome buttons share the base pill look.  Back is emphasised\n"
" * (accent background, slightly larger, no border) because it is the\n"
" * primary action on every sub-page and sits on the outer edge where\n"
" * it's easiest to hit with a thumb.  Close is the secondary action\n"
" * and stays in the quiet elevated-surface style.\n"
" */\n"
"#pihpsdr_menu_page_back,\n"
"#pihpsdr_menu_page_close {\n"
"  background-image: none;\n"
"  color: @ph_fg;\n"
"  border-radius: 8px;\n"
"  margin: 0 0 0 6px;\n"
"  font-size: 13px;\n"
"  font-weight: 600;\n"
"  min-height: 30px;\n"
"  padding: 4px 14px;\n"
"}\n"
"#pihpsdr_menu_page_close {\n"
"  background-color: @ph_elev;\n"
"  border: 1px solid @ph_border;\n"
"}\n"
"#pihpsdr_menu_page_back {\n"
"  background-color: @ph_accent_dim;\n"
"  border: 1px solid @ph_accent;\n"
"  padding: 4px 18px;\n"
"  min-height: 32px;\n"
"  font-size: 14px;\n"
"}\n"

"#pihpsdr_menu_page_close:hover {\n"
"  background-color: @ph_border;\n"
"}\n"
"#pihpsdr_menu_page_back:hover {\n"
"  background-color: @ph_accent;\n"
"}\n"

"#pihpsdr_menu_page_back:active,\n"
"#pihpsdr_menu_page_close:active {\n"
"  background-color: @ph_accent;\n"
"  color: @ph_fg;\n"
"}\n"

/*
 * The scroll surface, its viewport, and the intermediate centering
 * box must ALL carry zero padding and zero margin so the outer
 * scroll's right edge coincides exactly with the window's right
 * edge.  A stray 10-14 px of theme padding anywhere in this chain
 * was what pushed the scrollbar inward on OC/Display and made it
 * look like it sat next to the content's edge instead of the page
 * edge.  Any breathing room the content needs is applied as widget
 * margins on the content itself, not here.
 */
"#pihpsdr_menu_page_scroll {\n"
"  background-color: @ph_bg;\n"
"  border: none;\n"
"  box-shadow: none;\n"
"  padding: 0;\n"
"  margin: 0;\n"
"}\n"

"#pihpsdr_menu_page_scroll viewport,\n"
"#pihpsdr_menu_page_scroll > viewport,\n"
"#pihpsdr_menu_page_viewport {\n"
"  background-color: transparent;\n"
"  background-image: none;\n"
"  border: none;\n"
"  box-shadow: none;\n"
"  padding: 0;\n"
"  margin: 0;\n"
"}\n"

"#pihpsdr_menu_page_wrap {\n"
"  background-color: transparent;\n"
"  background-image: none;\n"
"  border: none;\n"
"  box-shadow: none;\n"
"  padding: 0;\n"
"  margin: 0;\n"
"}\n"

"/*\n"
" * Menu content controls sized for mouse + touch.\n"
" *\n"
" * Scoped to `viewport` descendants so the rules never leak onto the\n"
" * scrollbar's stepper buttons (which sit outside the viewport).\n"
" *\n"
" * Targets ~32 px hit height — comfortable for a finger on a 7\" Pi\n"
" * touchscreen without dominating the page like a phone-style 48 px\n"
" * button would.\n"
" */\n"
"#pihpsdr_menu_page_scroll viewport button,\n"
"#pihpsdr_menu_page_scroll viewport combobox button {\n"
"  min-height: 28px;\n"
"  padding: 6px 14px;\n"
"  font-size: 13px;\n"
"}\n"

"#pihpsdr_menu_page_scroll viewport combobox,\n"
"#pihpsdr_menu_page_scroll viewport entry,\n"
"#pihpsdr_menu_page_scroll viewport spinbutton {\n"
"  min-height: 28px;\n"
"  padding: 4px 8px;\n"
"  font-size: 13px;\n"
"}\n"

"#pihpsdr_menu_page_scroll viewport spinbutton button {\n"
"  min-width: 26px;\n"
"  min-height: 26px;\n"
"  padding: 0 6px;\n"
"}\n"

"#pihpsdr_menu_page_scroll viewport label {\n"
"  font-size: 13px;\n"
"}\n"

"#pihpsdr_menu_page_scroll viewport checkbutton,\n"
"#pihpsdr_menu_page_scroll viewport radiobutton {\n"
"  font-size: 13px;\n"
"  min-height: 26px;\n"
"}\n"

"/*\n"
" * Polished, touch-friendly scrollbar.\n"
" *\n"
" *   gutter  = 22 px\n"
" *   stepper = 18x18 square (border-radius 6) + 2 px margin on all sides\n"
" *   slider  = 18 px wide by design, rounded 9 px ends, 2 px margin\n"
" *\n"
" * Everything stays inside the 22 px gutter; the slider dragger lines\n"
" * up exactly with the two stepper buttons.\n"
" */\n"
"#pihpsdr_menu_page_scroll scrollbar {\n"
"  background-color: @ph_surface;\n"
"  border: none;\n"
"  opacity: 1;\n"
"  -gtk-icon-source: none;\n"
"}\n"

/*
 * Defeat the GTK overlay-indicator scrollbar style.  Even with
 * gtk_scrolled_window_set_overlay_scrolling(FALSE), some themes keep
 * the .overlay-indicator CSS class on the scrollbar and render it as
 * a translucent 4 px-tall pill that only fattens up when the pointer
 * is over it.  On a touchscreen that means you literally cannot see
 * or hit the scrollbar until you poke the content first, which was
 * exactly the complaint on the OC page.  Forcing min-size + opacity
 * here gives us a proper chunky always-visible scrollbar at the page
 * edge regardless of what the theme thinks.
 */
"#pihpsdr_menu_page_scroll scrollbar.vertical,\n"
"#pihpsdr_menu_page_scroll scrollbar.vertical.overlay-indicator {\n"
"  min-width: 22px;\n"
"  border-left: 1px solid @ph_border;\n"
"  opacity: 1;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.horizontal,\n"
"#pihpsdr_menu_page_scroll scrollbar.horizontal.overlay-indicator {\n"
"  min-height: 22px;\n"
"  border-top: 1px solid @ph_border;\n"
"  opacity: 1;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.overlay-indicator {\n"
"  background-color: @ph_surface;\n"
"  opacity: 1;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.overlay-indicator trough,\n"
"#pihpsdr_menu_page_scroll scrollbar.overlay-indicator slider {\n"
"  opacity: 1;\n"
"  min-width: 18px;\n"
"  min-height: 18px;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.overlay-indicator.dragging,\n"
"#pihpsdr_menu_page_scroll scrollbar.overlay-indicator.hovering {\n"
"  opacity: 1;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar trough {\n"
"  background-color: transparent;\n"
"  border: none;\n"
"  margin: 0;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar slider {\n"
"  background-color: @ph_border;\n"
"  border: none;\n"
"  border-radius: 9px;\n"
"  margin: 2px;\n"
"  transition: background-color 120ms ease;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.vertical slider {\n"
"  min-width: 18px;\n"
"  min-height: 44px;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar.horizontal slider {\n"
"  min-height: 18px;\n"
"  min-width: 44px;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar slider:hover {\n"
"  background-color: @ph_muted;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar slider:active {\n"
"  background-color: @ph_accent;\n"
"}\n"

"/* Stepper buttons (when the active theme draws them) */\n"
"#pihpsdr_menu_page_scroll scrollbar button {\n"
"  background-color: @ph_elev;\n"
"  color: @ph_fg;\n"
"  border: 1px solid @ph_border;\n"
"  border-radius: 6px;\n"
"  padding: 0;\n"
"  margin: 2px;\n"
"  min-width: 18px;\n"
"  min-height: 18px;\n"
"  transition: background-color 120ms ease;\n"
"  -GtkScrollbar-has-backward-stepper: 1;\n"
"  -GtkScrollbar-has-forward-stepper: 1;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar button:hover {\n"
"  background-color: @ph_border;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar button:active {\n"
"  background-color: @ph_accent_dim;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar button:disabled {\n"
"  background-color: @ph_surface;\n"
"  color: @ph_muted;\n"
"}\n"

"#pihpsdr_menu_page_scroll scrollbar button image {\n"
"  -gtk-icon-transform: scale(0.7);\n"
"}\n"

"#pihpsdr_menu_page_scroll junction {\n"
"  background-color: @ph_surface;\n"
"  border-left: 1px solid @ph_border;\n"
"  border-top: 1px solid @ph_border;\n"
"}\n"

"/*\n"
" * VOX mic-level progress bar: same palette as the fill slider so it reads\n"
" * as \"live slider mirror\" rather than a foreign green progressbar.\n"
" */\n"
"#pihpsdr_mic_meter {\n"
"  min-height: 16px;\n"
"}\n"
"#pihpsdr_mic_meter trough {\n"
"  min-height: 16px;\n"
"  border-radius: 9px;\n"
"  background-color: @ph_track;\n"
"  border: none;\n"
"  margin: 0;\n"
"  padding: 0;\n"
"}\n"
"#pihpsdr_mic_meter progress {\n"
"  min-height: 16px;\n"
"  border-radius: 7px;\n"
"  background-image: none;\n"
"  background-color: @ph_fill;\n"
"  border: none;\n"
"  margin: 2px;\n"
"  padding: 0;\n"
"}\n"

"/*\n"
" * Notebook (tabbed content) used by the PA menu.  The default GTK\n"
" * notebook has a hard rectangular border and flat rectangular tabs; we\n"
" * dress it up as a rounded container with a soft header strip that holds\n"
" * rounded pill tabs.\n"
" */\n"
"#pihpsdr_menu_notebook {\n"
"  background-color: transparent;\n"
"  border: none;\n"
"  padding: 0;\n"
"}\n"
"#pihpsdr_menu_notebook > header {\n"
"  background-color: @ph_elev;\n"
"  border: 1px solid @ph_border;\n"
"  border-radius: 10px;\n"
"  padding: 4px;\n"
"  margin: 0 0 6px 0;\n"
"  min-height: 0;\n"
"}\n"
"#pihpsdr_menu_notebook > header.top { border-bottom: 1px solid @ph_border; }\n"
"#pihpsdr_menu_notebook > header tabs {\n"
"  background: transparent;\n"
"  border: none;\n"
"  margin: 0;\n"
"  padding: 0;\n"
"}\n"
"#pihpsdr_menu_notebook > header tab {\n"
"  background-color: transparent;\n"
"  color: @ph_muted;\n"
"  border: none;\n"
"  border-radius: 7px;\n"
"  padding: 6px 14px;\n"
"  margin: 0 2px;\n"
"  min-height: 0;\n"
"  transition: background-color 120ms ease, color 120ms ease;\n"
"}\n"
"#pihpsdr_menu_notebook > header tab:hover {\n"
"  background-color: alpha(@ph_fg, 0.06);\n"
"  color: @ph_fg;\n"
"}\n"
"#pihpsdr_menu_notebook > header tab:checked {\n"
"  background-color: @ph_surface;\n"
"  color: @ph_fg;\n"
"  box-shadow: 0 1px 2px alpha(black, 0.25);\n"
"}\n"
"#pihpsdr_menu_notebook > header tab label {\n"
"  color: inherit;\n"
"  font-size: 13px;\n"
"  padding: 0;\n"
"}\n"
"#pihpsdr_menu_notebook > stack {\n"
"  background-color: transparent;\n"
"  border: none;\n"
"  /* Breathing room so tab body content doesn't butt the tab header or\n"
"   * the card edge below (Toolbar-menu preview buttons symptom). */\n"
"  padding: 14px 6px 10px 6px;\n"
"}\n"

"/*\n"
" * Toolbar (bottom dashboard button row).  Buttons are attached in a\n"
" * GtkGrid with 2 px column spacing set in C; we just ensure no heavy\n"
" * borders bleed into the gutter we carved out.\n"
" */\n"
"#pihpsdr_toolbar {\n"
"  background-color: transparent;\n"
"  padding: 0;\n"
"  margin: 0;\n"
"}\n"

"/*\n"
" * Bottom-dashboard buttons are shaped like ears: rounded along the top,\n"
" * flat along the bottom so they read as \"attached to the window edge\".\n"
" * The 2 px strip of empty space below the toolbar (carved out in\n"
" * toolbar_init) makes this readable without the buttons touching the\n"
" * window frame.\n"
" *\n"
" * The Toolbar Actions menu page renders a preview of this same row\n"
" * inside a notebook tab (see toolbar_menu.c); share the styling so the\n"
" * preview and the live toolbar look identical.\n"
" */\n"
"#pihpsdr_toolbar button,\n"
"#pihpsdr_toolbar_preview button {\n"
"  border-top-left-radius: 10px;\n"
"  border-top-right-radius: 10px;\n"
"  border-bottom-left-radius: 0;\n"
"  border-bottom-right-radius: 0;\n"
"}\n"

"/*\n"
" * Defensive blanket: legacy menu builders emit plain `box`, `grid`,\n"
" * `frame`, and `scrolledwindow` containers that would otherwise pick\n"
" * up the GTK theme's default @theme_bg_color / dialog-body shading\n"
" * and draw as faint rectangular panels inside our page (the FFT\n"
" * symptom).  Force the whole subtree transparent; interactive widgets\n"
" * (buttons, combos, entries, spins, the pihpsdr_menu_notebook header)\n"
" * have their own explicit backgrounds and keep them because of their\n"
" * higher CSS specificity.\n"
" */\n"
"#pihpsdr_menu_page_wrap,\n"
"#pihpsdr_menu_page_scroll viewport box,\n"
"#pihpsdr_menu_page_scroll viewport grid,\n"
"#pihpsdr_menu_page_scroll viewport frame,\n"
"#pihpsdr_menu_page_scroll viewport frame > border,\n"
"#pihpsdr_menu_page_scroll viewport scrolledwindow,\n"
"#pihpsdr_menu_page_scroll viewport paned,\n"
"#pihpsdr_menu_page_scroll viewport paned > separator,\n"
"#pihpsdr_menu_page_scroll viewport stack,\n"
"#pihpsdr_menu_page_scroll viewport stack > * {\n"
"  background-color: transparent;\n"
"  background-image: none;\n"
"  border: none;\n"
"  box-shadow: none;\n"
"}\n"

"/*\n"
" * Any generic notebook (one that isn't explicitly themed as\n"
" * pihpsdr_menu_notebook) ended up with @ph_surface from the blanket\n"
" * `notebook` rule further below, which reads as a heavy panel inside\n"
" * our otherwise flat pages.  Match the pihpsdr_menu_notebook treatment\n"
" * for any other notebooks that happen to land on a menu page so the\n"
" * whole app shares one visual language for tabbed content.\n"
" */\n"
"#pihpsdr_menu_page_scroll viewport notebook,\n"
"#pihpsdr_menu_page_scroll viewport notebook > stack {\n"
"  background-color: transparent;\n"
"  border: none;\n"
"}\n"

"/*\n"
" * Vertical breathing room on the notebook itself so its header/body\n"
" * never butt up against the card edge or siblings above/below it.\n"
" */\n"
"#pihpsdr_menu_page_scroll viewport notebook {\n"
"  margin: 4px 0 10px 0;\n"
"}\n"

"#pihpsdr_modal_scroll viewport {\n"
"  background-color: transparent;\n"
"}\n"

"#pihpsdr_discovery_dialog {\n"
"  background-color: @ph_surface;\n"
"}\n"

"#pihpsdr_discovery_label {\n"
"  color: @ph_fg;\n"
"}\n"

"dialog label,\n"
"messagedialog label {\n"
"  color: @ph_fg;\n"
"}\n"

"/* GtkDialog content/action areas (avoid theme light grey under dark chrome) */\n"
"dialog box {\n"
"  background-color: @ph_surface;\n"
"  padding: 12px 16px;\n"
"}\n"

"#pihpsdr_discovery_dialog > box {\n"
"  padding: 12px 16px;\n"
"}\n"

"dialog viewport {\n"
"  background-color: transparent;\n"
"}\n"

"/*\n"
" * Blanket notebook styling for dialogs / discovery / anything that\n"
" * hasn't opted into pihpsdr_menu_notebook explicitly.  Scoped tight\n"
" * enough that menu pages override it via the transparent rules above.\n"
" */\n"
"dialog notebook,\n"
"messagedialog notebook,\n"
"dialog notebook header tabs,\n"
"dialog notebook stack,\n"
"messagedialog notebook header tabs,\n"
"messagedialog notebook stack {\n"
"  background-color: @ph_surface;\n"
"  color: @ph_fg;\n"
"}\n"

"popover {\n"
"  background-color: @ph_elev;\n"
"  color: @ph_fg;\n"
"  border: 1px solid @ph_border;\n"
"}\n"

"label {\n"
"  color: @ph_fg;\n"
"}\n"

"#pihpsdr_main {\n"
"  font-family: \"Fira Sans\", \"FiraGO\", \"Noto Sans\", sans-serif;\n"
"}\n"

"#pihpsdr_sliders_grid,\n"
"#pihpsdr_zoompan {\n"
"  font-family: \"Fira Sans\", \"FiraGO\", sans-serif;\n"
"}\n"

"#pihpsdr_slider_overlay_row {\n"
"  background-color: transparent;\n"
"}\n"

"#pihpsdr_slider_inline_left {\n"
"  font-family: \"Fira Sans\", \"FiraGO\", sans-serif;\n"
"  font-size: 11px;\n"
"  font-weight: 700;\n"
"  color: @ph_fg;\n"
"  text-shadow: 0 0 3px alpha(black,0.95),\n"
"               0 1px 2px alpha(black,0.85);\n"
"}\n"

"#pihpsdr_slider_inline_value {\n"
"  font-family: \"Fira Code\", \"DejaVu Sans Mono\", monospace;\n"
"  font-size: 11px;\n"
"  font-weight: 600;\n"
"  color: @ph_fg;\n"
"  text-shadow: 0 0 3px alpha(black,0.95),\n"
"               0 1px 2px alpha(black,0.85);\n"
"}\n"

"#pihpsdr_fill_scale {\n"
"  padding: 1px 0;\n"
"  margin: 0;\n"
"}\n"

"/* Tighter vertical rhythm; zoom + dashboard use the same fill-scale chrome */\n"
"#pihpsdr_zoompan #pihpsdr_fill_scale,\n"
"#pihpsdr_sliders_grid #pihpsdr_fill_scale {\n"
"  padding: 0;\n"
"}\n"

"#pihpsdr_sliders_grid {\n"
"  background-color: @ph_bg;\n"
"  padding: 0 12px;\n"
"}\n"

"#pihpsdr_zoompan {\n"
"  background-color: @ph_bg;\n"
"  padding: 0 12px;\n"
"}\n"

"#pihpsdr_filter_grid button {\n"
"  font-size: 10px;\n"
"  font-weight: 600;\n"
"  min-height: 26px;\n"
"  min-width: 0;\n"
"  padding: 3px 8px;\n"
"}\n"

"#pihpsdr_chrome_btn {\n"
"  font-family: \"Fira Sans\", \"FiraGO\", \"Segoe UI\", sans-serif;\n"
"  font-size: 10px;\n"
"  font-weight: 600;\n"
"  min-height: 0;\n"
"  min-width: 0;\n"
"  padding: 4px 8px;\n"
"  margin: 0;\n"
"  border-radius: 8px;\n"
"}\n"

"button {\n"
"  font-size: 11px;\n"
"  font-weight: 600;\n"
"  min-height: 30px;\n"
"  min-width: 32px;\n"
"  padding: 5px 11px;\n"
"  border-radius: 10px;\n"
"  border: 1px solid @ph_border;\n"
"  background-image: none;\n"
"  background-color: @ph_elev;\n"
"  color: @ph_fg;\n"
"  transition: background-color 90ms ease, opacity 90ms ease, transform 90ms ease;\n"
"}\n"

"button:hover {\n"
"  background-color: shade(@ph_elev,1.1);\n"
"  border-color: shade(@ph_accent,0.7);\n"
"}\n"

"button:active {\n"
"  background-color: @ph_accent_press;\n"
"  border-color: shade(@ph_accent_press,0.9);\n"
"  color: white;\n"
"  opacity: 0.96;\n"
"}\n"

"button:checked {\n"
"  background-color: @ph_accent_dim;\n"
"  color: white;\n"
"}\n"

"button:focus {\n"
"  outline: 2px solid alpha(@ph_accent,0.4);\n"
"  outline-offset: 1px;\n"
"}\n"

"button:disabled {\n"
"  color: @ph_muted;\n"
"  opacity: 0.45;\n"
"}\n"

"scale {\n"
"  padding: 1px 2px;\n"
"}\n"

"scale slider {\n"
"  min-height: 20px;\n"
"  min-width: 20px;\n"
"  margin: -6px;\n"
"}\n"

"scale trough {\n"
"  background-color: @ph_track;\n"
"  border-radius: 6px;\n"
"  border: none;\n"
"}\n"

"scale highlight {\n"
"  background-image: none;\n"
"  background-color: alpha(@ph_fill,0.5);\n"
"  border-radius: 6px;\n"
"}\n"

"#pihpsdr_fill_scale slider {\n"
"  background-color: transparent;\n"
"  background-image: none;\n"
"  border: none;\n"
"  box-shadow: none;\n"
"  color: transparent;\n"
"  min-height: 26px;\n"
"  min-width: 30px;\n"
"  margin: -9px -8px;\n"
"}\n"

"#pihpsdr_fill_scale trough {\n"
"  min-height: 31px;\n"
"  border-radius: 9px;\n"
"  background-color: @ph_track;\n"
"}\n"

"/* Fill drawn in code (sliders.c); theme highlight cannot enforce min width + pill radius */\n"
"#pihpsdr_fill_scale highlight {\n"
"  opacity: 0;\n"
"  min-width: 0;\n"
"  min-height: 0;\n"
"  margin: 0;\n"
"}\n"

"entry,\n"
"spinbutton {\n"
"  font-size: 11px;\n"
"  padding: 5px 8px;\n"
"  min-height: 28px;\n"
"  border-radius: 8px;\n"
"  border: 1px solid @ph_border;\n"
"  background-color: @ph_bg;\n"
"  color: @ph_fg;\n"
"}\n"

"combobox button {\n"
"  font-size: 10px;\n"
"}\n"

"checkbutton,\n"
"radiobutton {\n"
"  font-size: 11px;\n"
"  padding: 3px 4px;\n"
"}\n"

"checkbutton check,\n"
"radiobutton radio {\n"
"  min-width: 20px;\n"
"  min-height: 20px;\n"
"  border-radius: 5px;\n"
"  border: 1.5px solid @ph_border;\n"
"  background-color: @ph_elev;\n"
"}\n"

"radiobutton radio {\n"
"  border-radius: 10px;\n"
"}\n"

"checkbutton check:checked,\n"
"radiobutton radio:checked {\n"
"  background-color: @ph_accent;\n"
"  border-color: shade(@ph_accent,0.85);\n"
"}\n"

"checkbutton label,\n"
"radiobutton label {\n"
"  padding-left: 4px;\n"
"}\n"

"scrollbar slider {\n"
"  min-width: 8px;\n"
"  min-height: 8px;\n"
"  border-radius: 4px;\n"
"  background-color: shade(@ph_border,0.9);\n"
"}\n"

"#small_button {\n"
"  font-size: 11px;\n"
"  min-height: 32px;\n"
"  min-width: 36px;\n"
"  padding: 5px 9px;\n"
"  border-radius: 10px;\n"
"}\n"

"#small_toggle_button {\n"
"  font-size: 11px;\n"
"  min-height: 32px;\n"
"  min-width: 36px;\n"
"  padding: 5px 9px;\n"
"  border-radius: 10px;\n"
"  background-image: none;\n"
"  background-color: @TOGGLE_OFF;\n"
"  color: @ph_fg;\n"
"  border: 1px solid @ph_border;\n"
"}\n"

"#small_toggle_button:checked {\n"
"  background-image: none;\n"
"  background-color: @TOGGLE_ON;\n"
"  color: white;\n"
"  border-color: shade(@TOGGLE_ON,0.85);\n"
"}\n"
;

void
pihpsdr_gdk_rgba_modal_surface(GdkRGBA *rgba)
{
  /* Matches @ph_surface #141820 */
  rgba->red = 20.0 / 255.0;
  rgba->green = 24.0 / 255.0;
  rgba->blue = 32.0 / 255.0;
  rgba->alpha = 1.0;
}

void
pihpsdr_dialog_dark_background(GtkWidget *dialog)
{
  GdkRGBA c;

  pihpsdr_gdk_rgba_modal_surface(&c);
  gtk_widget_override_background_color(dialog, GTK_STATE_FLAG_NORMAL, &c);
}

void
pihpsdr_style_modal_dialog(GtkWidget *dialog)
{
  gtk_widget_set_name(dialog, "pihpsdr_modal_dialog");
  pihpsdr_dialog_dark_background(dialog);
}

void
pihpsdr_cairo_text_rendering(cairo_t *cr)
{
  cairo_font_options_t *opt;

  /*
   * Gray AA + default hints: subpixel/RGB hinting can corrupt glyphs on
   * VNC / some Wayland compositors; keep text readable everywhere.
   */
  opt = cairo_font_options_create();
  cairo_font_options_set_antialias(opt, CAIRO_ANTIALIAS_GRAY);
  cairo_font_options_set_hint_style(opt, CAIRO_HINT_STYLE_DEFAULT);
  cairo_set_font_options(cr, opt);
  cairo_font_options_destroy(opt);
}

void load_css(void) {
  GtkSettings *settings;

  g_print("%s\n", __FUNCTION__);

  settings = gtk_settings_get_default();
  if (settings != NULL) {
    g_object_set(settings,
        "gtk-font-name", "Fira Sans 10",
        "gtk-application-prefer-dark-theme", TRUE,
        "gtk-xft-antialias", 1,
        "gtk-xft-hinting", 1,
        "gtk-xft-hintstyle", "hintslight",
        "gtk-xft-rgba", "none",
        NULL);
  }

  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  provider = gtk_css_provider_new();
  display = gdk_display_get_default();
  screen = gdk_display_get_default_screen(display);
  gtk_style_context_add_provider_for_screen(screen,
      GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  {
    GError *err = NULL;

    if (!gtk_css_provider_load_from_data(provider, css, -1, &err)) {
      g_warning("piHPSDR: CSS did not load (%s). Theme styling may be missing.",
          err ? err->message : "unknown error");
      g_clear_error(&err);
    }
  }
  g_object_unref(provider);
}

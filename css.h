#include <cairo.h>
#include <gtk/gtk.h>

extern char *css;
extern void load_css(void);
void pihpsdr_cairo_text_rendering(cairo_t *cr);

void pihpsdr_gdk_rgba_modal_surface(GdkRGBA *rgba);
void pihpsdr_dialog_dark_background(GtkWidget *dialog);
void pihpsdr_style_modal_dialog(GtkWidget *dialog);


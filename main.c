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

// Define maximum window size. 
// Standard values 800 and 480: suitable for RaspberryBi 7-inch screen
// 1280 and 720 for Display 2
#define DISPLAY2

#ifdef DISPLAY2
#define MAX_DISPLAY_WIDTH  1280
#define MAX_DISPLAY_HEIGHT 800
#else
#define MAX_DISPLAY_WIDTH  800
#define MAX_DISPLAY_HEIGHT 480
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "audio.h"
#include "band.h"
#include "bandstack.h"
#include "main.h"
#include "channel.h"
#include "discovered.h"
#include "configure.h"
#include "actions.h"
#ifdef GPIO
#include "gpio.h"
#endif
#include "wdsp.h"
#include "new_menu.h"
#include "radio.h"
#include "version.h"
#include "button_text.h"
#ifdef I2C
#include "i2c.h"
#endif
#include "discovery.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "protocols.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "ext.h"
#include "vfo.h"
#include "css.h"
#include "paths.h"

struct utsname unameData;

gint display_width;
gint display_height;
gint full_screen=1;

static GtkWidget *discovery_dialog;

static GdkCursor *cursor_arrow;
static GdkCursor *cursor_watch;

static GtkWidget *splash;

GtkWidget *top_window;
GtkWidget *grid;

static DISCOVERED* d;

static GtkWidget *status;

void status_text(char *text) {
  //fprintf(stderr,"splash_status: %s\n",text);
  gtk_label_set_text(GTK_LABEL(status),text);
  usleep(100000);
  while (gtk_events_pending ())
    gtk_main_iteration ();
}

static gint save_cb(gpointer data) {
    radioSaveState();
    return TRUE;
}

static pthread_t wisdom_thread_id;
static int wisdom_running=0;

static void* wisdom_thread(void *arg) {
  WDSPwisdom ((char *)arg);
  wisdom_running=0;
  return NULL;
}

//
// handler for key press events.
// SpaceBar presses toggle MOX, everything else downstream
// code to switch mox copied from mox_cb() in toolbar.c,
// but added the correct return values.
//
gboolean keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

   if (radio != NULL) {
	  if (event->keyval == GDK_KEY_space) {
		  
		  fprintf(stderr, "space");
		  
		if(getTune()==1) {
		  setTune(0);
		}
		if(getMox()==1) {
		  setMox(0);
		} else if(canTransmit() || tx_out_of_band) {
		  setMox(1);
		} else {
		  transmitter_set_out_of_band(transmitter);
		}
		g_idle_add(ext_vfo_update,NULL);
		return TRUE;
	  }
	  if (event->keyval == GDK_KEY_d ) {
		vfo_move(step,TRUE);
		return TRUE;
	  }
	  if (event->keyval == GDK_KEY_u ) {
		 vfo_move(-step,TRUE);
		 return TRUE;
	  }
  }
  
  return FALSE;
}

//
// Async-signal-safe radio quiet: send a single P1 metis-stop broadcast
// (EF FE 04 00, padded to 64 bytes) on UDP/1024. Uses only socket(),
// setsockopt(), bind(), sendto(), close() -- all safe to call from a
// signal handler. Does NOT touch GTK, malloc, or any global app state.
//
// Called from the SIGINT/SIGTERM/SIGSEGV/SIGABRT/SIGBUS handler so that
// a crashing piHPSDR still tells the radio to stop streaming. Without
// this, after a crash the radio firmware keeps spamming UDP packets to
// our dead endpoint and the *next* startup hangs in discovery (see also
// protocols_quiet_radios() which does the same thing on every start).
//
static void emergency_quiet_radios_signal_safe(void) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    return;
  }

  int on = 1;
  (void) setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family      = AF_INET;
  bind_addr.sin_port        = htons(0);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
    close(sock);
    return;
  }

  unsigned char metis_stop[64];
  memset(metis_stop, 0, sizeof(metis_stop));
  metis_stop[0] = 0xEF;
  metis_stop[1] = 0xFE;
  metis_stop[2] = 0x04;
  metis_stop[3] = 0x00;

  struct sockaddr_in to_addr;
  memset(&to_addr, 0, sizeof(to_addr));
  to_addr.sin_family      = AF_INET;
  to_addr.sin_port        = htons(1024);
  to_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  (void) sendto(sock, metis_stop, sizeof(metis_stop), 0,
                (struct sockaddr *)&to_addr, sizeof(to_addr));

  close(sock);
}

//
// Single signal handler for both clean shutdown signals (SIGINT, SIGTERM,
// SIGHUP) and crash signals (SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL).
//
// All it does is fire one async-signal-safe metis-stop broadcast and then
// either _exit() (clean signals) or restore the default handler and
// re-raise (crash signals, so we still get a core dump / parent shell sees
// the right exit code).
//
static void pihpsdr_signal_handler(int sig) {
  emergency_quiet_radios_signal_safe();

  switch (sig) {
    case SIGINT:
    case SIGTERM:
    case SIGHUP:
      _exit(128 + sig);

    case SIGSEGV:
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
    default:
      // Restore the default handler and re-raise so we still get the
      // normal crash behaviour (core dump, $? = 128+sig in the shell).
      signal(sig, SIG_DFL);
      raise(sig);
      break;
  }
}

static void install_pihpsdr_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = pihpsdr_signal_handler;
  sigemptyset(&sa.sa_mask);
  // Reset to default after firing once -- we don't want to recurse if our
  // handler itself segfaults.
  sa.sa_flags = SA_RESETHAND | SA_NODEFER;

  (void) sigaction(SIGINT,  &sa, NULL);
  (void) sigaction(SIGTERM, &sa, NULL);
  (void) sigaction(SIGHUP,  &sa, NULL);
  (void) sigaction(SIGSEGV, &sa, NULL);
  (void) sigaction(SIGABRT, &sa, NULL);
  (void) sigaction(SIGBUS,  &sa, NULL);
  (void) sigaction(SIGFPE,  &sa, NULL);
  (void) sigaction(SIGILL,  &sa, NULL);

  // SIGPIPE on a broken socket should not kill us.
  signal(SIGPIPE, SIG_IGN);
}

gboolean main_delete (GtkWidget *widget) {
  if(radio!=NULL) {
#ifdef GPIO
    gpio_close();
#endif
#ifdef CLIENT_SERVER
    if(!radio_is_remote) {
#endif
      switch(protocol) {
        case ORIGINAL_PROTOCOL:
          old_protocol_stop();
          break;
        case NEW_PROTOCOL:
          new_protocol_stop();
          break;
#ifdef SOAPYSDR
        case SOAPYSDR_PROTOCOL:
          soapy_protocol_stop();
          break;
#endif
      }
#ifdef CLIENT_SERVER
    }
#endif
    radioSaveState();
  }
  _exit(0);
}

static int init(void *data) {
  char wisdom_directory[1024];

  g_print("%s\n",__FUNCTION__);

  audio_get_cards();

  // wait for get_cards to complete
  //g_mutex_lock(&audio_mutex);
  //g_mutex_unlock(&audio_mutex);

  cursor_arrow=gdk_cursor_new(GDK_ARROW);
  cursor_watch=gdk_cursor_new(GDK_WATCH);

  gdk_window_set_cursor(gtk_widget_get_window(top_window),cursor_watch);

  //
  // Let WDSP (via FFTW) check for wisdom file in current dir
  // If there is one, the "wisdom thread" takes no time
  // Depending on the WDSP version, the file is wdspWisdom or wdspWisdom00.
  // sem_trywait() is not elegant, replaced this with wisdom_running variable.
  //
  char *c=getcwd(wisdom_directory, sizeof(wisdom_directory));
  strcpy(&wisdom_directory[strlen(wisdom_directory)],"/");
  fprintf(stderr,"Securing wisdom file in directory: %s\n", wisdom_directory);
  status_text("Checking FFTW Wisdom file ...");
  wisdom_running=1;
  pthread_create(&wisdom_thread_id, NULL, wisdom_thread, wisdom_directory);
  while (wisdom_running) {
      // wait for the wisdom thread to complete, meanwhile
      // handling any GTK events.
      usleep(100000); // 100ms
      while (gtk_events_pending ()) {
        gtk_main_iteration ();
      }
      status_text(wisdom_get_status());
  }

  g_idle_add(ext_discovery,NULL);
  return 0;
}

static void activate_pihpsdr(GtkApplication *app, gpointer data) {


  //gtk_init (&argc, &argv);

  fprintf(stderr,"Build: %s %s\n",build_date,version);
#ifdef __linux__
  {
    char exe[1024];
    ssize_t n;

    n = readlink("/proc/self/exe", exe, (int)sizeof(exe) - 1);
    if (n > 0) {
      exe[n] = '\0';
      fprintf(stderr,
          "Binary: %s\n"
          "(Run this exact path after `make`, or `sudo make install` to update /usr/local/bin.)\n",
          exe);
    }
  }
#endif

  fprintf(stderr,"GTK+ version %ud.%ud.%ud\n", gtk_major_version, gtk_minor_version, gtk_micro_version);
  uname(&unameData);
  fprintf(stderr,"sysname: %s\n",unameData.sysname);
  fprintf(stderr,"nodename: %s\n",unameData.nodename);
  fprintf(stderr,"release: %s\n",unameData.release);
  fprintf(stderr,"version: %s\n",unameData.version);
  fprintf(stderr,"machine: %s\n",unameData.machine);

  load_css();

  GdkScreen *screen=gdk_screen_get_default();
  if(screen==NULL) {
    fprintf(stderr,"no default screen!\n");
    _exit(0);
  }


  display_width=gdk_screen_get_width(screen);
  display_height=gdk_screen_get_height(screen);

fprintf(stderr,"width=%d height=%d\n", display_width, display_height);

  // Go to "window" mode if there is enough space on the screen.
  // Do not forget extra space needed for window top bars, screen bars etc.

  if(display_width>(MAX_DISPLAY_WIDTH+10) && display_height>(MAX_DISPLAY_HEIGHT+30)) {
    display_width=MAX_DISPLAY_WIDTH;
    display_height=MAX_DISPLAY_HEIGHT;
    full_screen=0;
  } else {
    //
    // Some RaspPi variants report slightly too large screen sizes
    // on a 7-inch screen, e.g. 848*480 while the physical resolution is 800*480
    // Therefore, as a work-around, limit window size to 800*480
    //
    if (display_width > MAX_DISPLAY_WIDTH) {
      display_width = MAX_DISPLAY_WIDTH;
    }
    if (display_height > MAX_DISPLAY_HEIGHT) {
      display_height = MAX_DISPLAY_HEIGHT;
    }
    full_screen=1;
  }

fprintf(stderr,"display_width=%d display_height=%d\n", display_width, display_height);

  fprintf(stderr,"create top level window\n");
  top_window = gtk_application_window_new (app);
  gtk_widget_set_name(top_window, "pihpsdr_main");
  if(full_screen) {
    fprintf(stderr,"full screen\n");
    gtk_window_fullscreen(GTK_WINDOW(top_window));
  }
  gtk_widget_set_size_request(top_window, display_width, display_height);
  gtk_window_set_title (GTK_WINDOW (top_window), "piHPSDR");
  gtk_window_set_position(GTK_WINDOW(top_window),GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(top_window), FALSE);
  g_signal_connect (top_window, "delete-event", G_CALLBACK (main_delete), NULL);
  //g_signal_connect (top_window,"draw", G_CALLBACK (main_draw_cb), NULL);

  //
  // We want to use the space-bar as an alternative to go to TX
  //
  gtk_widget_add_events(top_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(top_window, "key_press_event", G_CALLBACK(keypress_cb), NULL);


fprintf(stderr,"create splash layout\n");
  grid = gtk_grid_new();
  gtk_widget_set_size_request(grid, display_width, display_height);
  gtk_widget_set_name(grid, "pihpsdr_splash_root");
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

  GtkWidget *splash_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_set_name(splash_col, "pihpsdr_splash_column");
  gtk_widget_set_hexpand(splash_col, TRUE);
  gtk_widget_set_vexpand(splash_col, TRUE);
  gtk_widget_set_halign(splash_col, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(splash_col, GTK_ALIGN_CENTER);

  char build[128];
  sprintf(build,"build: %s %s",build_date, version);

  GtkWidget *pi_label=gtk_label_new("piHPSDR");
  gtk_widget_set_name(pi_label, "pihpsdr_splash_title");
  gtk_label_set_justify(GTK_LABEL(pi_label),GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(pi_label), 0.5);
  gtk_widget_set_halign(pi_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(splash_col), pi_label, FALSE, FALSE, 0);

  GtkWidget *author_label=gtk_label_new("by John Melton g0orx/n6lyt");
  gtk_widget_set_name(author_label, "pihpsdr_splash_subtitle");
  gtk_label_set_justify(GTK_LABEL(author_label),GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(author_label), 0.5);
  gtk_widget_set_halign(author_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(splash_col), author_label, FALSE, FALSE, 0);

  GtkWidget *build_date_label=gtk_label_new(build);
  gtk_widget_set_name(build_date_label, "pihpsdr_splash_subtitle");
  gtk_label_set_justify(GTK_LABEL(build_date_label),GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(build_date_label), 0.5);
  gtk_widget_set_halign(build_date_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(splash_col), build_date_label, FALSE, FALSE, 0);

  status=gtk_label_new("");
  gtk_widget_set_name(status, "pihpsdr_splash_status");
  gtk_label_set_justify(GTK_LABEL(status),GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(status), 0.5);
  gtk_widget_set_halign(status, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(splash_col), status, FALSE, FALSE, 0);

  GtkWidget *polish_label=gtk_label_new("UI Polish by NE0R");
  gtk_widget_set_name(polish_label, "pihpsdr_splash_subtitle");
  gtk_label_set_justify(GTK_LABEL(polish_label),GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(polish_label), 0.5);
  gtk_widget_set_halign(polish_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(splash_col), polish_label, FALSE, FALSE, 0);

  gtk_grid_attach(GTK_GRID(grid), splash_col, 0, 0, 1, 1);
fprintf(stderr,"add grid\n");
  gtk_container_add (GTK_CONTAINER (top_window), grid);

  {
    gchar *icon_path = pihpsdr_resolve_data_file("hpsdr.png");
    if (icon_path != NULL) {
      GError *err = NULL;
      if (!gtk_window_set_icon_from_file(GTK_WINDOW(top_window), icon_path, &err)) {
        fprintf(stderr,"Warning: failed to set icon for top_window\n");
        if (err != NULL) {
          fprintf(stderr,"%s\n", err->message);
          g_error_free(err);
        }
        gtk_window_set_icon_name(GTK_WINDOW(top_window), "audio-card");
      }
      g_free(icon_path);
    } else {
      gtk_window_set_icon_name(GTK_WINDOW(top_window), "audio-card");
    }
  }

  gtk_widget_show_all(top_window);

fprintf(stderr,"g_idle_add: init\n");
  g_idle_add(init,NULL);

}

int main(int argc,char **argv) {
  GtkApplication *pihpsdr;
  int status;

  char name[1024];

  // Install crash/exit signal handlers as early as possible so that even
  // a SIGSEGV during startup tells the radio to stop streaming. Without
  // this, a crash leaves the radio firmware spamming UDP packets to a
  // dead endpoint and the next piHPSDR start hangs in discovery until
  // the host is rebooted.
  install_pihpsdr_signal_handlers();

#ifdef __APPLE__
  void MacOSstartup(char *path);
  MacOSstartup(argv[0]);
#endif

  sprintf(name,"org.g0orx.pihpsdr.pid%d",getpid());

//fprintf(stderr,"gtk_application_new: %s\n",name);

  pihpsdr=gtk_application_new(name, G_APPLICATION_FLAGS_NONE);
  g_signal_connect(pihpsdr, "activate", G_CALLBACK(activate_pihpsdr), NULL);
  status=g_application_run(G_APPLICATION(pihpsdr), argc, argv);
  fprintf(stderr,"exiting ...\n");
  g_object_unref(pihpsdr);
  return status;
}

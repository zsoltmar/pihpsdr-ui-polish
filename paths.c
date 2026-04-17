#include "paths.h"

#include <glib.h>

#ifdef __linux__
#include <linux/limits.h>
#include <unistd.h>
#endif

gchar *
pihpsdr_resolve_data_file(const gchar *basename)
{
  gchar *p;

  if (basename == NULL) {
    return NULL;
  }

  if (g_path_is_absolute(basename) && g_file_test(basename, G_FILE_TEST_IS_REGULAR)) {
    return g_strdup(basename);
  }

  if (g_file_test(basename, G_FILE_TEST_IS_REGULAR)) {
    return g_strdup(basename);
  }

  {
    gchar *cwd = g_get_current_dir();
    p = g_build_filename(cwd, basename, NULL);
    g_free(cwd);
    if (g_file_test(p, G_FILE_TEST_IS_REGULAR)) {
      return p;
    }
    g_free(p);
  }

#ifdef __linux__
  {
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
      gchar *dir;
      exe[n] = 0;
      dir = g_path_get_dirname(exe);
      p = g_build_filename(dir, basename, NULL);
      g_free(dir);
      if (g_file_test(p, G_FILE_TEST_IS_REGULAR)) {
        return p;
      }
      g_free(p);
    }
  }
#endif

  p = g_build_filename("/usr/local/share/pihpsdr", basename, NULL);
  if (g_file_test(p, G_FILE_TEST_IS_REGULAR)) {
    return p;
  }
  g_free(p);

  p = g_build_filename("/usr/share/pihpsdr", basename, NULL);
  if (g_file_test(p, G_FILE_TEST_IS_REGULAR)) {
    return p;
  }
  g_free(p);

  return NULL;
}

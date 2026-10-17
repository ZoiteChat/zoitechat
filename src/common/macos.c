/* Relocate resources before GTK, Fontconfig, GIO or OpenSSL initialize.
 * GPL-2.0-or-later; see COPYING. */
#include "config.h"
#include "macos.h"
#include <gio/gio.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *locale_dir;

static void
set_path (const char *variable, const char *base, const char *relative)
{
    char *path = g_build_filename (base, relative, NULL);
    g_setenv (variable, path, TRUE);
    g_free (path);
}

static void
relocate_cache (const char *resources, const char *name, const char *variable)
{
    char *input = g_build_filename (resources, name, NULL);
    char *contents = NULL;
    GError *error = NULL;

    if (g_file_get_contents (input, &contents, NULL, &error))
    {
        /* GTK cache files require absolute module paths. Write only data to
         * the user cache, never into the signed/read-only application bundle. */
        char **parts = g_strsplit (contents, "@ZOITECHAT_RESOURCES@", -1);
        char *escaped = g_strescape (resources, NULL);
        char *relocated = g_strjoinv (escaped, parts);
        g_free (escaped);
        char *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, relocated, -1);
        char *directory = g_build_filename (g_get_user_cache_dir (), "zoitechat", "modules", digest, NULL);
        char *output = g_build_filename (directory, name, NULL);
        if (g_mkdir_with_parents (directory, 0700) == 0 &&
            g_file_set_contents (output, relocated, -1, &error))
            g_setenv (variable, output, TRUE);
        else
            g_warning ("Cannot relocate %s: %s", name, error ? error->message : "cannot create cache directory");
        g_free (output);
        g_free (directory);
        g_free (digest);
        g_free (relocated);
        g_strfreev (parts);
    }
    else
        g_warning ("Cannot read %s: %s", input, error->message);
    g_clear_error (&error);
    g_free (contents);
    g_free (input);
}

void
zoitechat_macos_init (void)
{
    uint32_t size = 0;
    char *raw, *executable, *macos, *contents, *resources, *marker;
    char *ca;
    GTlsDatabase *database;
    GError *error = NULL;

    _NSGetExecutablePath (NULL, &size);
    raw = g_malloc (size);
    if (_NSGetExecutablePath (raw, &size) != 0)
    {
        g_free (raw);
        return;
    }
    executable = realpath (raw, NULL);
    g_free (raw);
    if (!executable)
        return;
    macos = g_path_get_dirname (executable);
    free (executable);
    contents = g_path_get_dirname (macos);
    resources = g_build_filename (contents, "Resources", NULL);
    marker = g_build_filename (resources, "zoitechat-bundle", NULL);
    g_free (macos);
    g_free (contents);
    if (!g_file_test (marker, G_FILE_TEST_IS_REGULAR))
    {
        g_free (marker);
        g_free (resources);
        return; /* Ordinary Meson installs and build-tree runs retain their paths. */
    }
    g_free (marker);
    g_setenv ("GDK_BACKEND", "quartz", TRUE);
    set_path ("ZOITECHAT_MACOS_SHARE", resources, "share");
    set_path ("XDG_DATA_DIRS", resources, "share");
    set_path ("GSETTINGS_SCHEMA_DIR", resources, "share/glib-2.0/schemas");
    set_path ("FONTCONFIG_FILE", resources, "fonts.conf");
    set_path ("GIO_EXTRA_MODULES", resources, "lib/gio/modules");
    set_path ("GIO_MODULE_DIR", resources, "lib/gio/modules");
    set_path ("ZOITECHAT_LIBDIR", resources, "lib/zoitechat/plugins");
    set_path ("ZOITECHAT_EMOJI_FONT", resources, "share/fonts/zoitechat/NotoColorEmoji.ttf");
    set_path ("ZOITECHAT_ENCHANT_LIBRARY", resources, "lib/libenchant-2.dylib");
    g_setenv ("ZOITECHAT_ENCHANT_PREFIX", resources, TRUE);
    set_path ("SSL_CERT_FILE", resources, "certs/ca-bundle.crt");
    set_path ("OPENSSL_MODULES", resources, "lib/ossl-modules");
    locale_dir = g_build_filename (resources, "share/locale", NULL);
    relocate_cache (resources, "pixbuf-loaders.cache", "GDK_PIXBUF_MODULE_FILE");
    relocate_cache (resources, "gtk-immodules.cache", "GTK_IM_MODULE_FILE");

    /* HTTPS previews use GIO; IRC uses OpenSSL. Give both the same CA set. */
    ca = g_build_filename (resources, "certs/ca-bundle.crt", NULL);
    database = g_tls_file_database_new (ca, &error);
    if (database)
    {
        g_tls_backend_set_default_database (g_tls_backend_get_default (), database);
        g_object_unref (database);
    }
    else
        g_warning ("Cannot load bundled HTTPS trust database: %s", error->message);
    g_clear_error (&error);
    g_free (ca);
    g_free (resources);
}

const char *
zoitechat_macos_locale_dir (void)
{
    return locale_dir ? locale_dir : LOCALEDIR;
}

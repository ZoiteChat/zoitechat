/*
 * Modern Linux SysInfo backend for ZoiteChat.
 *
 * Uses native kernel/sysfs interfaces and caches static hardware data.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(USE_GTK_FRONTEND)
#include <gdk/gdk.h>
#endif

#ifdef HAVE_LIBPCI
#include <pci/pci.h>
#endif

#include "../format.h"
#include "../sysinfo-backend.h"

#define SYSINFO_SEP " \xC2\xB7 "

typedef struct
{
	char *os;
	char *cpu;
	char *gpu;
	char *chipset;
	char *sound;
	GHashTable *pci_names;
} LinuxStaticSnapshot;

static LinuxStaticSnapshot snapshot;
static gsize os_initialized = 0;
static gsize cpu_initialized = 0;
static gsize pci_initialized = 0;
static gsize sound_initialized = 0;

static char *read_trimmed_file (const char *path);
static gboolean read_hex_file (const char *path, guint *value);
static char *join_ptr_array (GPtrArray *items, const char *separator);
static void ptr_array_add_unique (GPtrArray *items, const char *value);
static char *read_os_pretty_name (void);
static char *read_cpu_model (void);
static guint count_physical_cores (void);
static char *build_cpu_string (void);
static char *build_sound_string (void);
static void scan_pci_devices (void);
static void ensure_os_snapshot (void);
static void ensure_cpu_snapshot (void);
static void ensure_pci_snapshot (void);
static void ensure_sound_snapshot (void);
static char *format_usage (guint64 total, guint64 available);
static char *read_driver_name (const char *interface_path);

static char *
read_trimmed_file (const char *path)
{
	char *contents = NULL;
	gsize length = 0;

	if (!g_file_get_contents (path, &contents, &length, NULL))
	{
		return NULL;
	}

	g_strstrip (contents);
	if (contents[0] == '\0')
	{
		g_free (contents);
		return NULL;
	}

	return contents;
}

static gboolean
read_hex_file (const char *path, guint *value)
{
	char *contents;
	char *end = NULL;
	guint64 parsed;

	contents = read_trimmed_file (path);
	if (!contents)
	{
		return FALSE;
	}

	errno = 0;
	parsed = g_ascii_strtoull (contents, &end, 16);
	if (errno != 0 || end == contents)
	{
		g_free (contents);
		return FALSE;
	}

	*value = (guint) parsed;
	g_free (contents);
	return TRUE;
}

static char *
join_ptr_array (GPtrArray *items, const char *separator)
{
	GString *joined;
	guint i;

	if (!items || items->len == 0)
	{
		return NULL;
	}

	joined = g_string_new (NULL);
	for (i = 0; i < items->len; i++)
	{
		const char *item = g_ptr_array_index (items, i);

		if (i != 0)
		{
			g_string_append (joined, separator);
		}
		g_string_append (joined, item);
	}

	return g_string_free (joined, FALSE);
}

static void
ptr_array_add_unique (GPtrArray *items, const char *value)
{
	guint i;

	if (!value || value[0] == '\0')
	{
		return;
	}

	for (i = 0; i < items->len; i++)
	{
		const char *existing = g_ptr_array_index (items, i);
		if (g_strcmp0 (existing, value) == 0)
		{
			return;
		}
	}

	g_ptr_array_add (items, g_strdup (value));
}

static char *
unquote_os_release_value (const char *value)
{
	char *copy;
	gsize len;

	if (!value)
	{
		return NULL;
	}

	copy = g_strdup (value);
	g_strstrip (copy);
	len = strlen (copy);

	if (len >= 2 &&
		((copy[0] == '"' && copy[len - 1] == '"') ||
		 (copy[0] == '\'' && copy[len - 1] == '\'')))
	{
		copy[len - 1] = '\0';
		memmove (copy, copy + 1, len - 1);
	}

	return copy;
}

static char *
read_pretty_name_from_os_release (const char *path)
{
	char *contents = NULL;
	char **lines;
	char *result = NULL;
	guint i;

	if (!g_file_get_contents (path, &contents, NULL, NULL))
	{
		return NULL;
	}

	lines = g_strsplit (contents, "\n", -1);
	for (i = 0; lines[i] != NULL; i++)
	{
		if (g_str_has_prefix (lines[i], "PRETTY_NAME="))
		{
			result = unquote_os_release_value (lines[i] + strlen ("PRETTY_NAME="));
			break;
		}
	}

	g_strfreev (lines);
	g_free (contents);
	return result;
}

static char *
read_os_pretty_name (void)
{
	char *pretty = NULL;

	/* Flatpak exposes the host os-release here. Prefer it over the runtime. */
	if (g_file_test ("/run/host/etc/os-release", G_FILE_TEST_IS_REGULAR))
	{
		pretty = read_pretty_name_from_os_release ("/run/host/etc/os-release");
	}

	if (!pretty)
	{
		pretty = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
	}

	if (!pretty)
	{
		pretty = read_pretty_name_from_os_release ("/etc/os-release");
	}

	return pretty ? pretty : g_strdup ("Linux");
}

static char *
read_cpu_model (void)
{
	FILE *fp;
	char line[1024];
	static const char *keys[] = {
		"model name",
		"Hardware",
		"Processor",
		"cpu model",
		"Model Name",
		NULL
	};

	fp = fopen ("/proc/cpuinfo", "r");
	if (fp)
	{
		while (fgets (line, sizeof (line), fp) != NULL)
		{
			char *colon = strchr (line, ':');
			guint i;

			if (!colon)
			{
				continue;
			}

			*colon = '\0';
			g_strstrip (line);
			for (i = 0; keys[i] != NULL; i++)
			{
				if (g_ascii_strcasecmp (line, keys[i]) == 0)
				{
					char *value = g_strdup (colon + 1);
					char *endptr = NULL;
					g_strstrip (value);

					/*
					 * On x86 /proc/cpuinfo starts with `processor : 0`.
					 * `Processor` is also a useful model key on some ARM systems,
					 * so only reject it when the value is purely a CPU index.
					 */
					if (g_ascii_strcasecmp (keys[i], "Processor") == 0 && value[0] != '\0')
					{
						g_ascii_strtoull (value, &endptr, 10);
						if (endptr && endptr != value && *endptr == '\0')
						{
							g_free (value);
							continue;
						}
					}

					if (value[0] != '\0')
					{
						fclose (fp);
						return value;
					}
					g_free (value);
				}
			}
		}
		fclose (fp);
	}

	if (g_file_test ("/sys/firmware/devicetree/base/model", G_FILE_TEST_IS_REGULAR))
	{
		char *model = read_trimmed_file ("/sys/firmware/devicetree/base/model");
		if (model)
		{
			return model;
		}
	}

	return g_strdup ("Unknown CPU");
}

static guint
count_physical_cores (void)
{
	GDir *dir;
	const char *entry;
	GHashTable *cores;
	guint count;

	dir = g_dir_open ("/sys/devices/system/cpu", 0, NULL);
	if (!dir)
	{
		return 0;
	}

	cores = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	while ((entry = g_dir_read_name (dir)) != NULL)
	{
		char *package_path;
		char *core_path;
		char *package_id;
		char *core_id;
		char *key;

		if (!g_str_has_prefix (entry, "cpu") || !g_ascii_isdigit (entry[3]))
		{
			continue;
		}

		package_path = g_build_filename ("/sys/devices/system/cpu", entry, "topology", "physical_package_id", NULL);
		core_path = g_build_filename ("/sys/devices/system/cpu", entry, "topology", "core_id", NULL);
		package_id = read_trimmed_file (package_path);
		core_id = read_trimmed_file (core_path);
		g_free (package_path);
		g_free (core_path);

		if (!package_id || !core_id)
		{
			g_free (package_id);
			g_free (core_id);
			continue;
		}

		key = g_strdup_printf ("%s:%s", package_id, core_id);
		g_hash_table_add (cores, key);
		g_free (package_id);
		g_free (core_id);
	}

	count = g_hash_table_size (cores);
	g_hash_table_destroy (cores);
	g_dir_close (dir);
	return count;
}

static char *
build_cpu_string (void)
{
	char *model;
	long logical_long;
	guint logical;
	guint physical;

	model = read_cpu_model ();
	logical_long = sysconf (_SC_NPROCESSORS_ONLN);
	logical = logical_long > 0 ? (guint) logical_long : 0;
	physical = count_physical_cores ();

	if (physical == 0)
	{
		physical = logical;
	}

	if (physical != 0 && logical != 0)
	{
		char *result = g_strdup_printf ("%s" SYSINFO_SEP "%uC/%uT", model, physical, logical);
		g_free (model);
		return result;
	}

	return model;
}


static char *
build_sound_string (void)
{
	FILE *fp;
	char line[1024];
	GPtrArray *cards;
	char *result;

	fp = fopen ("/proc/asound/cards", "r");
	if (!fp)
	{
		return NULL;
	}

	cards = g_ptr_array_new_with_free_func (g_free);
	while (fgets (line, sizeof (line), fp) != NULL)
	{
		char *start = line;
		char *separator;
		char *name;

		while (*start && g_ascii_isspace (*start))
		{
			start++;
		}
		if (!g_ascii_isdigit (*start))
		{
			continue;
		}

		separator = strstr (start, " - ");
		if (!separator)
		{
			continue;
		}

		name = g_strdup (separator + 3);
		g_strstrip (name);
		ptr_array_add_unique (cards, name);
		g_free (name);
	}
	fclose (fp);

	result = join_ptr_array (cards, ", ");
	g_ptr_array_free (cards, TRUE);
	return result;
}

#ifdef HAVE_LIBPCI
static char *
resolve_pci_name (struct pci_access *pacc, guint vendor, guint device)
{
	char name[1024];
	char *resolved;

	resolved = pci_lookup_name (pacc, name, sizeof (name),
		PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
		vendor, device);
	if (resolved && resolved[0] != '\0')
	{
		return g_strdup (resolved);
	}

	return g_strdup_printf ("%04x:%04x", vendor, device);
}
#endif

static char *
clean_gpu_name (const char *raw_name)
{
	static const char *vendor_prefixes[] = {
		"NVIDIA Corporation ",
		"Advanced Micro Devices, Inc. [AMD/ATI] ",
		"Advanced Micro Devices, Inc. ",
		"Intel Corporation ",
		NULL
	};
	char *name;
	char *open;
	char *close;
	guint i;

	if (!raw_name || raw_name[0] == '\0')
		return NULL;

	name = g_strdup (raw_name);
	g_strstrip (name);

	/* Prefer the friendly product name at the end of pci.ids entries. */
	close = strrchr (name, ']');
	open = strrchr (name, '[');
	if (open && close && close > open + 1 && close[1] == '\0')
	{
		char *clean;

		*close = '\0';
		clean = g_strdup (open + 1);
		g_strstrip (clean);
		if (clean[0] != '\0')
		{
			g_free (name);
			return clean;
		}
		g_free (clean);
	}

	/* Fall back to removing common PCI vendor prefixes. */
	for (i = 0; vendor_prefixes[i] != NULL; i++)
	{
		if (g_str_has_prefix (name, vendor_prefixes[i]))
		{
			char *clean = g_strdup (name + strlen (vendor_prefixes[i]));
			g_strstrip (clean);
			if (clean[0] != '\0')
			{
				g_free (name);
				return clean;
			}
			g_free (clean);
		}
	}

	return name;
}

static void
scan_pci_devices (void)
{
	GDir *dir;
	const char *entry;
	GPtrArray *gpus;
	GPtrArray *chipsets;
#ifdef HAVE_LIBPCI
	struct pci_access *pacc = NULL;
#endif

	snapshot.pci_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	gpus = g_ptr_array_new_with_free_func (g_free);
	chipsets = g_ptr_array_new_with_free_func (g_free);

#ifdef HAVE_LIBPCI
	pacc = pci_alloc ();
	if (pacc)
	{
		pci_init (pacc);
	}
#endif

	dir = g_dir_open ("/sys/bus/pci/devices", 0, NULL);
	if (dir)
	{
		while ((entry = g_dir_read_name (dir)) != NULL)
		{
			char *class_path;
			char *vendor_path;
			char *device_path;
			char *name;
			char *key;
			guint class_code;
			guint vendor;
			guint device;
			guint base_class;
			guint class_subclass;

			class_path = g_build_filename ("/sys/bus/pci/devices", entry, "class", NULL);
			vendor_path = g_build_filename ("/sys/bus/pci/devices", entry, "vendor", NULL);
			device_path = g_build_filename ("/sys/bus/pci/devices", entry, "device", NULL);

			if (!read_hex_file (class_path, &class_code) ||
				!read_hex_file (vendor_path, &vendor) ||
				!read_hex_file (device_path, &device))
			{
				g_free (class_path);
				g_free (vendor_path);
				g_free (device_path);
				continue;
			}

			g_free (class_path);
			g_free (vendor_path);
			g_free (device_path);

#ifdef HAVE_LIBPCI
			if (pacc)
			{
				name = resolve_pci_name (pacc, vendor, device);
			}
			else
#endif
			{
				name = g_strdup_printf ("%04x:%04x", vendor, device);
			}

			key = g_strdup_printf ("%04x:%04x", vendor, device);
			if (!g_hash_table_contains (snapshot.pci_names, key))
			{
				g_hash_table_insert (snapshot.pci_names, key, g_strdup (name));
			}
			else
			{
				g_free (key);
			}

			base_class = (class_code >> 16) & 0xff;
			class_subclass = (class_code >> 8) & 0xffff;

			if (base_class == 0x03)
			{
				char *gpu_name = clean_gpu_name (name);
				if (gpu_name)
				{
					ptr_array_add_unique (gpus, gpu_name);
					g_free (gpu_name);
				}
			}
			else if (class_subclass == 0x0600)
			{
				ptr_array_add_unique (chipsets, name);
			}

			g_free (name);
		}
		g_dir_close (dir);
	}

#ifdef HAVE_LIBPCI
	if (pacc)
	{
		pci_cleanup (pacc);
	}
#endif

	snapshot.gpu = join_ptr_array (gpus, ", ");
	snapshot.chipset = join_ptr_array (chipsets, ", ");
	g_ptr_array_free (gpus, TRUE);
	g_ptr_array_free (chipsets, TRUE);
}

static char *
build_os_string (void)
{
	char *pretty;
	struct utsname uts;
	char *result;

	pretty = read_os_pretty_name ();
	if (uname (&uts) != 0)
	{
		return pretty;
	}

	result = g_strdup_printf ("%s" SYSINFO_SEP "%s %s" SYSINFO_SEP "%s",
		pretty, uts.sysname, uts.release, uts.machine);
	g_free (pretty);
	return result;
}

static void
ensure_os_snapshot (void)
{
	if (g_once_init_enter (&os_initialized))
	{
		snapshot.os = build_os_string ();
		g_once_init_leave (&os_initialized, 1);
	}
}

static void
ensure_cpu_snapshot (void)
{
	if (g_once_init_enter (&cpu_initialized))
	{
		snapshot.cpu = build_cpu_string ();
		g_once_init_leave (&cpu_initialized, 1);
	}
}

static void
ensure_pci_snapshot (void)
{
	if (g_once_init_enter (&pci_initialized))
	{
		scan_pci_devices ();
		g_once_init_leave (&pci_initialized, 1);
	}
}

static void
ensure_sound_snapshot (void)
{
	if (g_once_init_enter (&sound_initialized))
	{
		snapshot.sound = build_sound_string ();
		g_once_init_leave (&sound_initialized, 1);
	}
}

static char *
format_usage (guint64 total, guint64 available)
{
	guint64 used;
	char *used_fmt;
	char *total_fmt;
	char *result;
	double percent;

	if (total == 0)
	{
		return NULL;
	}

	if (available > total)
	{
		available = total;
	}
	used = total - available;
	percent = ((double) used / (double) total) * 100.0;

	used_fmt = g_format_size_full (used, G_FORMAT_SIZE_IEC_UNITS);
	total_fmt = g_format_size_full (total, G_FORMAT_SIZE_IEC_UNITS);
	result = g_strdup_printf ("%s / %s used (%.0f%%)", used_fmt, total_fmt, percent);
	g_free (used_fmt);
	g_free (total_fmt);
	return result;
}

char *
sysinfo_backend_get_os (void)
{
	ensure_os_snapshot ();
	return g_strdup (snapshot.os);
}

char *
sysinfo_backend_get_cpu (void)
{
	ensure_cpu_snapshot ();
	return g_strdup (snapshot.cpu);
}

char *
sysinfo_backend_get_gpu (void)
{
	ensure_pci_snapshot ();
	return g_strdup (snapshot.gpu);
}

char *
sysinfo_backend_get_chipset (void)
{
	ensure_pci_snapshot ();
	return g_strdup (snapshot.chipset);
}

char *
sysinfo_backend_get_sound (void)
{
	ensure_sound_snapshot ();
	return g_strdup (snapshot.sound);
}

char *
sysinfo_backend_get_memory (void)
{
	FILE *fp;
	char line[512];
	guint64 mem_total_kib = 0;
	guint64 mem_available_kib = 0;
	guint64 swap_total_kib = 0;
	guint64 swap_free_kib = 0;
	char *memory;

	fp = fopen ("/proc/meminfo", "r");
	if (!fp)
	{
		return NULL;
	}

	while (fgets (line, sizeof (line), fp) != NULL)
	{
		if (g_str_has_prefix (line, "MemTotal:"))
		{
			mem_total_kib = g_ascii_strtoull (line + strlen ("MemTotal:"), NULL, 10);
		}
		else if (g_str_has_prefix (line, "MemAvailable:"))
		{
			mem_available_kib = g_ascii_strtoull (line + strlen ("MemAvailable:"), NULL, 10);
		}
		else if (g_str_has_prefix (line, "SwapTotal:"))
		{
			swap_total_kib = g_ascii_strtoull (line + strlen ("SwapTotal:"), NULL, 10);
		}
		else if (g_str_has_prefix (line, "SwapFree:"))
		{
			swap_free_kib = g_ascii_strtoull (line + strlen ("SwapFree:"), NULL, 10);
		}
	}
	fclose (fp);

	if (mem_total_kib == 0)
	{
		return NULL;
	}

	memory = format_usage (mem_total_kib * 1024, mem_available_kib * 1024);
	if (memory && swap_total_kib != 0)
	{
		char *swap = format_usage (swap_total_kib * 1024, swap_free_kib * 1024);
		if (swap)
		{
			char *result = g_strdup_printf ("%s" SYSINFO_SEP "Swap: %s", memory, swap);
			g_free (memory);
			g_free (swap);
			return result;
		}
	}

	return memory;
}

char *
sysinfo_backend_get_disk (void)
{
	const char *path;
	struct statvfs fs;
	guint64 block_size;
	guint64 total;
	guint64 available;
	char *usage;

	path = g_get_home_dir ();
	if (!path || path[0] == '\0')
	{
		path = "/";
	}

	if (statvfs (path, &fs) != 0)
	{
		return NULL;
	}

	block_size = fs.f_frsize != 0 ? (guint64) fs.f_frsize : (guint64) fs.f_bsize;
	total = (guint64) fs.f_blocks * block_size;
	available = (guint64) fs.f_bavail * block_size;
	usage = format_usage (total, available);
	return usage;
}

char *
sysinfo_backend_get_uptime (void)
{
	char *contents;
	char *end = NULL;
	double uptime;

	contents = read_trimmed_file ("/proc/uptime");
	if (!contents)
	{
		return NULL;
	}

	uptime = g_ascii_strtod (contents, &end);
	if (end == contents || uptime <= 0.0)
	{
		g_free (contents);
		return NULL;
	}

	g_free (contents);
	return sysinfo_format_uptime ((gint64) uptime);
}

static char *
read_driver_name (const char *interface_path)
{
	char *driver_path;
	char *target;
	char *name;

	driver_path = g_build_filename (interface_path, "device", "driver", NULL);
	target = g_file_read_link (driver_path, NULL);
	g_free (driver_path);
	if (!target)
	{
		return NULL;
	}

	name = g_path_get_basename (target);
	g_free (target);
	return name;
}

char *
sysinfo_backend_get_network (void)
{
	GDir *dir;
	const char *entry;
	GPtrArray *adapters;
	char *result;

	ensure_pci_snapshot ();
	dir = g_dir_open ("/sys/class/net", 0, NULL);
	if (!dir)
	{
		return NULL;
	}

	adapters = g_ptr_array_new_with_free_func (g_free);
	while ((entry = g_dir_read_name (dir)) != NULL)
	{
		char *interface_path;
		char *device_path;
		char *operstate_path;
		char *operstate;
		char *wireless_path;
		char *vendor_path;
		char *device_id_path;
		char *driver;
		const char *hardware_name = NULL;
		char *key = NULL;
		char *label;
		guint vendor;
		guint device;
		gboolean wireless;

		if (strcmp (entry, "lo") == 0)
		{
			continue;
		}

		interface_path = g_build_filename ("/sys/class/net", entry, NULL);
		device_path = g_build_filename (interface_path, "device", NULL);
		if (!g_file_test (device_path, G_FILE_TEST_EXISTS))
		{
			g_free (device_path);
			g_free (interface_path);
			continue;
		}

		operstate_path = g_build_filename (interface_path, "operstate", NULL);
		operstate = read_trimmed_file (operstate_path);
		g_free (operstate_path);
		if (!operstate || g_ascii_strcasecmp (operstate, "up") != 0)
		{
			g_free (operstate);
			g_free (device_path);
			g_free (interface_path);
			continue;
		}
		g_free (operstate);

		wireless_path = g_build_filename (interface_path, "wireless", NULL);
		wireless = g_file_test (wireless_path, G_FILE_TEST_IS_DIR);
		g_free (wireless_path);

		vendor_path = g_build_filename (device_path, "vendor", NULL);
		device_id_path = g_build_filename (device_path, "device", NULL);
		if (read_hex_file (vendor_path, &vendor) && read_hex_file (device_id_path, &device))
		{
			key = g_strdup_printf ("%04x:%04x", vendor, device);
			hardware_name = g_hash_table_lookup (snapshot.pci_names, key);
		}
		g_free (vendor_path);
		g_free (device_id_path);

		driver = read_driver_name (interface_path);
		if (hardware_name)
		{
			label = g_strdup_printf ("%s: %s (%s)", wireless ? "Wi-Fi" : "Ethernet", hardware_name, entry);
		}
		else if (driver)
		{
			label = g_strdup_printf ("%s: %s (%s)", wireless ? "Wi-Fi" : "Ethernet", driver, entry);
		}
		else
		{
			label = g_strdup_printf ("%s: %s", wireless ? "Wi-Fi" : "Ethernet", entry);
		}

		ptr_array_add_unique (adapters, label);
		g_free (label);
		g_free (driver);
		g_free (key);
		g_free (device_path);
		g_free (interface_path);
	}
	g_dir_close (dir);

	result = join_ptr_array (adapters, ", ");
	g_ptr_array_free (adapters, TRUE);
	return result;
}

static const char *
sysinfo_detect_toolkit (void)
{
#if defined(USE_GTK_FRONTEND)
	return "GTK3";
#else
	return NULL;
#endif
}

static const char *
sysinfo_detect_display_backend (void)
{
	const char *backend = NULL;
	const char *gdk_backend = g_getenv ("GDK_BACKEND");
	const char *session = g_getenv ("XDG_SESSION_TYPE");
	const gboolean session_wayland = session && g_ascii_strcasecmp (session, "wayland") == 0;

#if defined(USE_GTK_FRONTEND)
	{
		GdkDisplay *display = gdk_display_get_default ();
		if (display)
		{
			const char *type_name = G_OBJECT_TYPE_NAME (display);
			if (type_name)
			{
				if (g_strrstr (type_name, "Wayland"))
				{
					backend = "Wayland";
				}
				else if (g_strrstr (type_name, "X11"))
				{
					backend = "X11";
				}
			}
		}
	}
#endif

	if (!backend && gdk_backend)
	{
		if (g_strrstr (gdk_backend, "wayland"))
		{
			backend = "Wayland";
		}
		else if (g_strrstr (gdk_backend, "x11"))
		{
			backend = "X11";
		}
	}

	if (!backend)
	{
		const gboolean has_wayland = g_getenv ("WAYLAND_DISPLAY") != NULL;
		const gboolean has_x11 = g_getenv ("DISPLAY") != NULL;

		if (has_wayland && !has_x11)
		{
			backend = "Wayland";
		}
		else if (has_x11 && !has_wayland)
		{
			backend = "X11";
		}
		else if (session_wayland)
		{
			backend = "Wayland";
		}
	}

	if (backend && g_strcmp0 (backend, "X11") == 0 && session_wayland)
	{
		return "XWayland";
	}

	return backend;
}

char *
sysinfo_backend_get_ui (void)
{
	const char *toolkit = sysinfo_detect_toolkit ();
	const char *display = sysinfo_detect_display_backend ();

	if (toolkit && display)
	{
		return g_strdup_printf ("%s / %s", toolkit, display);
	}
	if (toolkit)
	{
		return g_strdup (toolkit);
	}
	if (display)
	{
		return g_strdup (display);
	}

	return NULL;
}

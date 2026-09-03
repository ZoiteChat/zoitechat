/*
 * Modern Windows SysInfo backend for ZoiteChat.
 *
 * Avoids WMI in the plugin hot path and uses native Win32/DXGI APIs.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#elif _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <windows.h>
#include <winternl.h>
#include <dxgi.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#include <mmsystem.h>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "winmm.lib")
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../format.h"
#include "../sysinfo-backend.h"

#define SYSINFO_SEP " \xC2\xB7 "

typedef struct
{
	char *os;
	char *cpu;
	char *gpu;
	char *sound;
} WinStaticSnapshot;

static WinStaticSnapshot snapshot;
static gsize os_initialized = 0;
static gsize cpu_initialized = 0;
static gsize gpu_initialized = 0;
static gsize sound_initialized = 0;

static char *wide_to_utf8 (const WCHAR *value);
static char *read_registry_string (HKEY root, const WCHAR *subkey, const WCHAR *value_name);
static gboolean read_registry_dword (HKEY root, const WCHAR *subkey, const WCHAR *value_name, DWORD *value);
static const char *get_native_arch (void);
static DWORD get_windows_build_number (void);
static char *build_os_string (void);
static DWORD count_physical_cores (void);
static char *build_cpu_string (void);
static char *build_gpu_string (void);
static char *build_sound_string (void);
static void ensure_os_snapshot (void);
static void ensure_cpu_snapshot (void);
static void ensure_gpu_snapshot (void);
static void ensure_sound_snapshot (void);
static char *format_usage (guint64 total, guint64 available);
static char *join_ptr_array (GPtrArray *items, const char *separator);
static void ptr_array_add_unique (GPtrArray *items, const char *value);
static gboolean adapter_looks_virtual (const char *name);

static char *
wide_to_utf8 (const WCHAR *value)
{
	if (!value || value[0] == L'\0')
	{
		return NULL;
	}

	return g_utf16_to_utf8 ((const gunichar2 *) value, -1, NULL, NULL, NULL);
}

static char *
read_registry_string (HKEY root, const WCHAR *subkey, const WCHAR *value_name)
{
	HKEY key;
	DWORD type = 0;
	DWORD size = 0;
	WCHAR *buffer;
	char *result = NULL;
	LONG status;

	status = RegOpenKeyExW (root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key);
	if (status != ERROR_SUCCESS)
	{
		return NULL;
	}

	status = RegQueryValueExW (key, value_name, NULL, &type, NULL, &size);
	if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || size < sizeof (WCHAR))
	{
		RegCloseKey (key);
		return NULL;
	}

	buffer = g_malloc0 ((gsize) size + sizeof (WCHAR));
	status = RegQueryValueExW (key, value_name, NULL, &type, (LPBYTE) buffer, &size);
	if (status == ERROR_SUCCESS)
	{
		result = wide_to_utf8 (buffer);
		if (result)
		{
			g_strstrip (result);
		}
	}

	g_free (buffer);
	RegCloseKey (key);
	return result;
}

static gboolean
read_registry_dword (HKEY root, const WCHAR *subkey, const WCHAR *value_name, DWORD *value)
{
	HKEY key;
	DWORD type = 0;
	DWORD size = sizeof (*value);
	LONG status;

	status = RegOpenKeyExW (root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key);
	if (status != ERROR_SUCCESS)
	{
		return FALSE;
	}

	status = RegQueryValueExW (key, value_name, NULL, &type, (LPBYTE) value, &size);
	RegCloseKey (key);
	return status == ERROR_SUCCESS && type == REG_DWORD;
}

static const char *
get_native_arch (void)
{
	SYSTEM_INFO info;

	GetNativeSystemInfo (&info);
	switch (info.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		return "x64";
	case PROCESSOR_ARCHITECTURE_ARM64:
		return "ARM64";
	case PROCESSOR_ARCHITECTURE_INTEL:
		return "x86";
	default:
		return "unknown-arch";
	}
}

static DWORD
get_windows_build_number (void)
{
	typedef LONG (WINAPI *RtlGetVersionFn) (PRTL_OSVERSIONINFOW);
	HMODULE ntdll;
	RtlGetVersionFn rtl_get_version;
	RTL_OSVERSIONINFOW version;
	DWORD build = 0;

	ntdll = GetModuleHandleW (L"ntdll.dll");
	if (ntdll)
	{
		rtl_get_version = (RtlGetVersionFn) GetProcAddress (ntdll, "RtlGetVersion");
		if (rtl_get_version)
		{
			ZeroMemory (&version, sizeof (version));
			version.dwOSVersionInfoSize = sizeof (version);
			if (rtl_get_version (&version) == 0)
			{
				build = version.dwBuildNumber;
			}
		}
	}

	return build;
}

static char *
build_os_string (void)
{
	static const WCHAR current_version_key[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
	char *product;
	char *display_version;
	char *registry_build;
	char *fixed_product = NULL;
	char *result;
	DWORD ubr = 0;
	DWORD build;
	guint64 parsed_build = 0;

	product = read_registry_string (HKEY_LOCAL_MACHINE, current_version_key, L"ProductName");
	display_version = read_registry_string (HKEY_LOCAL_MACHINE, current_version_key, L"DisplayVersion");
	registry_build = read_registry_string (HKEY_LOCAL_MACHINE, current_version_key, L"CurrentBuildNumber");
	read_registry_dword (HKEY_LOCAL_MACHINE, current_version_key, L"UBR", &ubr);

	build = get_windows_build_number ();
	if (registry_build)
	{
		parsed_build = g_ascii_strtoull (registry_build, NULL, 10);
		if (parsed_build != 0)
		{
			build = (DWORD) parsed_build;
		}
	}

	if (!product)
	{
		product = g_strdup (build >= 22000 ? "Windows 11" : "Windows");
	}
	else if (build >= 22000 && g_str_has_prefix (product, "Windows 10"))
	{
		fixed_product = g_strdup_printf ("Windows 11%s", product + strlen ("Windows 10"));
		g_free (product);
		product = fixed_product;
	}

	if (display_version && build != 0)
	{
		result = g_strdup_printf ("%s %s" SYSINFO_SEP "build %lu.%lu" SYSINFO_SEP "%s",
			product, display_version, (unsigned long) build, (unsigned long) ubr, get_native_arch ());
	}
	else if (build != 0)
	{
		result = g_strdup_printf ("%s" SYSINFO_SEP "build %lu.%lu" SYSINFO_SEP "%s",
			product, (unsigned long) build, (unsigned long) ubr, get_native_arch ());
	}
	else
	{
		result = g_strdup_printf ("%s" SYSINFO_SEP "%s", product, get_native_arch ());
	}

	g_free (product);
	g_free (display_version);
	g_free (registry_build);
	return result;
}

static DWORD
count_physical_cores (void)
{
	DWORD length = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer;
	BYTE *cursor;
	BYTE *end;
	DWORD cores = 0;

	GetLogicalProcessorInformationEx (RelationProcessorCore, NULL, &length);
	if (GetLastError () != ERROR_INSUFFICIENT_BUFFER || length == 0)
	{
		return 0;
	}

	buffer = g_malloc0 (length);
	if (!GetLogicalProcessorInformationEx (RelationProcessorCore, buffer, &length))
	{
		g_free (buffer);
		return 0;
	}

	cursor = (BYTE *) buffer;
	end = cursor + length;
	while (cursor < end)
	{
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) cursor;

		if (info->Size == 0)
		{
			break;
		}
		if (info->Relationship == RelationProcessorCore)
		{
			cores++;
		}
		cursor += info->Size;
	}

	g_free (buffer);
	return cores;
}

static char *
build_cpu_string (void)
{
	static const WCHAR cpu_key[] = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
	char *name;
	DWORD cores;
	DWORD threads;
	SYSTEM_INFO info;
	char *result;

	name = read_registry_string (HKEY_LOCAL_MACHINE, cpu_key, L"ProcessorNameString");
	if (!name)
	{
		name = g_strdup ("Unknown CPU");
	}

	cores = count_physical_cores ();
	threads = GetActiveProcessorCount (ALL_PROCESSOR_GROUPS);
	if (threads == 0)
	{
		GetNativeSystemInfo (&info);
		threads = info.dwNumberOfProcessors;
	}
	if (cores == 0)
	{
		cores = threads;
	}

	result = g_strdup_printf ("%s" SYSINFO_SEP "%luC/%luT",
		name, (unsigned long) cores, (unsigned long) threads);
	g_free (name);
	return result;
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
		if (i != 0)
		{
			g_string_append (joined, separator);
		}
		g_string_append (joined, g_ptr_array_index (items, i));
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
		if (g_ascii_strcasecmp (g_ptr_array_index (items, i), value) == 0)
		{
			return;
		}
	}
	g_ptr_array_add (items, g_strdup (value));
}

static char *
build_gpu_string (void)
{
	IDXGIFactory1 *factory = NULL;
	GPtrArray *adapters;
	UINT index;
	HRESULT hr;
	char *result;

	hr = CreateDXGIFactory1 (&IID_IDXGIFactory1, (void **) &factory);
	if (FAILED (hr) || !factory)
	{
		return NULL;
	}

	adapters = g_ptr_array_new_with_free_func (g_free);
	for (index = 0; ; index++)
	{
		IDXGIAdapter1 *adapter = NULL;
		DXGI_ADAPTER_DESC1 desc;
		char *name;
		char *label;

		hr = factory->lpVtbl->EnumAdapters1 (factory, index, &adapter);
		if (hr == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}
		if (FAILED (hr) || !adapter)
		{
			continue;
		}

		ZeroMemory (&desc, sizeof (desc));
		hr = adapter->lpVtbl->GetDesc1 (adapter, &desc);
		adapter->lpVtbl->Release (adapter);
		if (FAILED (hr) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			continue;
		}

		name = wide_to_utf8 (desc.Description);
		if (!name)
		{
			continue;
		}
		g_strstrip (name);

		if ((guint64) desc.DedicatedVideoMemory >= (512ULL * 1024ULL * 1024ULL))
		{
			char *vram = g_format_size_full ((guint64) desc.DedicatedVideoMemory, G_FORMAT_SIZE_IEC_UNITS);
			label = g_strdup_printf ("%s (%s VRAM)", name, vram);
			g_free (vram);
		}
		else
		{
			label = g_strdup (name);
		}

		ptr_array_add_unique (adapters, label);
		g_free (label);
		g_free (name);
	}

	factory->lpVtbl->Release (factory);
	result = join_ptr_array (adapters, ", ");
	g_ptr_array_free (adapters, TRUE);
	return result;
}

static char *
build_sound_string (void)
{
	UINT count;
	UINT i;
	GPtrArray *devices;
	char *result;

	count = waveOutGetNumDevs ();
	if (count == 0)
	{
		return NULL;
	}

	devices = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; i < count; i++)
	{
		WAVEOUTCAPSW caps;
		char *name;

		ZeroMemory (&caps, sizeof (caps));
		if (waveOutGetDevCapsW ((UINT_PTR) i, &caps, sizeof (caps)) != MMSYSERR_NOERROR)
		{
			continue;
		}

		name = wide_to_utf8 (caps.szPname);
		if (name)
		{
			g_strstrip (name);
			ptr_array_add_unique (devices, name);
			g_free (name);
		}
	}

	result = join_ptr_array (devices, ", ");
	g_ptr_array_free (devices, TRUE);
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
ensure_gpu_snapshot (void)
{
	if (g_once_init_enter (&gpu_initialized))
	{
		snapshot.gpu = build_gpu_string ();
		g_once_init_leave (&gpu_initialized, 1);
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
	ensure_gpu_snapshot ();
	return g_strdup (snapshot.gpu);
}

char *
sysinfo_backend_get_sound (void)
{
	ensure_sound_snapshot ();
	return g_strdup (snapshot.sound);
}

char *
sysinfo_backend_get_chipset (void)
{
	return NULL;
}

char *
sysinfo_backend_get_memory (void)
{
	MEMORYSTATUSEX memory;

	ZeroMemory (&memory, sizeof (memory));
	memory.dwLength = sizeof (memory);
	if (!GlobalMemoryStatusEx (&memory))
	{
		return NULL;
	}

	return format_usage ((guint64) memory.ullTotalPhys, (guint64) memory.ullAvailPhys);
}

char *
sysinfo_backend_get_disk (void)
{
	WCHAR windows_dir[MAX_PATH];
	WCHAR volume_path[MAX_PATH];
	ULARGE_INTEGER available;
	ULARGE_INTEGER total;
	ULARGE_INTEGER total_free;
	char *usage;
	char *volume_utf8;
	char *result;

	if (GetWindowsDirectoryW (windows_dir, G_N_ELEMENTS (windows_dir)) == 0)
	{
		return NULL;
	}
	if (!GetVolumePathNameW (windows_dir, volume_path, G_N_ELEMENTS (volume_path)))
	{
		return NULL;
	}
	if (!GetDiskFreeSpaceExW (volume_path, &available, &total, &total_free))
	{
		return NULL;
	}

	usage = format_usage ((guint64) total.QuadPart, (guint64) available.QuadPart);
	if (!usage)
	{
		return NULL;
	}

	volume_utf8 = wide_to_utf8 (volume_path);
	if (!volume_utf8)
	{
		return usage;
	}
	g_strchomp (volume_utf8);
	result = g_strdup_printf ("%s" SYSINFO_SEP "%s", usage, volume_utf8);
	g_free (volume_utf8);
	g_free (usage);
	return result;
}

char *
sysinfo_backend_get_uptime (void)
{
	return sysinfo_format_uptime ((gint64) (GetTickCount64 () / 1000ULL));
}

static gboolean
adapter_looks_virtual (const char *name)
{
	char *lower;
	gboolean result;

	if (!name)
	{
		return FALSE;
	}

	lower = g_ascii_strdown (name, -1);
	result = strstr (lower, "virtual") != NULL ||
		strstr (lower, "hyper-v") != NULL ||
		strstr (lower, "vmware") != NULL ||
		strstr (lower, "virtualbox") != NULL ||
		strstr (lower, "tap-windows") != NULL ||
		strstr (lower, "wireguard") != NULL ||
		strstr (lower, "vpn") != NULL;
	g_free (lower);
	return result;
}

char *
sysinfo_backend_get_network (void)
{
	ULONG size = 16 * 1024;
	ULONG status;
	IP_ADAPTER_ADDRESSES *addresses;
	IP_ADAPTER_ADDRESSES *adapter;
	GPtrArray *physical;
	GPtrArray *fallback;
	GPtrArray *selected;
	char *result;

	addresses = g_malloc0 (size);
	status = GetAdaptersAddresses (AF_UNSPEC,
		GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
		NULL, addresses, &size);
	if (status == ERROR_BUFFER_OVERFLOW)
	{
		g_free (addresses);
		addresses = g_malloc0 (size);
		status = GetAdaptersAddresses (AF_UNSPEC,
			GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
			NULL, addresses, &size);
	}
	if (status != NO_ERROR)
	{
		g_free (addresses);
		return NULL;
	}

	physical = g_ptr_array_new_with_free_func (g_free);
	fallback = g_ptr_array_new_with_free_func (g_free);
	for (adapter = addresses; adapter != NULL; adapter = adapter->Next)
	{
		char *description;
		char *friendly;
		const char *name;
		const char *type;
		char *label;

		if (adapter->OperStatus != IfOperStatusUp || adapter->PhysicalAddressLength == 0)
		{
			continue;
		}
		if (adapter->IfType != IF_TYPE_ETHERNET_CSMACD && adapter->IfType != IF_TYPE_IEEE80211)
		{
			continue;
		}

		description = wide_to_utf8 (adapter->Description);
		friendly = wide_to_utf8 (adapter->FriendlyName);
		name = description && description[0] ? description : friendly;
		if (!name)
		{
			g_free (description);
			g_free (friendly);
			continue;
		}

		type = adapter->IfType == IF_TYPE_IEEE80211 ? "Wi-Fi" : "Ethernet";
		label = g_strdup_printf ("%s: %s", type, name);
		ptr_array_add_unique (fallback, label);
		if (!adapter_looks_virtual (name))
		{
			ptr_array_add_unique (physical, label);
		}

		g_free (label);
		g_free (description);
		g_free (friendly);
	}
	g_free (addresses);

	selected = physical->len != 0 ? physical : fallback;
	result = join_ptr_array (selected, ", ");
	g_ptr_array_free (physical, TRUE);
	g_ptr_array_free (fallback, TRUE);
	return result;
}

char *
sysinfo_backend_get_ui (void)
{
#if defined(USE_GTK_FRONTEND)
	return g_strdup ("Windows / GTK3");
#else
	return g_strdup ("Windows");
#endif
}

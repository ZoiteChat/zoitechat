/* ZoiteChat
 * Copyright (c) 2011-2012 Berke Viktor.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <wbemidl.h>

#include "../sysinfo.h"

/* Cache */
static int cpu_arch = 0;
static int build_arch = 0;
static char *os_name = NULL;
static char *cpu_info = NULL;
static char *vga_name = NULL;

typedef enum
{
	QUERY_WMI_OS,
	QUERY_WMI_CPU,
	QUERY_WMI_VGA,
	QUERY_WMI_HDD,
} QueryWmiType;

static char *query_wmi (QueryWmiType mode);
static char *read_os_name (IWbemClassObject *object);
static char *read_cpu_info (IWbemClassObject *object);
static char *read_vga_name (IWbemClassObject *object);

static uint64_t hdd_capacity;
static uint64_t hdd_free_space;
static char *read_hdd_info (IWbemClassObject *object);

static char *bstr_to_utf8 (BSTR bstr);
static uint64_t variant_to_uint64 (VARIANT *variant);
static char *zoitechat_strdup (const char *value);
static char *zoitechat_strdup_printf (const char *format, ...);
static char *zoitechat_strchomp (char *value);

typedef struct
{
	char *data;
	size_t len;
	size_t cap;
} StringBuilder;

static bool string_builder_init (StringBuilder *builder);
static void string_builder_free (StringBuilder *builder);
static bool string_builder_append (StringBuilder *builder, const char *text);

char *
sysinfo_get_cpu (void)
{
	if (cpu_info == NULL)
		cpu_info = query_wmi (QUERY_WMI_CPU);

	return zoitechat_strdup (cpu_info);
}

char *
sysinfo_get_os (void)
{
	if (os_name == NULL)
		os_name = query_wmi (QUERY_WMI_OS);

	if (os_name == NULL)
	{
		return NULL;
	}

	return zoitechat_strdup_printf ("%s (x%d)", os_name, sysinfo_get_cpu_arch ());
}

int
sysinfo_get_cpu_arch (void)
{
	SYSTEM_INFO si;

	if (cpu_arch != 0)
	{
		return cpu_arch;
	}

	GetNativeSystemInfo (&si);

	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
	{
		return cpu_arch = 64;
	}
	else
	{
		return cpu_arch = 86;
	}
}

int
sysinfo_get_build_arch (void)
{
	SYSTEM_INFO si;

	if (build_arch != 0)
	{
		return build_arch;
	}

	GetSystemInfo (&si);

	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
	{
		return build_arch = 64;
	}
	else
	{
		return build_arch = 86;
	}
}

char *
sysinfo_get_gpu (void)
{
	if (vga_name == NULL)
		vga_name = query_wmi (QUERY_WMI_VGA);

	return zoitechat_strdup (vga_name);
}

void
sysinfo_get_hdd_info (uint64_t *hdd_capacity_out, uint64_t *hdd_free_space_out)
{
	char *hdd_info;

	/* HDD information is always loaded dynamically since it includes the current amount of free space */
	*hdd_capacity_out = hdd_capacity = 0;
	*hdd_free_space_out = hdd_free_space = 0;

	hdd_info = query_wmi (QUERY_WMI_HDD);
	if (hdd_info == NULL)
	{
		return;
	}

	*hdd_capacity_out = hdd_capacity;
	*hdd_free_space_out = hdd_free_space;
}

/* https://msdn.microsoft.com/en-us/library/aa390423 */
static char *query_wmi (QueryWmiType type)
{
	StringBuilder result;
	bool result_initialized = false;
	HRESULT hr;

	IWbemLocator *locator = NULL;
	BSTR namespaceName = NULL;
	BSTR queryLanguageName = NULL;
	BSTR query = NULL;
	IWbemServices *namespace = NULL;
	IUnknown *namespaceUnknown = NULL;
	IEnumWbemClassObject *enumerator = NULL;
	int i;
	bool atleast_one_appended = false;

	hr = CoCreateInstance (&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (LPVOID *) &locator);
	if (FAILED (hr))
	{
		goto exit;
	}

	namespaceName = SysAllocString (L"root\\CIMV2");

	hr = locator->lpVtbl->ConnectServer (locator, namespaceName, NULL, NULL, NULL, 0, NULL, NULL, &namespace);
	if (FAILED (hr))
	{
		goto release_locator;
	}

	hr = namespace->lpVtbl->QueryInterface (namespace, &IID_IUnknown, (void**)&namespaceUnknown);
	if (FAILED (hr))
	{
		goto release_namespace;
	}

	hr = CoSetProxyBlanket (namespaceUnknown, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	if (FAILED (hr))
	{
		goto release_namespaceUnknown;
	}

	queryLanguageName = SysAllocString (L"WQL");

	switch (type)
	{
	case QUERY_WMI_OS:
		query = SysAllocString (L"SELECT Caption FROM Win32_OperatingSystem");
		break;
	case QUERY_WMI_CPU:
		query = SysAllocString (L"SELECT Name, MaxClockSpeed FROM Win32_Processor");
		break;
	case QUERY_WMI_VGA:
		query = SysAllocString (L"SELECT Name FROM Win32_VideoController");
		break;
	case QUERY_WMI_HDD:
		/*
		 * Query local fixed logical disks only.
		 *
		 * Win32_Volume can cause Windows to probe additional network-backed
		 * providers (for example Plan9/WSL providers), which may trigger RPC
		 * failures in debugger sessions. Restricting this to fixed logical disks
		 * avoids those probes while still providing useful local disk stats.
		 */
		query = SysAllocString (L"SELECT Name, Size, FreeSpace FROM Win32_LogicalDisk WHERE DriveType = 3");
		break;
	default:
		goto release_queryLanguageName;
	}

	hr = namespace->lpVtbl->ExecQuery (namespace, queryLanguageName, query, WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator);
	if (FAILED (hr))
	{
		goto release_query;
	}

	if (!string_builder_init (&result))
	{
		goto release_query;
	}
	result_initialized = true;

	for (i = 0;; i++)
	{
		ULONG numReturned = 0;
		IWbemClassObject *object;
		char *line;

		hr = enumerator->lpVtbl->Next (enumerator, WBEM_INFINITE, 1, &object, &numReturned);
		if (FAILED (hr) || numReturned == 0)
		{
			break;
		}

		switch (type)
		{
			case QUERY_WMI_OS:
				line = read_os_name (object);
				break;

			case QUERY_WMI_CPU:
				line = read_cpu_info (object);
				break;

			case QUERY_WMI_VGA:
				line = read_vga_name (object);
				break;

			case QUERY_WMI_HDD:
				line = read_hdd_info (object);
				break;

			default:
				break;
		}

		object->lpVtbl->Release (object);

		if (line != NULL)
		{
			if (atleast_one_appended)
			{
				if (!string_builder_append (&result, ", "))
				{
					free (line);
					goto release_query;
				}
			}

			if (!string_builder_append (&result, line))
			{
				free (line);
				goto release_query;
			}

			free (line);

			atleast_one_appended = true;
		}
	}

	enumerator->lpVtbl->Release (enumerator);

release_query:
	SysFreeString (query);

release_queryLanguageName:
	SysFreeString (queryLanguageName);

release_namespaceUnknown:
	namespaceUnknown->lpVtbl->Release (namespaceUnknown);

release_namespace:
	namespace->lpVtbl->Release (namespace);

release_locator:
	locator->lpVtbl->Release (locator);
	SysFreeString (namespaceName);

exit:
	if (!result_initialized || result.data == NULL)
	{
		return NULL;
	}

	return result.data;
}

static char *read_os_name (IWbemClassObject *object)
{
	HRESULT hr;
	VARIANT caption_variant;
	char *caption_utf8;

	hr = object->lpVtbl->Get (object, L"Caption", 0, &caption_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	caption_utf8 = bstr_to_utf8 (caption_variant.bstrVal);

	VariantClear(&caption_variant);

	if (caption_utf8 == NULL)
	{
		return NULL;
	}

	zoitechat_strchomp (caption_utf8);

	return caption_utf8;
}

static char *read_cpu_info (IWbemClassObject *object)
{
	HRESULT hr;
	VARIANT name_variant;
	char *name_utf8;
	VARIANT max_clock_speed_variant;
	uint32_t cpu_freq_mhz;
	char *result;

	hr = object->lpVtbl->Get (object, L"Name", 0, &name_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	name_utf8 = bstr_to_utf8 (name_variant.bstrVal);

	VariantClear (&name_variant);

	if (name_utf8 == NULL)
	{
		return NULL;
	}

	hr = object->lpVtbl->Get (object, L"MaxClockSpeed", 0, &max_clock_speed_variant, NULL, NULL);
	if (FAILED (hr))
	{
		free (name_utf8);

		return NULL;
	}

	cpu_freq_mhz = max_clock_speed_variant.uintVal;

	VariantClear (&max_clock_speed_variant);

	zoitechat_strchomp (name_utf8);

	if (cpu_freq_mhz > 1000)
	{
		result = zoitechat_strdup_printf ("%s (%.2fGHz)", name_utf8, cpu_freq_mhz / 1000.f);
	}
	else
	{
		result = zoitechat_strdup_printf ("%s (%" PRIu32 "MHz)", name_utf8, cpu_freq_mhz);
	}

	free (name_utf8);

	return result;
}

static char *read_vga_name (IWbemClassObject *object)
{
	HRESULT hr;
	VARIANT name_variant;
	char *name_utf8;

	hr = object->lpVtbl->Get (object, L"Name", 0, &name_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	name_utf8 = bstr_to_utf8 (name_variant.bstrVal);

	VariantClear (&name_variant);

	if (name_utf8 == NULL)
	{
		return NULL;
	}

	return zoitechat_strchomp (name_utf8);
}

static char *read_hdd_info (IWbemClassObject *object)
{
	HRESULT hr;
	VARIANT name_variant;
	BSTR name_bstr;
	size_t name_len;
	VARIANT capacity_variant;
	uint64_t capacity;
	VARIANT free_space_variant;
	uint64_t free_space;

	hr = object->lpVtbl->Get (object, L"Name", 0, &name_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	name_bstr = name_variant.bstrVal;
	name_len = SysStringLen (name_variant.bstrVal);

	if (name_len >= 4 && name_bstr[0] == L'\\' && name_bstr[1] == L'\\' && name_bstr[2] == L'?' && name_bstr[3] == L'\\')
	{
		// This is not a named volume. Skip it.
		VariantClear (&name_variant);

		return NULL;
	}

	VariantClear (&name_variant);

	hr = object->lpVtbl->Get (object, L"Size", 0, &capacity_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	capacity = variant_to_uint64 (&capacity_variant);

	VariantClear (&capacity_variant);

	if (capacity == 0)
	{
		return NULL;
	}

	hr = object->lpVtbl->Get (object, L"FreeSpace", 0, &free_space_variant, NULL, NULL);
	if (FAILED (hr))
	{
		return NULL;
	}

	free_space = variant_to_uint64 (&free_space_variant);

	VariantClear (&free_space_variant);

	if (free_space == 0)
	{
		return NULL;
	}

	hdd_capacity += capacity;
	hdd_free_space += free_space;

	return NULL;
}

static char *bstr_to_utf8 (BSTR bstr)
{
	int utf8_len;
	char *utf8;

	if (bstr == NULL)
	{
		return NULL;
	}

	utf8_len = WideCharToMultiByte (CP_UTF8, 0, bstr, SysStringLen (bstr), NULL, 0, NULL, NULL);
	if (utf8_len <= 0)
	{
		return NULL;
	}

	utf8 = malloc ((size_t) utf8_len + 1);
	if (utf8 == NULL)
	{
		return NULL;
	}

	if (WideCharToMultiByte (CP_UTF8, 0, bstr, SysStringLen (bstr), utf8, utf8_len, NULL, NULL) <= 0)
	{
		free (utf8);
		return NULL;
	}

	utf8[utf8_len] = '\0';

	return utf8;
}

static uint64_t variant_to_uint64 (VARIANT *variant)
{
	switch (V_VT (variant))
	{
	case VT_UI8:
		return variant->ullVal;

	case VT_BSTR:
		return wcstoull (variant->bstrVal, NULL, 10);

	default:
		return 0;
	}
}

static char *zoitechat_strdup (const char *value)
{
	size_t len;
	char *copy;

	if (value == NULL)
	{
		return NULL;
	}

	len = strlen (value);
	copy = malloc (len + 1);
	if (copy == NULL)
	{
		return NULL;
	}

	memcpy (copy, value, len + 1);

	return copy;
}

static char *zoitechat_strdup_printf (const char *format, ...)
{
	va_list args;
	va_list args_copy;
	int length;
	char *buffer;

	va_start (args, format);
	va_copy (args_copy, args);
	length = vsnprintf (NULL, 0, format, args_copy);
	va_end (args_copy);

	if (length < 0)
	{
		va_end (args);
		return NULL;
	}

	buffer = malloc ((size_t) length + 1);
	if (buffer == NULL)
	{
		va_end (args);
		return NULL;
	}

	vsnprintf (buffer, (size_t) length + 1, format, args);
	va_end (args);

	return buffer;
}

static char *zoitechat_strchomp (char *value)
{
	size_t len;

	if (value == NULL)
	{
		return NULL;
	}

	len = strlen (value);
	while (len > 0 && isspace ((unsigned char) value[len - 1]) != 0)
	{
		value[len - 1] = '\0';
		len--;
	}

	return value;
}

static bool string_builder_init (StringBuilder *builder)
{
	builder->cap = 64;
	builder->len = 0;
	builder->data = malloc (builder->cap);
	if (builder->data == NULL)
	{
		return false;
	}

	builder->data[0] = '\0';
	return true;
}

static void string_builder_free (StringBuilder *builder)
{
	if (builder->data != NULL)
	{
		free (builder->data);
		builder->data = NULL;
	}
	builder->len = 0;
	builder->cap = 0;
}

static bool string_builder_append (StringBuilder *builder, const char *text)
{
	size_t add_len;
	size_t needed;
	size_t new_cap;
	char *new_data;

	if (text == NULL)
	{
		return true;
	}

	add_len = strlen (text);
	needed = builder->len + add_len + 1;
	if (needed > builder->cap)
	{
		new_cap = builder->cap;
		while (new_cap < needed)
		{
			new_cap *= 2;
		}

		new_data = realloc (builder->data, new_cap);
		if (new_data == NULL)
		{
			string_builder_free (builder);
			return false;
		}

		builder->data = new_data;
		builder->cap = new_cap;
	}

	memcpy (builder->data + builder->len, text, add_len);
	builder->len += add_len;
	builder->data[builder->len] = '\0';
	return true;
}

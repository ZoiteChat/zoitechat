#include "zoitechat.h"
#include "cfgfiles.h"
#include "secretstore.h"

#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <wincred.h>
#endif

#ifdef HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif

static char *secretstore_target (const char *network_name)
{
	return g_strdup_printf ("zoitechat/network/%s", network_name ? network_name : "default");
}

int secretstore_is_keyring_available (void)
{
#ifdef WIN32
	return TRUE;
#elif defined(HAVE_LIBSECRET)
	return TRUE;
#else
	return FALSE;
#endif
}

char *secretstore_get_network_password (const char *network_name)
{
	char *target;
	target = secretstore_target (network_name);
#ifdef WIN32
	{
		PCREDENTIALA cred = NULL;
		char *ret = NULL;
		if (CredReadA (target, CRED_TYPE_GENERIC, 0, &cred))
		{
			ret = g_strndup ((const char *) cred->CredentialBlob, cred->CredentialBlobSize);
			CredFree (cred);
		}
		g_free (target);
		return ret;
	}
#elif defined(HAVE_LIBSECRET)
	{
		static const SecretSchema schema = {
			"net.zoite.ZoiteChat.Network", SECRET_SCHEMA_NONE,
			{
				{ "network", SECRET_SCHEMA_ATTRIBUTE_STRING },
				{ NULL, 0 },
			}
		};
		char *ret = secret_password_lookup_sync (&schema, NULL, NULL, "network", target, NULL);
		g_free (target);
		return ret;
	}
#else
	g_free (target);
	return NULL;
#endif
}

int secretstore_set_network_password (const char *network_name, const char *password)
{
	char *target;
	target = secretstore_target (network_name);
#ifdef WIN32
	{
		CREDENTIALA cred;
		memset (&cred, 0, sizeof (cred));
		cred.Type = CRED_TYPE_GENERIC;
		cred.TargetName = target;
		cred.CredentialBlobSize = (DWORD) strlen (password);
		cred.CredentialBlob = (LPBYTE) password;
		cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
		cred.UserName = "zoitechat";
		if (!CredWriteA (&cred, 0))
		{
			g_free (target);
			return FALSE;
		}
		g_free (target);
		return TRUE;
	}
#elif defined(HAVE_LIBSECRET)
	{
		static const SecretSchema schema = {
			"net.zoite.ZoiteChat.Network", SECRET_SCHEMA_NONE,
			{
				{ "network", SECRET_SCHEMA_ATTRIBUTE_STRING },
				{ NULL, 0 },
			}
		};
		gboolean ok = secret_password_store_sync (&schema, SECRET_COLLECTION_DEFAULT,
			"ZoiteChat network password", password, NULL, NULL, "network", target, NULL);
		g_free (target);
		return ok;
	}
#else
	g_free (target);
	return FALSE;
#endif
}

int secretstore_delete_network_password (const char *network_name)
{
	char *target;
	target = secretstore_target (network_name);
#ifdef WIN32
	{
		gboolean ok = CredDeleteA (target, CRED_TYPE_GENERIC, 0);
		g_free (target);
		return ok;
	}
#elif defined(HAVE_LIBSECRET)
	{
		static const SecretSchema schema = {
			"net.zoite.ZoiteChat.Network", SECRET_SCHEMA_NONE,
			{
				{ "network", SECRET_SCHEMA_ATTRIBUTE_STRING },
				{ NULL, 0 },
			}
		};
		gboolean ok = secret_password_clear_sync (&schema, NULL, NULL, "network", target, NULL);
		g_free (target);
		return ok;
	}
#else
	g_free (target);
	return FALSE;
#endif
}

int secretstore_require_unlock (const char *network_name)
{
#ifdef WIN32
	return TRUE;
#elif defined(HAVE_LIBSECRET)
	{
		static const SecretSchema schema = {
			"net.zoite.ZoiteChat.Network", SECRET_SCHEMA_NONE,
			{
				{ "network", SECRET_SCHEMA_ATTRIBUTE_STRING },
				{ NULL, 0 },
			}
		};
		char *target;
		char *password;
		GError *error = NULL;
		target = secretstore_target (network_name);
		password = secret_password_lookup_sync (&schema, NULL, &error, "network", target, NULL);
		g_free (target);
		if (password)
		{
			memset (password, 0, strlen (password));
			g_free (password);
			return TRUE;
		}
		if (error)
		{
			g_error_free (error);
			return FALSE;
		}
		return TRUE;
	}
#else
	return TRUE;
#endif
}

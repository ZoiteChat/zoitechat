#ifndef ZOITECHAT_SECRETSTORE_H
#define ZOITECHAT_SECRETSTORE_H

char *secretstore_get_network_password (const char *network_name);
int secretstore_set_network_password (const char *network_name, const char *password);
int secretstore_delete_network_password (const char *network_name);
int secretstore_is_keyring_available (void);
int secretstore_require_unlock (const char *network_name);

#endif

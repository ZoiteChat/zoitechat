/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_ALERTS_BRIDGE_H
#define ZOITECHAT_QT_ALERTS_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
struct QtAlertPolicy { int notify, tray, flash, force_notify, omit_focused, quiet; };
int qt_core_alert_policy(void *session, int mention, int private_message, struct QtAlertPolicy *result);
#ifdef __cplusplus
}
#endif
#endif

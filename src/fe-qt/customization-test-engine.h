/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CUSTOMIZATION_TEST_ENGINE_H
#define ZOITECHAT_QT_CUSTOMIZATION_TEST_ENGINE_H
#include "customization-bridge.h"
#include <QList>
#include <QMap>
#include <QString>
#include <cstring>
namespace CustomizationTestEngine {
struct Row { QString name, command; };
QList<Row> lists[QT_CONFIG_COUNT];
int revisions[QT_CONFIG_COUNT] = {};
QStringList executed;
QString lastTarget, lastAll;
void *lastSession = nullptr;
struct Menu { void *id; QString path, label, command, uncheck, group; bool hasCommand = true, toggle = false, radio = false, main = true;
              int position = 0xffff, checked = 0, enabled = 1, key = 0, modifiers = 0; QString icon, markup; };
QList<Menu> menus;
int preference = 0;
}
extern "C" {
int qt_core_config_read(int kind, QtConfigVisitor visitor, void *data, char *revision, int capacity)
{
    if (kind < 0 || kind >= QT_CONFIG_COUNT || capacity < 65) return 0;
    const QByteArray version = QByteArray::number(CustomizationTestEngine::revisions[kind]);
    std::strcpy(revision, version.constData());
    if (visitor) for (const auto &row : CustomizationTestEngine::lists[kind]) visitor(row.name.toUtf8().constData(), row.command.toUtf8().constData(), data);
    return 1;
}
int qt_core_config_save(int kind, const QtConfigRow *rows, int count, const char *revision, char *error, int capacity)
{
    if (kind < 0 || kind >= QT_CONFIG_COUNT) return 0;
    if (QByteArray(revision) != QByteArray::number(CustomizationTestEngine::revisions[kind])) {
        if (capacity > 0) { std::strncpy(error, "List changed; reload before saving.", capacity - 1); error[capacity - 1] = 0; } return 0;
    }
    QList<CustomizationTestEngine::Row> values;
    for (int i = 0; i < count; ++i) {
        if (!*rows[i].name) { if (capacity > 0) { std::strncpy(error, "Name required.", capacity - 1); error[capacity - 1] = 0; } return 0; }
        values.append({QString::fromUtf8(rows[i].name), QString::fromUtf8(rows[i].command)});
    }
    CustomizationTestEngine::lists[kind] = values; ++CustomizationTestEngine::revisions[kind]; qt_fe_customization_changed(kind); return 1;
}
const char *qt_core_replace_word(const char *word)
{
    static QByteArray replacement;
    for (const auto &row : CustomizationTestEngine::lists[QT_CONFIG_REPLACE]) if (row.name == QString::fromUtf8(word)) {
        replacement = row.command.toUtf8(); return replacement.constData();
    }
    return nullptr;
}
int qt_core_custom_pref(const char *) { return CustomizationTestEngine::preference; }
int qt_core_custom_run(void *, const char *command)
{
    const QString value = QString::fromUtf8(command); CustomizationTestEngine::executed.append(value);
    if (value.startsWith(QStringLiteral("set "))) CustomizationTestEngine::preference = value.endsWith(QLatin1Char('1'));
    return 1;
}
void qt_core_menus_foreach(void (*visitor)(const QtDynamicMenu *, void *), void *data)
{
    for (const auto &row : CustomizationTestEngine::menus) {
        const QByteArray path = row.path.toUtf8(), label = row.label.toUtf8(), cmd = row.command.toUtf8(), off = row.uncheck.toUtf8(), group = row.group.toUtf8(), icon = row.icon.toUtf8(), markup = row.markup.toUtf8();
        const QtDynamicMenu entry{row.id, path.constData(), label.constData(), row.hasCommand ? cmd.constData() : nullptr,
            row.toggle ? off.constData() : nullptr, row.radio ? group.constData() : nullptr, row.main, row.position, row.checked, row.enabled, row.key, row.modifiers, icon.constData(), row.markup.isEmpty() ? nullptr : markup.constData()};
        visitor(&entry, data);
    }
}
int qt_core_menu_activate(void *session, void *identity, const char *expected, int checked)
{
    for (auto &row : CustomizationTestEngine::menus) if (row.id == identity && row.command == QString::fromUtf8(expected) && row.enabled) {
        row.checked = checked;
        if (row.radio && checked) for (auto &other : CustomizationTestEngine::menus) if (other.id != identity && other.group == row.group) other.checked = false;
        const QString command = row.toggle && !checked ? row.uncheck : row.command;
        qt_fe_menus_changed(); return qt_core_custom_run(session, command.toUtf8().constData());
    }
    return 0;
}
int qt_core_custom_run_target(void *session, const char *command, const char *target, const char *all) {
    CustomizationTestEngine::lastSession = session; CustomizationTestEngine::lastTarget = QString::fromUtf8(target);
    CustomizationTestEngine::lastAll = QString::fromUtf8(all); return qt_core_custom_run(session, command);
}
int qt_core_menu_activate_target(void *session, void *identity, const char *expected, int checked, const char *, const char *) { return qt_core_menu_activate(session, identity, expected, checked); }
}
#endif

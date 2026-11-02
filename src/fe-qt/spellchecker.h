/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_SPELLCHECKER_H
#define ZOITECHAT_QT_SPELLCHECKER_H
#include <QStringList>
#include <functional>
#include <memory>
class SpellChecker {
public:
    explicit SpellChecker(QString personalFile);
    ~SpellChecker();
    static bool compiled();
    void configure(bool enabled, const QString &languages);
    QStringList availableLanguages();
    QStringList activeLanguages() const;
    QString status() const;
    bool ready() const;
    bool misspelled(const QString &word);
    QStringList suggestions(const QString &word);
    void ignore(const QString &word);
    bool add(const QString &word, QString *error = nullptr);
    std::function<void()> changed;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
#endif

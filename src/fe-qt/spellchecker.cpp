/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "spellchecker.h"
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#ifdef HAVE_QT_ENCHANT
#include <enchant.h>
#endif
struct SpellChecker::Impl {
    QString personalFile, requested, message;
    QStringList available, active;
    QSet<QString> ignored, personal;
    QHash<QString, bool> cache;
    bool enabled = false, configured = false;
#ifdef HAVE_QT_ENCHANT
    EnchantBroker *broker = nullptr;
    QList<EnchantDict *> dictionaries;
    void initialize() {
        if (broker) return;
        broker = enchant_broker_init();
        if (broker) enchant_broker_list_dicts(broker, [](const char *tag, const char *, const char *, const char *, void *data) { static_cast<QStringList *>(data)->append(QString::fromUtf8(tag)); }, &available);
        available.removeDuplicates(); available.sort(Qt::CaseInsensitive);
    }
    void release() { if (broker) for (auto *dict : dictionaries) enchant_broker_free_dict(broker, dict); dictionaries.clear(); }
    ~Impl() { release(); if (broker) enchant_broker_free(broker); }
#endif
};
static bool validWord(const QString &word) {
    static const QRegularExpression pattern(QStringLiteral("^\\p{L}[\\p{L}\\p{M}]*(?:['’][\\p{L}\\p{M}]+)*$"));
    return !word.isEmpty() && word.toUtf8().size() <= 256 && pattern.match(word).hasMatch();
}
static bool readWords(const QString &path, QSet<QString> &words, QString &error) {
    QFile file(path); if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { error = QStringLiteral("Could not read personal dictionary, or file exceeds 1 MiB."); return false; }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.contains('\0') || QString::fromUtf8(bytes).toUtf8() != bytes) { error = QStringLiteral("Personal dictionary is not valid UTF-8 text."); return false; }
    for (const auto &line : QString::fromUtf8(bytes).split(QLatin1Char('\n'))) { const QString word = line.trimmed(); if (validWord(word)) words.insert(word.toCaseFolded()); }
    return true;
}
SpellChecker::SpellChecker(QString personalFile) : d(std::make_unique<Impl>()) { d->personalFile = std::move(personalFile); }
SpellChecker::~SpellChecker() = default;
bool SpellChecker::compiled() {
#ifdef HAVE_QT_ENCHANT
    return true;
#else
    return false;
#endif
}
QStringList SpellChecker::availableLanguages() {
#ifdef HAVE_QT_ENCHANT
    d->initialize();
#endif
    return d->available;
}
void SpellChecker::configure(bool enabled, const QString &languages) {
    if (d->configured && d->enabled == enabled && d->requested == languages) return;
    d->configured = true; d->enabled = enabled; d->requested = languages; d->active.clear(); d->cache.clear(); d->message.clear();
#ifdef HAVE_QT_ENCHANT
    d->release();
#endif
    if (!enabled) { d->message = QStringLiteral("Spell checking is disabled."); return; }
    if (!compiled()) { d->message = QStringLiteral("Spell checking is unavailable in this build."); return; }
#ifdef HAVE_QT_ENCHANT
    d->initialize();
    if (!d->broker) { d->message = QStringLiteral("Could not initialize the spelling provider."); return; }
    QStringList wanted = languages.split(QLatin1Char(','), Qt::SkipEmptyParts); const bool automatic = wanted.isEmpty();
    if (automatic) wanted.append(QLocale::system().name());
    QStringList missing;
    for (QString tag : wanted) {
        tag = tag.trimmed(); if (tag.isEmpty() || tag.size() > 120) continue;
        if (d->active.contains(tag)) continue;
        EnchantDict *dict = nullptr;
#ifdef HAVE_QT_ENCHANT_PWL
        dict = enchant_broker_request_dict_with_pwl(d->broker, tag.toUtf8().constData(), d->personalFile.toUtf8().constData());
#else
        dict = enchant_broker_request_dict(d->broker, tag.toUtf8().constData());
#endif
        if (!dict && (automatic || !tag.contains(QLatin1Char('_')))) {
            const QString base = tag.section(QLatin1Char('_'), 0, 0);
            for (const QString &candidate : d->available) if ((automatic && tag == QStringLiteral("C")) || candidate.section(QLatin1Char('_'), 0, 0).compare(base, Qt::CaseInsensitive) == 0) {
#ifdef HAVE_QT_ENCHANT_PWL
                dict = enchant_broker_request_dict_with_pwl(d->broker, candidate.toUtf8().constData(), d->personalFile.toUtf8().constData());
#else
                dict = enchant_broker_request_dict(d->broker, candidate.toUtf8().constData());
#endif
                if (dict) { tag = candidate; break; }
            }
        }
        if (dict) { if (d->active.contains(tag)) enchant_broker_free_dict(d->broker, dict); else { d->dictionaries.append(dict); d->active.append(tag); } } else missing.append(tag);
    }
    d->personal.clear(); QString error; readWords(d->personalFile, d->personal, error);
    if (!error.isEmpty()) d->message = error;
    else if (d->active.isEmpty()) d->message = QStringLiteral("No requested spelling dictionary is installed. Available: ") + d->available.join(QStringLiteral(", "));
    else d->message = QStringLiteral("Using: ") + d->active.join(QStringLiteral(", ")) + (missing.isEmpty() ? QString() : QStringLiteral(". Unavailable: ") + missing.join(QStringLiteral(", ")));
#endif
}
QStringList SpellChecker::activeLanguages() const { return d->active; }
QString SpellChecker::status() const { return d->message; }
bool SpellChecker::ready() const { return d->enabled && !d->active.isEmpty(); }
bool SpellChecker::misspelled(const QString &word) {
    if (!ready() || !validWord(word)) return false;
    const QString key = word.toCaseFolded(); if (d->ignored.contains(key) || d->personal.contains(key)) return false;
    if (d->cache.contains(word)) return d->cache.value(word);
    bool incorrect = true;
#ifdef HAVE_QT_ENCHANT
    const QByteArray bytes = word.toUtf8();
    for (auto *dict : d->dictionaries) if (enchant_dict_check(dict, bytes.constData(), bytes.size()) != 1) { incorrect = false; break; }
#endif
    if (d->cache.size() >= 8192) d->cache.clear();
    d->cache.insert(word, incorrect); return incorrect;
}
QStringList SpellChecker::suggestions(const QString &word) {
    QStringList result; if (!misspelled(word)) return result;
#ifdef HAVE_QT_ENCHANT
    const QByteArray bytes = word.toUtf8();
    for (auto *dict : d->dictionaries) {
        size_t count = 0; char **list = enchant_dict_suggest(dict, bytes.constData(), bytes.size(), &count);
        if (list) { for (size_t i = 0; i < count && result.size() < 10; ++i) { const QString candidate = QString::fromUtf8(list[i]); if (candidate.size() <= 256 && !result.contains(candidate)) result.append(candidate); } enchant_dict_free_string_list(dict, list); }
        if (result.size() >= 10) break;
    }
#endif
    return result;
}
void SpellChecker::ignore(const QString &word) { if (!validWord(word)) return; d->ignored.insert(word.toCaseFolded()); d->cache.clear(); if (changed) changed(); }
bool SpellChecker::add(const QString &word, QString *error) {
    QString message; QSet<QString> words;
    if (!validWord(word)) message = QStringLiteral("Invalid dictionary word.");
    else if (readWords(d->personalFile, words, message)) {
        words.insert(word.toCaseFolded()); QStringList sorted = words.values(); sorted.sort(); const QByteArray bytes = (sorted.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
        if (bytes.size() > 1024 * 1024) message = QStringLiteral("Personal dictionary exceeds 1 MiB.");
        else { QSaveFile file(d->personalFile); if (!file.open(QIODevice::WriteOnly)) message = file.errorString();
            else { file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); if (file.write(bytes) != bytes.size() || !file.commit()) message = file.errorString(); else { d->personal = words; d->cache.clear(); if (changed) changed(); return true; } }
        }
    }
    if (error) *error = message;
    return false;
}

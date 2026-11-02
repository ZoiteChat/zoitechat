/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "spellchecker.h"
#include "chatinput.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
    QApplication app(argc, argv); QTemporaryDir dir; bool ok = true;
    auto check = [&](bool condition, const char *message) { if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; } };
    const QString path = dir.filePath("spelling.dic");
    auto checker = std::make_shared<SpellChecker>(path);
    checker->configure(false, {}); check(!checker->ready(), "disabled checker");
    checker->configure(true, "zz_missing_dictionary"); check(!checker->ready(), "missing dictionary fails gracefully");
    const auto languages = checker->availableLanguages();
    if (!SpellChecker::compiled() || languages.isEmpty()) {
        check(!checker->misspelled("zxqvblorft"), "unavailable checker leaves text alone");
        std::fprintf(stderr, "No installed spelling provider/dictionaries; dictionary checks skipped\n");
        return ok ? 0 : 1;
    }
    checker->configure(true, languages.first()); check(checker->ready(), "installed dictionary loads");
    check(checker->misspelled("zxqvblorft"), "dictionary rejects nonsense");
    check(!checker->misspelled("bad word"), "reject invalid dictionary token");
    checker->ignore("zxqvblorft"); check(!checker->misspelled("zxqvblorft"), "session ignore clears cache");
    QString error; check(checker->add("codexqtpersonalword", &error), "personal dictionary saves");
    SpellChecker reload(path); reload.configure(true, languages.first());
    check(!reload.misspelled("codexqtpersonalword"), "personal dictionary survives reload");
    check(reload.misspelled("zxqvblorft"), "ignore does not persist");
    check(!reload.add("two words", &error), "invalid personal word refused");
    ChatInput input; InputHistory first, second; input.resize(600, 40); input.show();
    input.setConversation(&first, {"zxqvblorftnick"});
    input.setSpellChecker(checker, true, QColor(255, 0, 0));
    input.setText("zxqvblorfttypo"); input.setCursorPosition(3); input.refreshSpelling();
    check(input.spellingRanges().size() == 1, "input identifies spelling range");
    check(input.text() == "zxqvblorfttypo" && input.cursorPosition() == 3, "checking preserves draft and cursor");
    const auto image = input.grab().toImage(); int red = 0;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) { const auto c = image.pixelColor(x,y); if (c.red() > 200 && c.green() < 80 && c.blue() < 80) ++red; }
    check(red > 5, "spelling underline is rendered");
    for (const QString &text : {QString("/msg zxqvblorfttypo"), QString("https://zxqvblorfttypo.invalid"), QString("zxqvblorftnick")}) {
        input.setText(text); input.refreshSpelling(); check(input.spellingRanges().isEmpty(), "commands URLs and known nicks skipped");
    }
    auto menuAction = [&](bool stale) {
        input.setText("zxqvblorfttypo"); input.setCursorPosition(0); input.refreshSpelling();
        QTimer::singleShot(0, [&]() {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            check(menu != nullptr, "context menu opens"); if (!menu) return;
            QAction *action = nullptr; for (auto *candidate : menu->actions()) if (candidate->objectName() == "spellingIgnore") action = candidate;
            check(action != nullptr, "ignore action offered");
            if (stale) input.setConversation(&second, {});
            if (action) action->trigger();
            menu->close();
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(8,20), input.mapToGlobal(QPoint(8,20)));
        QApplication::sendEvent(&input, &event);
    };
    menuAction(true); check(checker->misspelled("zxqvblorfttypo"), "stale conversation menu cannot ignore word");
    menuAction(false); check(!checker->misspelled("zxqvblorfttypo"), "context menu ignores current word");
    if (languages.first().startsWith("en")) {
        input.setText("\002helo\002 suffix"); input.setCursorPosition(1); input.refreshSpelling();
        const auto suggestions = checker->suggestions("helo");
        check(!suggestions.isEmpty(), "English misspelling offers suggestions");
        QTimer::singleShot(0, [&]() {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            check(menu != nullptr, "correction menu opens"); if (!menu) return;
            QAction *action = nullptr;
            for (auto *candidate : menu->actions()) if (candidate->objectName() == "spellingSuggestion") { action = candidate; break; }
            check(action != nullptr, "correction action offered");
            if (action) action->trigger();
            menu->close();
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(10,20), input.mapToGlobal(QPoint(10,20)));
        QApplication::sendEvent(&input, &event);
        if (!suggestions.isEmpty()) check(input.text() == QString("\002") + suggestions.first() + "\002 suffix", "correction preserves surrounding formatting and suffix");
    }
    return ok ? 0 : 1;
}

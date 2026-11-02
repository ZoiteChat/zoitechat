/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "keybindings.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QRegularExpression>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>
// X11/GDK-compatible values for the existing "Key Press" plugin event.
// No GTK dependency is introduced. Native XKB keysyms retain legacy aliases;
// synthetic and non-XKB events use the corresponding Unicode keysym.
PluginKey pluginKey(const QKeyEvent &event)
{
    PluginKey result;
    const auto modifiers = event.modifiers();
    if (modifiers & Qt::ShiftModifier) result.state |= 1;
    if (modifiers & Qt::ControlModifier) result.state |= 4;
    if (modifiers & Qt::AltModifier) result.state |= 8;
    if (modifiers & Qt::MetaModifier) result.state |= 64;
    if (modifiers & Qt::GroupSwitchModifier) result.state |= 128;
    const int key = event.key();
    unsigned unicode = 0;
    const auto text = event.text().toUcs4();
    if (!text.isEmpty() && text.first() >= 32 && text.first() != 127) unicode = text.first();
    else if (key >= 32 && key <= 0x10ffff) {
        unicode = key;
        if (!(modifiers & Qt::ShiftModifier)) unicode = QChar::toLower(unicode);
    }
    result.unicode = unicode;
    result.keyval = unicode ? (unicode <= 255 ? unicode : 0x01000000u | unicode) : 0;
    const bool keypad = modifiers & Qt::KeypadModifier;
    if (keypad && key >= Qt::Key_0 && key <= Qt::Key_9) result.keyval = 0xffb0 + key - Qt::Key_0;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F35) result.keyval = 0xffbe + key - Qt::Key_F1;
    else switch (key) {
    case Qt::Key_Backspace: result.keyval = 0xff08; result.unicode = 8; break;
    case Qt::Key_Tab: result.keyval = keypad ? 0xff89 : 0xff09; result.unicode = keypad ? 0 : 9; break;
    case Qt::Key_Backtab: result.keyval = 0xfe20; result.unicode = 0; result.state |= 1; break;
    case Qt::Key_Return: result.keyval = 0xff0d; result.unicode = 13; break;
    case Qt::Key_Enter: result.keyval = 0xff8d; result.unicode = 0; break;
    case Qt::Key_Escape: result.keyval = 0xff1b; result.unicode = 27; break;
    case Qt::Key_Delete: result.keyval = keypad ? 0xff9f : 0xffff; result.unicode = keypad ? 0 : 127; break;
    case Qt::Key_Insert: result.keyval = keypad ? 0xff9e : 0xff63; break;
    case Qt::Key_Home: result.keyval = keypad ? 0xff95 : 0xff50; break;
    case Qt::Key_Left: result.keyval = keypad ? 0xff96 : 0xff51; break;
    case Qt::Key_Up: result.keyval = keypad ? 0xff97 : 0xff52; break;
    case Qt::Key_Right: result.keyval = keypad ? 0xff98 : 0xff53; break;
    case Qt::Key_Down: result.keyval = keypad ? 0xff99 : 0xff54; break;
    case Qt::Key_PageUp: result.keyval = keypad ? 0xff9a : 0xff55; break;
    case Qt::Key_PageDown: result.keyval = keypad ? 0xff9b : 0xff56; break;
    case Qt::Key_End: result.keyval = keypad ? 0xff9c : 0xff57; break;
    case Qt::Key_Clear: result.keyval = keypad ? 0xff9d : 0xff0b; result.unicode = keypad ? 0 : 11; break;
    case Qt::Key_Pause: result.keyval = 0xff13; break;
    case Qt::Key_Print: result.keyval = 0xff61; break;
    case Qt::Key_Menu: result.keyval = 0xff67; break;
    case Qt::Key_Help: result.keyval = 0xff6a; break;
    case Qt::Key_Shift: result.keyval = 0xffe1; break;
    case Qt::Key_Control: result.keyval = 0xffe3; break;
    case Qt::Key_Meta: result.keyval = 0xffeb; break;
    case Qt::Key_Alt: result.keyval = 0xffe9; break;
    case Qt::Key_AltGr: result.keyval = 0xfe03; break;
    case Qt::Key_CapsLock: result.keyval = 0xffe5; break;
    case Qt::Key_NumLock: result.keyval = 0xff7f; break;
    case Qt::Key_ScrollLock: result.keyval = 0xff14; break;
    default: break;
    }
    if (keypad) {
        if (key == Qt::Key_Plus) result.keyval = 0xffab;
        else if (key == Qt::Key_Minus) result.keyval = 0xffad;
        else if (key == Qt::Key_Asterisk) result.keyval = 0xffaa;
        else if (key == Qt::Key_Slash) result.keyval = 0xffaf;
        else if (key == Qt::Key_Period) result.keyval = 0xffae;
        else if (key == Qt::Key_Comma) result.keyval = 0xffac;
        else if (key == Qt::Key_Equal) result.keyval = 0xffbd;
        else if (key == Qt::Key_Space) result.keyval = 0xff80;
    }
    if (key >= 0x01001250 && key <= 0x01001293) { result.keyval = 0xfe00 + key - 0x01001200; result.unicode = 0; }
    const QString platform = QGuiApplication::platformName();
    if (event.nativeVirtualKey() && (platform == QStringLiteral("xcb") || platform.startsWith(QStringLiteral("wayland")))) {
        result.keyval = event.nativeVirtualKey();
        // Preserve layout/Caps Lock casing even when Ctrl changes event.text().
        if (result.keyval >= 32 && result.keyval <= 255) result.unicode = result.keyval;
        else if ((result.keyval & 0xff000000u) == 0x01000000u) result.unicode = result.keyval & 0x00ffffffu;
    }
    return result;
}
QList<InputBinding> defaultInputBindings(const QString &prefix)
{
    QList<InputBinding> rows;
    auto add = [&](const QString &key, const QString &action, const QString &first = {}, const QString &second = {}) { rows.append({key, action, first, second}); };
    add(QStringLiteral("<Primary>Page_Up"), QStringLiteral("Change Page"), QStringLiteral("-1"), QStringLiteral("Relative"));
    add(QStringLiteral("<Primary>Page_Down"), QStringLiteral("Change Page"), QStringLiteral("1"), QStringLiteral("Relative"));
    for (int i = 9; i >= 1; --i) add(QStringLiteral("<Alt>%1").arg(i), QStringLiteral("Change Page"), QString::number(i));
    add(QStringLiteral("<Alt>grave"), QStringLiteral("Change Page"), QStringLiteral("auto"));
    const QList<QPair<QString, ushort>> controls{{"o",15},{"b",2},{"k",3},{"i",29},{"u",31},{"r",22}};
    for (const auto &control : controls) add(QStringLiteral("<Primary>") + control.first, QStringLiteral("Insert in Buffer"), QString(QChar(control.second)));
    add(QStringLiteral("<Shift>Page_Down"), QStringLiteral("Change Selected Nick"));
    add(QStringLiteral("<Shift>Page_Up"), QStringLiteral("Change Selected Nick"), QStringLiteral("Up"));
    const QList<QPair<QString, QString>> scroll{{"Page_Down","Down"},{"<Primary>Home","Top"},{"<Primary>End","Bottom"},{"Page_Up","Up"},{"<Shift>Down","+1"},{"<Shift>Up","-1"}};
    for (const auto &entry : scroll) add(entry.first, QStringLiteral("Scroll Page"), entry.second);
    add(QStringLiteral("Down"), QStringLiteral("Next Command")); add(QStringLiteral("Up"), QStringLiteral("Last Command"));
    add(QStringLiteral("Tab"), QStringLiteral("Complete nick/command"));
    add(QStringLiteral("<Shift>ISO_Left_Tab"), QStringLiteral("Complete nick/command"), QStringLiteral("Previous"));
    for (const auto &key : {QStringLiteral("space"), QStringLiteral("Return"), QStringLiteral("KP_Enter")}) add(key, QStringLiteral("Check For Replace"));
    add(QStringLiteral("<Primary>Tab"), QStringLiteral("Complete nick/command"), QStringLiteral("Up"));
    add(QStringLiteral("<Alt>Left"), QStringLiteral("Move front tab left")); add(QStringLiteral("<Alt>Right"), QStringLiteral("Move front tab right"));
    add(QStringLiteral("<Primary><Shift>Page_Up"), QStringLiteral("Move tab family left")); add(QStringLiteral("<Primary><Shift>Page_Down"), QStringLiteral("Move tab family right"));
    add(QStringLiteral("F9"), QStringLiteral("Run Command"), prefix.isEmpty() ? QString() : prefix + QStringLiteral("GUI MENU TOGGLE"));
    return rows;
}
namespace {
QStringList actions() { return {QStringLiteral("Run Command"), QStringLiteral("Insert in Buffer"), QStringLiteral("Set Buffer"), QStringLiteral("Change Page"), QStringLiteral("Scroll Page"), QStringLiteral("Last Command"), QStringLiteral("Next Command"), QStringLiteral("Complete nick/command"), QStringLiteral("Change Selected Nick"), QStringLiteral("Move front tab left"), QStringLiteral("Move front tab right"), QStringLiteral("Push input line into history"), QStringLiteral("Check For Replace"), QStringLiteral("Move tab family left"), QStringLiteral("Move tab family right")}; }
QKeySequence eventKey(const QKeyEvent *event) {
    int key = event->key(); auto modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier);
    if (key == Qt::Key_Backtab) { key = Qt::Key_Tab; modifiers |= Qt::ShiftModifier; }
    return QKeySequence(QKeyCombination(modifiers, Qt::Key(key)));
}
bool readFile(const QString &path, QByteArray &bytes, bool &exists, QString &error) {
    QFile file(path); exists = file.exists();
    if (!exists) { bytes.clear(); return true; }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) { error = QStringLiteral("Could not read key bindings, or file exceeds 4 MiB."); return false; }
    bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) { error = file.errorString(); return false; }
    return true;
}
bool parse(const QByteArray &bytes, QList<InputBinding> &rows, QStringList &comments, QString &error) {
    if (bytes.contains('\0') || QString::fromUtf8(bytes).toUtf8() != bytes) { error = QStringLiteral("Key bindings must contain UTF-8 text without NUL bytes."); return false; }
    QStringList lines;
    for (QString line : QString::fromUtf8(bytes).split(QLatin1Char('\n'))) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        if (line.startsWith(QLatin1Char('#'))) comments.append(line);
        else if (!line.isEmpty()) lines.append(line);
    }
    for (int i = 0; i < lines.size();) {
        if (rows.size() >= 4096) { error = QStringLiteral("At most 4,096 bindings are supported."); return false; }
        QString key;
        if (lines[i].startsWith(QStringLiteral("ACCEL="))) key = lines[i++].mid(6);
        else {
            const QString modifiers = lines[i++].trimmed();
            if (i >= lines.size() || (modifiers != QStringLiteral("None") && (modifiers.isEmpty() || modifiers.contains(QRegularExpression(QStringLiteral("[^CAS]")))))) {
                error = QStringLiteral("Invalid legacy binding modifiers. Original file has been left unchanged."); return false;
            }
            if (modifiers != QStringLiteral("None")) {
                if (modifiers.contains(QLatin1Char('C'))) key += QStringLiteral("<Primary>");
                if (modifiers.contains(QLatin1Char('A'))) key += QStringLiteral("<Alt>");
                if (modifiers.contains(QLatin1Char('S'))) key += QStringLiteral("<Shift>");
            }
            key += lines[i++].trimmed();
        }
        if (i + 2 >= lines.size()) { error = QStringLiteral("Incomplete key-binding file. Original file has been left unchanged."); return false; }
        const QString action = lines[i++].trimmed(), first = lines[i++], second = lines[i++];
        if (key.isEmpty() || action.isEmpty() || !(first == QStringLiteral("D1!") || first.startsWith(QStringLiteral("D1:"))) || !(second == QStringLiteral("D2!") || second.startsWith(QStringLiteral("D2:")))) {
            error = QStringLiteral("Invalid binding fields. Original file has been left unchanged."); return false;
        }
        if (key.toUtf8().size() > 120 || action.toUtf8().size() > 120 || first.mid(3).toUtf8().size() > 480 || second.mid(3).toUtf8().size() > 480) {
            error = QStringLiteral("A binding exceeds the native field byte limits. Original file has been left unchanged."); return false;
        }
        rows.append({key, action, first.startsWith(QStringLiteral("D1:")) ? first.mid(3) : QString(), second.startsWith(QStringLiteral("D2:")) ? second.mid(3) : QString()});
    }
    return true;
}
class ActionDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override { auto *combo = new QComboBox(parent); combo->setEditable(true); combo->addItems(actions()); return combo; }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override { static_cast<QComboBox *>(editor)->setCurrentText(index.data().toString()); }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override { model->setData(index, static_cast<QComboBox *>(editor)->currentText()); }
};
class BindingEditor final : public QDialog {
public:
    BindingEditor(const QString &path, QWidget *parent, std::function<void()> applied, QList<InputBinding> defaults) : QDialog(parent), path_(path), applied_(std::move(applied)), defaults_(std::move(defaults)) {
        setObjectName(QStringLiteral("keyBindingsEditor")); setWindowTitle(QStringLiteral("Keyboard Shortcuts")); resize(980, 500); setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(this);
        auto *help = new QLabel(QStringLiteral("Bindings apply while the message input has focus, before standard shortcuts. Keys use native notation such as <Primary>F10 or <Alt>x. Run Command uses Data 1; literal \\n separates commands and \\\\ means a backslash. Unsupported entries are preserved but inactive. Without a binding file, native defaults are used; Save writes them as an explicit configuration. Existing files replace those implicit bindings. Unhandled keys retain widget/menu behavior."), this); help->setTextFormat(Qt::PlainText); help->setWordWrap(true); layout->addWidget(help);
        model_ = new QStandardItemModel(this); model_->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Action"), QStringLiteral("Data 1"), QStringLiteral("Data 2"), QStringLiteral("Qt status")});
        table_ = new QTableView(this); table_->setObjectName(QStringLiteral("keyBindingRows")); table_->setModel(model_); table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection); table_->setItemDelegateForColumn(1, new ActionDelegate(table_)); table_->horizontalHeader()->setStretchLastSection(true); layout->addWidget(table_, 1);
        error_ = new QLabel(this); error_->setObjectName(QStringLiteral("keyBindingError")); error_->setTextFormat(Qt::PlainText); error_->setWordWrap(true); layout->addWidget(error_);
        auto *row = new QHBoxLayout;
        auto button = [&](const QString &text, const QString &id) { auto *b = new QPushButton(text, this); b->setObjectName(id); row->addWidget(b); return b; };
        auto *add = button(QStringLiteral("Add"), QStringLiteral("addKeyBinding")), *remove = button(QStringLiteral("Remove"), QStringLiteral("removeKeyBinding")), *up = button(QStringLiteral("Up"), QStringLiteral("keyBindingUp")), *down = button(QStringLiteral("Down"), QStringLiteral("keyBindingDown")), *reload = button(QStringLiteral("Reload"), QStringLiteral("reloadKeyBindings")); layout->addLayout(row);
        connect(add, &QPushButton::clicked, this, [this]() { append({QStringLiteral("F10"), QStringLiteral("Run Command"), {}, {}}); table_->selectRow(model_->rowCount()-1); });
        connect(remove, &QPushButton::clicked, this, [this]() { const int row = table_->currentIndex().row(); if (row >= 0) model_->removeRow(row); });
        auto move = [this](int direction) { const int from = table_->currentIndex().row(), to = from + direction; if (from < 0 || to < 0 || to >= model_->rowCount()) return; auto items = model_->takeRow(from); model_->insertRow(to, items); table_->selectRow(to); };
        connect(up, &QPushButton::clicked, this, [move]() { move(-1); }); connect(down, &QPushButton::clicked, this, [move]() { move(1); }); connect(reload, &QPushButton::clicked, this, [this]() { load(); });
        connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) { if (item->column() >= 4) return; updateStatus(item->row()); });
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save(); }); load();
    }
private:
    void append(const InputBinding &binding) {
        QList<QStandardItem *> items; for (const auto &value : {binding.key,binding.action,binding.first,binding.second,QString()}) items.append(new QStandardItem(value)); items.last()->setEditable(false); model_->appendRow(items); updateStatus(model_->rowCount()-1);
    }
    void updateStatus(int row) { if (row < 0 || row >= model_->rowCount()) return; model_->item(row,4)->setText(inputBindingSupported(model_->item(row,1)->text()) && !inputBindingKey(model_->item(row,0)->text()).isEmpty() ? QStringLiteral("Active") : QStringLiteral("Unsupported in Qt")); }
    void load() {
        QList<InputBinding> rows; QStringList comments; QString error; QByteArray bytes; bool exists;
        if (!readFile(path_, bytes, exists, error) || !parse(bytes, rows, comments, error)) { loaded_ = false; error_->setText(error); return; }
        if (!exists) rows = defaults_;
        original_ = bytes; existed_ = exists; comments_ = comments; loaded_ = true; model_->removeRows(0, model_->rowCount()); for (const auto &row : rows) append(row); error_->clear();
    }
    void save() {
        if (!loaded_) { error_->setText(QStringLiteral("Reload a supported binding file before saving.")); return; }
        QByteArray current; bool exists; QString error;
        if (!readFile(path_, current, exists, error)) { error_->setText(error); return; }
        if (exists != existed_ || current != original_) { error_->setText(QStringLiteral("Key bindings changed externally. Reload before saving.")); return; }
        QByteArray output; for (const auto &comment : comments_) output += comment.toUtf8() + '\n';
        if (model_->rowCount() > 4096) { error_->setText(QStringLiteral("At most 4,096 bindings are supported.")); return; }
        for (int row = 0; row < model_->rowCount(); ++row) {
            QStringList values; for (int col = 0; col < 4; ++col) values.append(model_->item(row,col)->text());
            for (int col = 0; col < 4; ++col) if (values[col].contains(QLatin1Char('\n')) || values[col].contains(QLatin1Char('\r')) || values[col].contains(QChar(0)) || values[col].toUtf8().size() > (col < 2 ? 120 : 480) || (col < 2 && values[col].isEmpty())) { error_->setText(QStringLiteral("Keys/actions need names; all fields must be single lines within native byte limits.")); return; }
            output += "\nACCEL=" + values[0].toUtf8() + '\n' + values[1].toUtf8() + '\n';
            for (int i = 2; i < 4; ++i) output += QByteArray(i == 2 ? "D1" : "D2") + (values[i].isEmpty() ? "!" : ":" + values[i].toUtf8()) + '\n';
        }
        if (output.size() > 4 * 1024 * 1024) { error_->setText(QStringLiteral("Key bindings exceed the 4 MiB file limit.")); return; }
        QSaveFile file(path_); if (!file.open(QIODevice::WriteOnly)) { error_->setText(file.errorString()); return; }
        if (!existed_) file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (file.write(output) != output.size() || !file.commit()) { error_->setText(file.errorString()); return; }
        const QPointer<BindingEditor> guard(this); const auto callback = applied_; if (callback) callback(); if (guard) accept();
    }
    QString path_; std::function<void()> applied_; QList<InputBinding> defaults_; QByteArray original_; QStringList comments_; bool loaded_ = false, existed_ = false;
    QStandardItemModel *model_; QTableView *table_; QLabel *error_;
};
}
QKeySequence inputBindingKey(const QString &key)
{
    QString name = key; Qt::KeyboardModifiers modifiers;
    while (name.startsWith(QLatin1Char('<'))) {
        const int end = name.indexOf(QLatin1Char('>')); if (end < 0) return {};
        const QString modifier = name.mid(1,end-1).toLower(); name.remove(0,end+1);
        if (modifier == QStringLiteral("primary") || modifier == QStringLiteral("control") || modifier == QStringLiteral("ctrl")) modifiers |= Qt::ControlModifier;
        else if (modifier == QStringLiteral("shift")) modifiers |= Qt::ShiftModifier;
        else if (modifier == QStringLiteral("alt") || modifier == QStringLiteral("mod1")) modifiers |= Qt::AltModifier;
        else if (modifier == QStringLiteral("super") || modifier == QStringLiteral("meta") || modifier == QStringLiteral("mod4")) modifiers |= Qt::MetaModifier;
        else return {};
    }
    const QMap<QString, QString> names{{"Page_Up","PgUp"},{"Page_Down","PgDown"},{"ISO_Left_Tab","Tab"},{"space","Space"},{"grave","`"},{"BackSpace","Backspace"},{"Escape","Esc"},{"minus","-"},{"plus","+"},{"comma",","},{"period","."},{"slash","/"},{"backslash","\\"},{"equal","="},{"bracketleft","["},{"bracketright","]"}};
    if (name == QStringLiteral("ISO_Left_Tab")) modifiers |= Qt::ShiftModifier;
    if (name.startsWith(QStringLiteral("KP_"))) { modifiers |= Qt::KeypadModifier; name.remove(0,3); if (name == QStringLiteral("Enter")) return QKeySequence(QKeyCombination(modifiers, Qt::Key_Enter)); }
    name = names.value(name,name); const auto parsed = QKeySequence::fromString(name, QKeySequence::PortableText);
    if (parsed.isEmpty() || parsed.count() != 1 || parsed[0].key() == Qt::Key_unknown || parsed[0].keyboardModifiers() != Qt::NoModifier) return {};
    return QKeySequence(QKeyCombination(modifiers, parsed[0].key()));
}
bool inputBindingSupported(const QString &action) { return actions().contains(action); }
QStringList inputBindingCommands(const QString &data)
{
    QString decoded;
    for (int i = 0; i < data.size(); ++i) {
        if (data[i] == QLatin1Char('\\') && i + 1 < data.size()) { const auto next = data[++i]; if (next == QLatin1Char('n')) decoded += QLatin1Char('\n'); else if (next == QLatin1Char('\\')) decoded += next; else { decoded += QLatin1Char('\\'); decoded += next; } }
        else decoded += data[i];
    }
    return decoded.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}
InputBindings::InputBindings(QWidget *input, const QString &path, std::function<bool(const InputBinding &)> run, std::function<QString()> commandPrefix)
    : QObject(input), commandPrefix_(std::move(commandPrefix)), input_(input), path_(path), run_(std::move(run)) { input_->installEventFilter(this); reload(); }
bool InputBindings::reload(QString *error) {
    QByteArray bytes; bool exists; QString message; QList<InputBinding> rows; QStringList comments;
    if (!readFile(path_, bytes, exists, message) || !parse(bytes, rows, comments, message)) { if (error) *error = message; return false; }
    if (!exists) rows = defaultInputBindings(commandPrefix_ ? commandPrefix_() : QStringLiteral("/"));
    QList<QPair<QKeySequence, InputBinding>> resolved;
    for (const auto &row : rows) { const auto key = inputBindingKey(row.key); if (inputBindingSupported(row.action) && !key.isEmpty()) resolved.append({key, row}); }
    bindings_ = resolved; return true;
}
bool InputBindings::eventFilter(QObject *object, QEvent *event) {
    if (object == input_ && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (event->type() == QEvent::KeyPress) {
            const QPointer<InputBindings> guard(this); const auto hook = keypress;
            const bool consumed = hook && hook(*key);
            if (!guard || consumed) return true;
        }
        const auto sequence = eventKey(key);
        // A command may reload the binding list, so dispatch from a snapshot.
        const auto rows = bindings_;
        for (const auto &entry : rows) if (entry.first == sequence) {
            const auto row = entry.second;
            if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
            if (key->isAutoRepeat() && row.action == QStringLiteral("Run Command")) return true;
            const QPointer<InputBindings> guard(this); const auto callback = run_;
            const bool handled = callback && callback(row);
            if (!guard || handled) return true;
        }
    }
    return QObject::eventFilter(object,event);
}
void InputBindings::edit(QWidget *parent) {
    const QPointer<InputBindings> guard(this); auto *editor = new BindingEditor(path_,parent,[guard]() { if (guard) { guard->reload(); const auto callback = guard->applied; if (callback) callback(); } }, defaultInputBindings(commandPrefix_ ? commandPrefix_() : QStringLiteral("/"))); editor->show(); editor->raise(); editor->activateWindow();
}

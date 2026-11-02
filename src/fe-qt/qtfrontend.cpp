/*
 * ZoiteChat Qt 6 prototype. The engine and plugin ABI remain C.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qt-bridge.h"
#include "ircformat.h"
#include "topicentry.h"
#include "panedrag.h"
#include "buffersearch.h"
#include "networkdialog.h"
#include "chatinput.h"
#include "spellchecker.h"
#include "chatbackground.h"
#include <QPainter>
#include "completion-bridge.h"
#include "nicklist.h"
#include "rankicon.h"
#include "irctools.h"
#include "moderation.h"
#include "moderation-bridge.h"
#include "desktopintegration.h"
#include "alerts-bridge.h"
#include "urlhistory.h"
#include "friends.h"
#include "friends-bridge.h"
#include "pastepreview.h"
#include "customization.h"
#include "textevents.h"
#include "paletteeditor.h"
#include "addons.h"
#include "addons-bridge.h"
#include "characterchart.h"
#include "keybindings.h"
#include "preferences-bridge.h"
#include <QStackedWidget>
#include <QComboBox>
#include "conversationoptions.h"
#include "conversationoptions-bridge.h"
#include "transfers.h"
#include "transfers-bridge.h"
#include "customization-bridge.h"
#include <QTextBlock>
#include "tools-bridge.h"
#include <memory>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QPushButton>
#include <QProgressBar>
#include <QToolButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCursor>
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QGridLayout>
#include <QScrollArea>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCryptographicHash>
#include <QTextBrowser>
#include <QTextDocument>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QTabBar>
#include <QStylePainter>
#include <QStyleOptionTab>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <glib.h>

namespace {
QString plainIrcText(const char *text, bool trim = true)
{
    QString result;
    const QPalette palette;
    for (const IrcRun &run : parseIrcText(QString::fromUtf8(text ? text : ""), palette))
        result += run.text;
    return trim ? result.trimmed() : result;
}

QString searchTranscript(QTextBrowser *chat, const QString &query, bool backwards, bool matchCase, bool regex)
{
    QTextDocument::FindFlags flags;
    if (backwards) flags |= QTextDocument::FindBackward;
    if (matchCase) flags |= QTextDocument::FindCaseSensitively;
    QRegularExpression expression(query, matchCase ? QRegularExpression::UseUnicodePropertiesOption : QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::CaseInsensitiveOption);
    if (regex && !expression.isValid()) return QStringLiteral("Invalid regular expression");
    auto find = [&](const QTextCursor &cursor) { return regex ? chat->document()->find(expression, cursor, flags) : chat->document()->find(query, cursor, flags); };
    QTextCursor match = find(chat->textCursor()); const bool wrapped = match.isNull();
    if (wrapped) { QTextCursor start(chat->document()); start.movePosition(backwards ? QTextCursor::End : QTextCursor::Start); match = find(start); }
    if (match.isNull()) return QStringLiteral("No match");
    chat->setTextCursor(match); chat->ensureCursorVisible();
    return wrapped ? QStringLiteral("Wrapped") : QStringLiteral("Found");
}

QStringList completionWords(void *session, int kind)
{
    QStringList words;
    qt_core_completion_read(session, kind, [](const char *name, void *data) { static_cast<QStringList *>(data)->append(QString::fromUtf8(name)); }, &words);
    return words;
}

struct MarkerTag final : public QTextBlockUserData {
    explicit MarkerTag(quint64 value) : id(value) {}
    quint64 id;
};
struct Conversation {
    QString name;
    QString topic;
    QTextDocument *document = nullptr;
    int scrollPosition = -1;
    int inputCursor = 0;
    int activity = 0;
    QTextBlock marker;
    quint64 markerId = 0;
    std::shared_ptr<NickListState> users = std::make_shared<NickListState>();
    QString draft;
    std::shared_ptr<InputHistory> history = std::make_shared<InputHistory>();
};

class ConversationTree final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    void setTreeLines(bool enabled) { lines_ = enabled; viewport()->update(); }
protected:
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override {
        const int step = indentation(); if (step <= 0) return;
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        const int center = rtl ? rect.left() + step / 2 : rect.right() - step / 2;
        if (lines_) {
            painter->save(); painter->setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
            int x = center; auto current = index; bool leaf = true;
            while (current.isValid() && x >= rect.left() && x <= rect.right()) {
                const bool sibling = current.row() + 1 < model()->rowCount(current.parent());
                if (leaf) {
                    painter->drawLine(x, rect.top(), x, sibling ? rect.bottom() : rect.center().y());
                    painter->drawLine(x, rect.center().y(), rtl ? rect.left() : rect.right(), rect.center().y());
                } else if (sibling) painter->drawLine(x, rect.top(), x, rect.bottom());
                leaf = false; current = current.parent(); x += rtl ? step : -step;
            }
            painter->restore();
        }
        QStyleOption option; option.initFrom(this); option.rect = QRect(center - step / 2, rect.top(), step, rect.height());
        if (model()->hasChildren(index)) option.state |= QStyle::State_Children;
        if (isExpanded(index)) option.state |= QStyle::State_Open;
        // Omit Item/Sibling flags so the style paints its expander without connecting lines.
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter, this);
    }
private:
    bool lines_ = false;
};

class ConversationTabs final : public QTabBar {
public:
    using QTabBar::QTabBar;
protected:
    bool vertical() const { return shape() == RoundedWest || shape() == RoundedEast; }
    QSize tabSizeHint(int index) const override { const auto size = QTabBar::tabSizeHint(index); return vertical() ? size.transposed() : size; }
    void paintEvent(QPaintEvent *event) override {
        if (!vertical()) { QTabBar::paintEvent(event); return; }
        QStylePainter painter(this);
        for (int i = 0; i < count(); ++i) {
            QStyleOptionTab option; initStyleOption(&option, i); painter.drawControl(QStyle::CE_TabBarTabShape, option);
            painter.setPen(tabTextColor(i).isValid() ? tabTextColor(i) : palette().color(QPalette::WindowText));
            painter.drawText(tabRect(i).adjusted(6, 2, -6, -2), Qt::AlignCenter | Qt::TextSingleLine, QString(tabText(i)).replace(QStringLiteral("&&"), QStringLiteral("&")));
        }
    }
};

class ChatTranscript final : public QTextBrowser {
public:
    using QTextBrowser::QTextBrowser;
    bool autoCopy = false, copyTimestamps = false, copyColors = false;
    std::function<bool()> followNewText;
    bool atBottom() const {
        const auto *scroll = verticalScrollBar();
        return scroll->maximum() - scroll->value() <= 20;
    }
    int savedScrollPosition() const {
        return followNewText && followNewText() && atBottom() ? -1 : verticalScrollBar()->value();
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        const bool follow = followNewText && followNewText() && atBottom();
        QTextBrowser::resizeEvent(event);
        if (follow) verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
    Qt::KeyboardModifiers selectionModifiers_;
    QMimeData *createMimeDataFromSelection() const override {
        auto *data = new QMimeData;
        data->setText(copyIrcSelection(textCursor(), copyTimestamps || (selectionModifiers_ & Qt::ShiftModifier), copyColors || (selectionModifiers_ & Qt::ControlModifier)));
        return data;
    }
    void mousePressEvent(QMouseEvent *event) override { selectionModifiers_ = event->modifiers(); QTextBrowser::mousePressEvent(event); }
    void mouseReleaseEvent(QMouseEvent *event) override {
        selectionModifiers_ = event->modifiers();
        QTextBrowser::mouseReleaseEvent(event);
        if (autoCopy && event->button() == Qt::LeftButton && textCursor().hasSelection()) {
            copy();
        }
    }
};

class ConversationWindow final : public QMainWindow {
public:
    std::function<void()> activated, attaching;
    ChatTranscript *chat = nullptr;
    QPushButton *nickname = nullptr;
    InputBindings *bindings = nullptr;
    CustomMenus *menus = nullptr;
    QAction *menuVisible = nullptr, *showModes = nullptr, *showUserButtons = nullptr, *showDialogButtons = nullptr;
    QWidget *userPanel = nullptr, *modeRow = nullptr;
    QString geometryKey;
    QSplitter *splitter = nullptr;
    QScrollArea *userButtons = nullptr, *dialogButtons = nullptr;
    QMap<QChar, QToolButton *> modeButtons;
    QToolButton *keyStatus = nullptr, *limitStatus = nullptr;
    QLineEdit *userFilter = nullptr;
    QLabel *userCount = nullptr, *lagStatus = nullptr, *queueStatus = nullptr;
    QProgressBar *lagMeter = nullptr, *queueMeter = nullptr;
    QLineEdit *search = nullptr;
    QLabel *searchStatus = nullptr;
    QCheckBox *matchCase = nullptr, *regexSearch = nullptr;
    ChatInput *input = nullptr;
    TopicEntry *topic = nullptr;
    NickList *users = nullptr;
protected:
    void changeEvent(QEvent *event) override {
        QMainWindow::changeEvent(event);
        if (event->type() == QEvent::ActivationChange && isActiveWindow() && activated) activated();
    }
    void closeEvent(QCloseEvent *event) override {
        event->ignore();
        if (attaching) attaching();
    }
};

class MainWindow final : public QMainWindow {
public:
    MainWindow()
    {
        loadIrcPalette(QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("colors.conf")));
        setWindowTitle(QStringLiteral("ZoiteChat — Qt 6"));
        resize(1080, 700);
        settings_ = std::make_unique<QSettings>(
            QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("qt-frontend.ini")),
            QSettings::IniFormat);

        spellChecker_ = std::make_shared<SpellChecker>(QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("spelling.dic")));
        spellChecker_->changed = [guard = QPointer<MainWindow>(this)]() { if (!guard) return; if (guard->input_) guard->input_->refreshSpelling(); for (auto *view : guard->detached_) view->input->refreshSpelling(); };
        auto *center = new QWidget(this);
        auto *layout = new QVBoxLayout(center);
        layout->setContentsMargins(5, 5, 5, 5);
        auto *split = new QSplitter(Qt::Horizontal, center);
        split_ = split;
        leftDock_ = new QSplitter(Qt::Vertical, split); leftDock_->setObjectName(QStringLiteral("leftDock")); leftDock_->setMaximumWidth(400);
        channels_ = new ConversationTree(leftDock_);
        channels_->viewport()->installEventFilter(this);
        channels_->setHeaderHidden(true);
        channels_->setMinimumWidth(130);
        channels_->setMaximumWidth(340);
        auto *middle = new QWidget(split); middle_ = middle; middle->setObjectName(QStringLiteral("conversationCenter"));
        auto *middleLayout = new QVBoxLayout(middle); middleLayout_ = middleLayout;
        middleLayout->setContentsMargins(0, 0, 0, 0);
        tabs_ = new ConversationTabs(middle); tabs_->setObjectName(QStringLiteral("conversationTabs")); tabs_->setExpanding(false); tabs_->setUsesScrollButtons(true); tabs_->installEventFilter(this); tabs_->hide();
        middleLayout->addWidget(tabs_);
        connect(tabs_, &QTabBar::currentChanged, this, [this](int index) { void *key = tabKey(index); if (conversations_.contains(key)) select(key); syncTabSelection(); });
        tabs_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabs_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
            const int index = point.x() < 0 ? tabs_->currentIndex() : tabs_->tabAt(point);
            showConversationPopup(tabKey(index), tabs_->mapToGlobal(point.x() < 0 ? tabs_->rect().center() : point));
        });
        topic_ = new TopicEntry(middle);
        topic_->setObjectName(QStringLiteral("topicDisplay"));
        topic_->setAccessibleName(QStringLiteral("Channel topic"));
        topic_->setReadOnly(true);
        topic_->setPlaceholderText(QStringLiteral("No topic set"));
        topic_->setMinimumWidth(0);
        topic_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(topic_, &QLineEdit::returnPressed, this, [this]() {
            const auto target = captureTarget();
            QtSessionInfo info{};
            if (!alive(target) || !qt_core_session_info(target.key, &info) || !info.connected || !info.channel) return;
            const QString text = topic_->text();
            if (text.contains(QLatin1Char('\r')) || text.contains(QLatin1Char('\n'))) return;
            const QByteArray bytes = text.toUtf8();
            qt_core_set_topic(target.key, bytes.constData());
            if (alive(target) && active_ == target.key) {
                setTopicText(topic_, conversations_[target.key].topic);
            }
            statusBar()->showMessage(QStringLiteral("Topic change requested; waiting for server confirmation"), 5000);
        });
        chat_ = new ChatTranscript(middle);
        chat_->followNewText = [this]() { return autoScroll_ && autoScroll_->isChecked(); };
        chat_->setObjectName(QStringLiteral("chat"));
        chat_->setReadOnly(true);
        chat_->setPalette(ircDisplayPalette(chat_->palette()));
        chat_->setOpenLinks(false);
        chat_->setOpenExternalLinks(false);
        chat_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        chat_->setUndoRedoEnabled(false);
        emptyDocument_ = new QTextDocument(this);
        chat_->setDocument(emptyDocument_);
        connect(chat_, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
            const QString scheme = url.scheme().toLower();
            if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https") ||
                scheme == QStringLiteral("irc") || scheme == QStringLiteral("ircs"))
                QDesktopServices::openUrl(url);
        });
        chat_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(chat_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
            const QString link = chat_->anchorAt(point);
            std::unique_ptr<QMenu> menu(chat_->createStandardContextMenu(point));
            if (!link.isEmpty()) {
                menu->addSeparator();
                auto *open = menu->addAction(QStringLiteral("Open Link"));
                const QUrl url = safeHistoryUrl(link); open->setEnabled(url.isValid() && !url.isEmpty());
                connect(open, &QAction::triggered, menu.get(), [url]() { QDesktopServices::openUrl(url); });
                connect(menu->addAction(QStringLiteral("Copy Link")), &QAction::triggered, menu.get(), [link]() { QApplication::clipboard()->setText(link); });
                populateCustomPopup(menu.get(), QStringLiteral("$URL"), QT_CONFIG_URL_HANDLERS, popupContext(captureTarget(), {link}));
            }
            menu->exec(chat_->viewport()->mapToGlobal(point));
        });
        middleLayout->addWidget(topic_);
        channelModes_ = buildModeRow(middle, [this]() { return captureTarget(); }, modeButtons_, channelKey_, channelLimit_);
        channelModes_->setObjectName(QStringLiteral("channelModes")); middleLayout->addWidget(channelModes_);
        middleLayout->addWidget(chat_, 1);
        buildSearch(middle, middleLayout);
        dialogButtons_ = new QScrollArea(middle); dialogButtons_->setWidgetResizable(true); dialogButtons_->setMaximumHeight(90);
        middleLayout->addWidget(dialogButtons_);
        input_ = new ChatInput(middle); input_->setDefaultShortcutsEnabled(false);
        input_->setObjectName(QStringLiteral("messageInput"));
        input_->wordReplacement = [](const QString &word) {
            const QByteArray bytes = word.toUtf8(); const char *replacement = qt_core_replace_word(bytes.constData());
            return replacement ? QString::fromUtf8(replacement) : word;
        };
        input_->completionEquivalent = [this](const QString &left, const QString &right) { return qt_core_completion_compare(active_, left.toUtf8().constData(), right.toUtf8().constData()) == 0; };
        input_->orderedNicks = [this]() { return completionWords(active_, 2); };
        input_->completionMatches = [this](const QStringList &matches) { printCompletions(captureTarget(), matches); };
        input_->completionWords = [this](bool commands) { return completionWords(active_, commands); };
        input_->multilinePaste = [this](const QString &text) { previewPaste(text); };
        input_->setPlaceholderText(QStringLiteral("Message or /command"));
        auto *inputRow = new QHBoxLayout;
        inputNickname_ = new QPushButton(middle); inputNickname_->setObjectName(QStringLiteral("inputNickname")); inputNickname_->setFlat(true); inputNickname_->setFocusPolicy(Qt::NoFocus);
        connect(inputNickname_, &QPushButton::clicked, this, [this]() { changeNickname(captureTarget()); });
        inputRow->addWidget(inputNickname_); inputRow->addWidget(input_, 1); middleLayout->addLayout(inputRow);
        rightDock_ = new QSplitter(Qt::Vertical, split); rightDock_->setObjectName(QStringLiteral("rightDock")); rightDock_->setMaximumWidth(400);
        usersPanel_ = new QWidget(rightDock_);
        auto *usersLayout = new QVBoxLayout(usersPanel_);
        usersLayout->setContentsMargins(0, 0, 0, 0);
        userFilter_ = new QLineEdit(usersPanel_);
        userFilter_->setObjectName(QStringLiteral("userFilter"));
        userFilter_->setPlaceholderText(QStringLiteral("Filter users"));
        userCount_ = new QLabel(usersPanel_);
        userCount_->setObjectName(QStringLiteral("userCount"));
        nicks_ = new NickList(usersPanel_);
        usersLayout->addWidget(userFilter_); usersLayout->addWidget(userCount_); usersLayout->addWidget(nicks_, 1);
        connect(userFilter_, &QLineEdit::textChanged, this, [this](const QString &text) {
            nicks_->setFilterText(text); updateUserCount();
        });
        nicks_->activate = [this](const QString &nick) { activateUser(captureTarget(), nick); };
        nicks_->action = [this](const QString &nick, bool whois) {
            submit((whois ? QStringLiteral("/whois ") : QStringLiteral("/query ")) + nick);
        };
        nicks_->decoratePopup = [this](QMenu *menu, const QStringList &targets, const std::shared_ptr<NickListState> &state) {
            populateCustomPopup(menu, QStringLiteral("$NICK"), QT_CONFIG_POPUP, popupContext(captureTarget(), targets, state, true));
        };
        nicks_->userAction = [this](NickAction action, const QStringList &nicks) { handleUserAction(action, nicks); };
        userButtons_ = new QScrollArea(usersPanel_); userButtons_->setWidgetResizable(true); userButtons_->setMaximumHeight(180);
        usersLayout->addWidget(userButtons_);
        rebuildButtons();
        usersPanel_->setMinimumWidth(130);
        usersPanel_->setMaximumWidth(400);
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setStretchFactor(2, 0);
        split->setSizes({175, 720, 170});
        paneDrag_ = new PaneDrag(center);
        paneDrag_->source(channels_->viewport(), PaneDrag::Switcher);
        paneDrag_->source(tabs_, PaneDrag::Switcher);
        paneDrag_->source(nicks_->viewport(), PaneDrag::Users);
        paneDrag_->horizontalTabs = [this]() { return switcherLayout_ != 2; };
        paneDrag_->move = [this](PaneDrag::Pane pane, int position) { return movePane(pane, position); };
        layout->addWidget(split);
        setCentralWidget(center);
        initializeTransfers(this, [guard = QPointer<MainWindow>(this)]() { if (guard) guard->promptTransferSend(); },
            [guard = QPointer<MainWindow>(this)]() { if (guard) guard->promptDirectChat(); });
        initializeIrcTools(this);
        initializeUrlHistory(this);
        setUrlPopupDecorator([guard = QPointer<MainWindow>(this)](QMenu *menu, const QString &url) {
            if (guard) populateCustomPopup(menu, QStringLiteral("$URL"), QT_CONFIG_URL_HANDLERS, guard->popupContext(guard->captureTarget(), {url}));
        });
        initializeFriends(this);
        desktop_ = new DesktopIntegration(this, [this](void *session) {
            if (conversations_.contains(session)) select(session);
        }, [this]() { requestQuit(); });

        desktop_->hiddenChanged = [this](bool hidden) { if (trayAway_) setAllServersAway(hidden); };
        desktop_->awayRequested = [this](bool away) { setAllServersAway(away); };
        desktop_->awayState = [this]() {
            bool away = false, back = false;
            for (auto it = conversations_.cbegin(); it != conversations_.cend(); ++it) {
                QtSessionInfo info{}; if (!qt_core_session_info(it.key(), &info) || !info.connected) continue;
                if (info.away) away = true; else back = true;
            }
            return away && back ? DesktopAwayState::Mixed : away ? DesktopAwayState::Away : back ? DesktopAwayState::Back : DesktopAwayState::Disconnected;
        };

        keyBindings_ = new InputBindings(input_, QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("keybindings.conf")), [this](const InputBinding &binding) { return runInputBinding(binding); }, []() { return QString::fromUtf8(qt_core_command_prefix()); });
        keyBindings_->keypress = [this](const QKeyEvent &event) { return pluginKeypress(captureTarget(), event); };
        keyBindings_->applied = [this]() { for (auto *view : detached_) if (view->bindings) view->bindings->reload(); };
        buildActions();
        restorePreferences();
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { savePreferences(); });

        connect(input_, &QLineEdit::returnPressed, this, [this]() {
            const QString line = input_->text();
            if (line.isEmpty() || !active_) return;
            conversations_[active_].history->record(line);
            input_->clear();
            const QByteArray bytes = line.toUtf8();
            qt_core_submit(active_, bytes.constData());
        });
        connect(channels_, &QTreeWidget::itemSelectionChanged, this, [this]() {
            auto *item = channels_->currentItem();
            if (!item) return;
            void *key = nullptr;
            for (auto it = items_.cbegin(); it != items_.cend(); ++it) {
                if (it.value() == item) { key = it.key(); break; }
            }
            if (key) select(key);
        });
        statusBar()->showMessage(QStringLiteral(
            "Qt 6 prototype — use ZoiteChat → Network List, then Server → Join a Channel"));
        connection_ = new QLabel(this);
        connection_->setObjectName(QStringLiteral("connectionStatus"));
        connection_->setTextFormat(Qt::PlainText);
        statusBar()->addPermanentWidget(connection_);
        connectionProgress_ = new QProgressBar(this);
        connectionProgress_->setObjectName(QStringLiteral("connectionProgress"));
        connectionProgress_->setRange(0, 0); connectionProgress_->setMaximumWidth(90);
        connectionProgress_->setTextVisible(false); connectionProgress_->hide();
        statusBar()->addPermanentWidget(connectionProgress_);
        lagStatus_ = new QLabel(this); lagStatus_->setObjectName(QStringLiteral("lagStatus"));
        lagStatus_->setTextFormat(Qt::PlainText); statusBar()->addPermanentWidget(lagStatus_);
        queueStatus_ = new QLabel(this); queueStatus_->setObjectName(QStringLiteral("queueStatus"));
        queueStatus_->setTextFormat(Qt::PlainText); statusBar()->addPermanentWidget(queueStatus_);
        lagMeter_ = makeMeter(this, QStringLiteral("lagMeter"), 1000);
        queueMeter_ = makeMeter(this, QStringLiteral("queueMeter"), 1024);
        statusBar()->addPermanentWidget(lagMeter_); statusBar()->addPermanentWidget(queueMeter_);
        desktop_->decoratePopup = [this](QMenu *menu) {
            const auto target = captureTarget();
            if (alive(target)) populateCustomPopup(menu, QStringLiteral("$TRAY"), -1, popupContext(target, {conversations_[target.key].name}));
        };
        auto *statusTimer = new QTimer(this);
        connect(statusTimer, &QTimer::timeout, this, [this]() { updateStatus(); });
        statusTimer->start(1000);
        // On Linux Qt's GLib event dispatcher runs the default GMainContext
        // directly, including ZoiteChat's sockets, timers and plugin sources.
        // Only keep the old bounded timer fallback for Qt builds that do not
        // ship the GLib dispatcher (notably Windows).
        auto *dispatcher = QAbstractEventDispatcher::instance();
        if (dispatcher && dispatcher->inherits("QEventDispatcherGlib")) {
            qInfo("ZoiteChat Qt: GLib-backed Qt event loop active");
        } else {
            qWarning("ZoiteChat Qt: non-GLib event dispatcher; using fallback");
            auto *pump = new QTimer(this);
            connect(pump, &QTimer::timeout, this, []() {
                for (int i = 0; i < 32 && g_main_context_pending(nullptr); ++i)
                    g_main_context_iteration(nullptr, FALSE);
            });
            pump->start(15);
        }
    }

    void addSession(void *key, const QString &name, bool focus)
    {
        if (conversations_.contains(key)) return;
        Conversation conversation;
        conversation.name = name;
        conversation.document = new QTextDocument(this);
        conversation.document->setDefaultFont(chat_->font());
        conversation.document->setMaximumBlockCount(scrollback_);
        conversations_.insert(key, conversation);
        auto *item = new QTreeWidgetItem(channels_);
        item->setData(0, Qt::UserRole, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(key)));
        item->setFont(0, channels_->font());
        items_.insert(key, item);
        rebuildSwitcher(switcherItems()); applySwitcherAppearance(); refreshSwitcherLabels();
        if (!active_ || newConversationFocus_ == 1 || (newConversationFocus_ == 2 && focus)) {
            channels_->setCurrentItem(item);
            select(key);
        }
        updateStatus();
    }

    void removeSession(void *key)
    {
        if (!conversations_.contains(key)) return;
        attachConversation(key, false);
        desktop_->forget(key);
        const bool wasActive = active_ == key;
        const int oldRow = switcherItems().indexOf(items_.value(key));
        const QSignalBlocker blocker(channels_);
        // A QTextBrowser must not retain a document after that session dies.
        if (active_ == key)
            chat_->setDocument(emptyDocument_);
        if (active_ == key) chat_->setExtraSelections({});
        if (active_ == key) {
            input_->setConversation(nullptr, {});
            nicks_->setState(nullptr);
        }
        auto it = items_.find(key);
        if (it != items_.end()) {
            QTreeWidgetItem *item = it.value();
            auto rows = switcherItems(); rows.removeOne(item);
            rebuildSwitcher(rows);
            items_.erase(it);
            delete item;
        }
        QTextDocument *oldDocument = conversations_.value(key).document;
        conversations_.remove(key);
        delete oldDocument;
        if (wasActive) {
            active_ = nullptr;
            const auto rows = switcherItems();
            if (!rows.isEmpty())
                select(keyForItem(rows[qBound(0, oldRow, int(rows.size()) - 1)]));
            else { chat_->clear(); nicks_->clear(); topic_->clear(); input_->clear(); updateTitle(); }
        }
        updateStatus();
        updateUserCount();
        updateTrayUnread();
    }

    void renameSession(void *key, const QString &name)
    {
        if (!conversations_.contains(key)) return;
        conversations_[key].name = name;
        rebuildSwitcher(switcherItems()); applySwitcherAppearance(); refreshSwitcherLabels();
        if (active_ == key) updateTitle();
    }

    void select(void *key)
    {
        if (!conversations_.contains(key)) return;
        if (detached_.contains(key)) {
            auto *view = detached_.value(key); view->showNormal(); view->raise(); view->activateWindow(); view->input->setFocus(); return;
        }
        if (active_ == key) {
            qt_core_activate(key);
            activity(key, 0);
            desktop_->markRead(key);
            updateTrayUnread();
            return;
        }
        if (active_ && conversations_.contains(active_)) {
            if (!conversations_[active_].document->isEmpty())
                placeMarker(conversations_[active_], chat_->cursorForPosition(QPoint(0, chat_->viewport()->height() - 1)).block());
            conversations_[active_].draft = input_->text();
            conversations_[active_].inputCursor = input_->cursorPosition();
            conversations_[active_].scrollPosition = chat_->savedScrollPosition();
        }
        active_ = key;
        qt_core_activate(key);
        auto &conversation = conversations_[key];
        chat_->setDocument(conversation.document);
        updateMarker();
        QScrollBar *scroll = chat_->verticalScrollBar();
        scroll->setValue(conversation.scrollPosition < 0 ? scroll->maximum()
                         : qMin(conversation.scrollPosition, scroll->maximum()));
        setTopicText(topic_, conversation.topic);
        input_->setText(conversation.draft);
        input_->setCursorPosition(conversation.inputCursor);
        input_->setConversation(conversation.history.get(), conversation.users->users.keys());
        userFilter_->clear();
        refreshUsers();
        updateTitle();
        auto *item = items_.value(key, nullptr);
        if (item) for (auto *parent = item->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        if (item && channels_->currentItem() != item)
            channels_->setCurrentItem(item);
        if (item) item->setForeground(0, channels_->palette().text());
        conversation.activity = 0;
        desktop_->markRead(key);
        updateTrayUnread();
        if (item) { QFont font = item->font(0); font.setBold(false); item->setFont(0, font); }
        searchResult_->clear();
        updateStatus(); syncTabSelection();
        input_->setFocus();
    }

    void print(void *key, const QString &message, std::time_t timestamp)
    {
        if (!conversations_.contains(key)) return;
        auto *display = detached_.contains(key) ? detached_.value(key)->chat : chat_;
        QScrollBar *scroll = display->verticalScrollBar();
        const bool visible = key == active_ || detached_.contains(key);
        const bool wasAtBottom = visible &&
            (scroll->maximum() - scroll->value() <= 20);
        const int oldPosition = scroll->value();
        appendIrcMessage(conversations_[key].document, message, timestamp, chat_->palette(), timestamps_, timestampFormat_, indentWidth_);
        if (wasAtBottom && autoScroll_->isChecked()) scroll->setValue(scroll->maximum());
        else if (visible) scroll->setValue(oldPosition);
        if (key == active_ || detached_.contains(key)) updateMarker();
    }

    void lastlog(void *source, void *destination, const QString &query, int flags)
    {
        if (!conversations_.contains(source) || !conversations_.contains(destination)) return;
        auto *output = conversations_[destination].document;
        const auto result = filterChatBuffer(conversations_[source].document, output, query, flags, chat_->palette());
        if (!result.error.isEmpty()) appendIrcMessage(output, result.error, 0, chat_->palette(), false);
        else if (!result.lines) appendIrcMessage(output, QStringLiteral("No matching lines."), 0, chat_->palette(), false);
        select(destination);
        statusBar()->showMessage(result.error.isEmpty() ? QStringLiteral("%1 matching lines").arg(result.lines) : result.error, 6000);
    }

    void clear(void *key, int lines = 0)
    {
        if (!conversations_.contains(key)) return;
        auto &conversation = conversations_[key];
        auto *document = conversation.document;
        const int count = document->blockCount();
        const qint64 amount = lines < 0 ? -qint64(lines) : qint64(lines);
        if (lines && amount < count) {
            const int marker = validMarker(conversation) ? conversation.marker.blockNumber() : -1;
            if (marker >= 0 && (lines > 0 ? marker < amount : marker >= count - amount)) {
                conversation.marker = QTextBlock(); conversation.markerId = 0;
            }
            QTextCursor remove(document);
            if (lines > 0) {
                remove.setPosition(0); remove.setPosition(document->findBlockByNumber(int(amount)).position(), QTextCursor::KeepAnchor);
            } else {
                const auto lastKept = document->findBlockByNumber(count - int(amount) - 1);
                remove.setPosition(lastKept.position() + lastKept.length() - 1);
                remove.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            }
            remove.removeSelectedText(); conversation.scrollPosition = -1;
            if (marker >= 0 && conversation.markerId) {
                conversation.marker = document->findBlockByNumber(lines > 0 ? marker - int(amount) : marker);
                conversation.marker.setUserData(new MarkerTag(conversation.markerId));
            }
            if (key == active_ || detached_.contains(key)) updateMarker();
            return;
        }
        document->clear();
        conversations_[key].scrollPosition = -1;
        conversations_[key].marker = QTextBlock();
        conversations_[key].markerId = 0;
        if (key == active_ || detached_.contains(key)) updateMarker();
    }

    QString displayTopic(const QString &raw) const
    {
        if (!stripTopics_) return raw;
        QString plain;
        for (const auto &run : parseIrcText(raw, topic_->palette())) plain += run.text;
        return plain;
    }

    void setTopicText(TopicEntry *entry, const QString &raw)
    {
        const QString displayed = displayTopic(raw);
        if (entry->text() != displayed) {
            entry->setText(displayed);
            entry->setCursorPosition(0);
        }
        entry->setToolTip(Qt::convertFromPlainText(plainIrcText(raw.toUtf8().constData(), false)));
    }

    void topic(void *key, const QString &topic)
    {
        if (!conversations_.contains(key)) return;
        conversations_[key].topic = topic;
        if (detached_.contains(key)) setTopicText(detached_[key]->topic, topic);
        if (key == active_) {
            setTopicText(topic_, topic);
        }
    }

    void addUser(void *key, const QString &nick, const QString &prefix, bool select)
    {
        if (!conversations_.contains(key)) return;
        auto &users = *conversations_[key].users;
        QtSessionInfo info{}; if (qt_core_session_info(key, &info)) users.prefixes = QString::fromUtf8(info.nick_prefixes ? info.nick_prefixes : "");
        if (!users.users.contains(nick)) users.insertionOrder.prepend(nick);
        users.compare = [key](const QString &a, const QString &b) { return qt_core_completion_compare(key, a.toUtf8().constData(), b.toUtf8().constData()); };
        users.users[nick] = prefix + nick;
        if (select) users.selected.insert(nick);
        if (key == active_) scheduleUsersRefresh();
    }

    void removeUser(void *key, const QString &nick)
    {
        if (!conversations_.contains(key)) return;
        conversations_[key].users->users.remove(nick);
        conversations_[key].users->insertionOrder.removeAll(nick);
        conversations_[key].users->selected.remove(nick);
        conversations_[key].users->details.remove(nick);
        if (key == active_) scheduleUsersRefresh();
    }

    void setUserDetails(void *key, const QString &nick, const NickDetails &details)
    {
        if (!conversations_.contains(key)) return;
        conversations_[key].users->details[nick] = details;
        if (key == active_) scheduleUsersRefresh();
    }
    bool trayAllowed(void *key) const
    {
        if (!conversations_.contains(key) || conversations_[key].activity < 2) return false;
        QtSessionInfo info{}; qt_core_session_info(key, &info); QtAlertPolicy policy{};
        return qt_core_alert_policy(key, conversations_[key].activity == 3, info.query, &policy) && policy.tray && (!policy.omit_focused || sessionWindowStatus(key) != 1);
    }
    void updateTrayUnread()
    {
        bool unread = false;
        for (auto it = conversations_.cbegin(); it != conversations_.cend(); ++it) unread |= it->activity > 0 && trayAllowed(it.key());
        desktop_->setUnread(unread);
    }
    void displayConversationOptions(void *source = nullptr)
    {
        const auto target = source && conversations_.contains(source) ? Target{source, conversations_[source].history} : captureTarget(); if (!alive(target)) return;
        const auto membership = conversations_[target.key].users;
        showConversationOptions(target.key, this, [guard = QPointer<MainWindow>(this), target, membership]() {
            return guard && guard->alive(target) && guard->conversations_[target.key].users == membership;
        }, [guard = QPointer<MainWindow>(this)]() { if (guard) guard->updateTrayUnread(); });
    }
    void messageAlert(void *key, const QString &text, bool mention, bool privateMessage)
    {
        if (!conversations_.contains(key)) return;
        QtAlertPolicy policy{};
        if (!qt_core_alert_policy(key, mention, privateMessage, &policy)) return;
        const bool focused = policy.omit_focused && sessionWindowStatus(key) == 1;
        DesktopAlertPolicy desktopPolicy;
        desktopPolicy.notifications = policy.notify && (!policy.quiet || sessionWindowStatus(key) == 2);
        desktopPolicy.forceNotifications = policy.force_notify;
        desktopPolicy.ordinaryMessages = true; desktopPolicy.trayActivity = policy.tray; desktopPolicy.flash = false;
        if (policy.flash && !focused) attention(key);
        desktop_->alert(key, conversations_[key].name, text, mention, privateMessage, focused, desktopPolicy);
    }
    void guiControl(void *session, int action, int argument = 0)
    {
        // Numeric values are the C core's fe_gui_action enum.
        if (!conversations_.contains(session)) return;
        if (action == 6 && detached_.contains(session)) { detached_.value(session)->menuVisible->toggle(); return; }
        if (action != 7 && detached_.contains(session) && (action <= 2 || action == 5)) {
            auto *view = detached_.value(session);
            if (action == 0 || action == 5) view->showMinimized();
            else { view->showNormal(); view->raise(); view->activateWindow(); view->input->setFocus(); }
            return;
        }
        switch (action) {
        case 0: if (!desktop_->hideWindow()) showMinimized(); break;
        case 1: desktop_->showWindow(); break;
        case 2: select(session); desktop_->showWindow(); input_->setFocus(); break;
        case 5: showMinimized(); break;
        case 6: showMenu_->trigger(); break;
        case 7:
            if (argument == 2 || (argument == 0 && detached_.contains(session))) attachConversation(session);
            else detachConversation(session);
            break;
        case 8:
            if (keyBindings_) keyBindings_->reload();
            for (auto *view : detached_) if (view->bindings) view->bindings->reload();
            reloadBackground(); paletteChanged(); configureDesktop(); rebuildButtons(); updateStatus();
            if (customMenus_) customMenus_->schedule(true, true);
            break;
        default: break;
        }
    }
    void attachConversation(void *key, bool focus = true)
    {
        auto *view = detached_.take(key);
        if (!view) return;
        if (conversations_.contains(key)) {
            auto &state = conversations_[key];
            state.draft = view->input->text(); state.inputCursor = view->input->cursorPosition();
            state.scrollPosition = view->chat->savedScrollPosition();
        }
        settings_->setValue(view->geometryKey + QStringLiteral("/geometry"), view->saveGeometry());
        settings_->setValue(view->geometryKey + QStringLiteral("/splitter"), view->splitter->saveState());
        view->activated = {}; view->attaching = {};
        view->chat->setDocument(new QTextDocument(view->chat));
        view->hide(); view->deleteLater();
        if (focus && conversations_.contains(key)) {
            if (active_ == key) active_ = nullptr;
            select(key); showNormal(); raise();
        }
    }
    void detachConversation(void *key)
    {
        if (!conversations_.contains(key)) return;
        if (detached_.contains(key)) { select(key); return; }
        select(key);
        auto &state = conversations_[key];
        state.draft = input_->text(); state.inputCursor = input_->cursorPosition();
        state.scrollPosition = chat_->savedScrollPosition();
        const Target target{key, state.history};
        auto *view = new ConversationWindow;
        view->setParent(this, Qt::Window);
        view->setObjectName(QStringLiteral("detachedConversation"));
        view->resize(850, 600);
        QtSessionInfo placementInfo{}; qt_core_session_info(key, &placementInfo);
        QByteArray identity = QByteArray(placementInfo.host ? placementInfo.host : ""); identity.append(char(0)); identity += state.name.toUtf8();
        view->geometryKey = QStringLiteral("detached/") + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
        auto *center = new QWidget(view); auto *layout = new QVBoxLayout(center);
        view->topic = new TopicEntry(center); view->topic->setObjectName(QStringLiteral("detachedTopic")); layout->addWidget(view->topic);
        view->modeRow = buildModeRow(center, [target]() { return target; }, view->modeButtons, view->keyStatus, view->limitStatus);
        view->modeRow->setObjectName(QStringLiteral("detachedChannelModes")); layout->addWidget(view->modeRow);
        auto *split = new QSplitter(center); view->splitter = split;
        view->chat = new ChatTranscript(split); view->chat->setOpenLinks(false);
        view->chat->followNewText = [this]() { return autoScroll_ && autoScroll_->isChecked(); };
        view->chat->setPalette(chat_->palette()); view->chat->setDocument(state.document);
        view->userPanel = new QWidget(split); view->userPanel->setMinimumWidth(130); view->userPanel->setMaximumWidth(400); auto *userLayout = new QVBoxLayout(view->userPanel); userLayout->setContentsMargins(0, 0, 0, 0);
        view->userFilter = new QLineEdit(view->userPanel); view->userFilter->setObjectName(QStringLiteral("userFilter")); view->userFilter->setPlaceholderText(QStringLiteral("Filter users"));
        view->userCount = new QLabel(view->userPanel); view->userCount->setObjectName(QStringLiteral("userCount"));
        userLayout->addWidget(view->userFilter); userLayout->addWidget(view->userCount);
        view->users = new NickList(view->userPanel); view->users->setState(state.users); userLayout->addWidget(view->users, 1);
        connect(view->userFilter, &QLineEdit::textChanged, view, [this, view, target](const QString &text) { if (alive(target)) { view->users->setFilterText(text); refreshUserCount(target.key, view->users, view->userCount); } });
        view->lagStatus = new QLabel(view); view->lagStatus->setObjectName(QStringLiteral("lagStatus")); view->lagStatus->setTextFormat(Qt::PlainText);
        view->queueStatus = new QLabel(view); view->queueStatus->setObjectName(QStringLiteral("queueStatus")); view->queueStatus->setTextFormat(Qt::PlainText);
        view->lagMeter = makeMeter(view, QStringLiteral("lagMeter"), 1000); view->queueMeter = makeMeter(view, QStringLiteral("queueMeter"), 1024);
        view->statusBar()->addPermanentWidget(view->lagStatus); view->statusBar()->addPermanentWidget(view->queueStatus);
        view->statusBar()->addPermanentWidget(view->lagMeter); view->statusBar()->addPermanentWidget(view->queueMeter);
        view->userButtons = new QScrollArea(view->userPanel); view->userButtons->setWidgetResizable(true); view->userButtons->setMaximumHeight(180); userLayout->addWidget(view->userButtons);
        layout->addWidget(split, 1);
        view->dialogButtons = new QScrollArea(center); view->dialogButtons->setWidgetResizable(true); view->dialogButtons->setMaximumHeight(90); layout->addWidget(view->dialogButtons);
        view->input = new ChatInput(center); view->input->setDefaultShortcutsEnabled(false); view->input->setObjectName(QStringLiteral("detachedInput"));
        view->input->setConversation(state.history.get(), state.users->users.keys());
        view->input->setText(state.draft); view->input->setCursorPosition(state.inputCursor);
        view->input->wordReplacement = input_->wordReplacement;
        view->input->completionEquivalent = [this, target](const QString &left, const QString &right) { return alive(target) && qt_core_completion_compare(target.key, left.toUtf8().constData(), right.toUtf8().constData()) == 0; };
        view->input->orderedNicks = [this, target]() { return alive(target) ? completionWords(target.key, 2) : QStringList(); };
        view->input->completionMatches = [this, target](const QStringList &matches) { printCompletions(target, matches); };
        view->input->completionWords = [this, target](bool commands) { return alive(target) ? completionWords(target.key, commands) : QStringList(); };
        view->input->multilinePaste = [this, target](const QString &text) { if (alive(target)) previewPaste(text, target.key); };
        view->bindings = new InputBindings(view->input, QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("keybindings.conf")),
            [this, target](const InputBinding &binding) { return alive(target) && runInputBinding(binding, target.key); }, []() { return QString::fromUtf8(qt_core_command_prefix()); });
        view->bindings->keypress = [this, target](const QKeyEvent &event) { return pluginKeypress(target, event); };
        view->users->userAction = [this, target](NickAction action, const QStringList &nicks) { if (alive(target)) handleUserAction(action, nicks, target.key); };
        view->users->decoratePopup = [this, target](QMenu *menu, const QStringList &nicks, const std::shared_ptr<NickListState> &membership) {
            if (alive(target)) populateCustomPopup(menu, QStringLiteral("$NICK"), QT_CONFIG_POPUP, popupContext(target, nicks, membership, true));
        };
        view->users->activate = [this, target](const QString &nick) { activateUser(target, nick); };
        view->users->action = [this, target](const QString &nick, bool whois) { submitTo(target, (whois ? QStringLiteral("/whois ") : QStringLiteral("/query ")) + nick); };
        auto *inputRow = new QHBoxLayout; view->nickname = new QPushButton(center); view->nickname->setObjectName(QStringLiteral("inputNickname")); view->nickname->setFlat(true); view->nickname->setFocusPolicy(Qt::NoFocus);
        connect(view->nickname, &QPushButton::clicked, view, [this, target]() { changeNickname(target); });
        inputRow->addWidget(view->nickname); inputRow->addWidget(view->input, 1); layout->addLayout(inputRow); view->setCentralWidget(center);
        auto *file = view->menuBar()->addMenu(QStringLiteral("&ZoiteChat"));
        file->setObjectName(QStringLiteral("detachedZoitechatMenu"));
        addAction(file, QStringLiteral("Network Li&st"), QStringLiteral("detachedNetworkList"), QKeySequence(QStringLiteral("Ctrl+S")), [view]() { showNetworkManager(view); });
        file->addSeparator();
        auto *newMenu = file->addMenu(QStringLiteral("&New"));
        addAction(newMenu, QStringLiteral("Server Tab"), QStringLiteral("detachedNewServerTab"), QKeySequence(QStringLiteral("Ctrl+T")), [this, target]() { if (alive(target)) qt_core_new_tab(target.key, true); });
        addAction(newMenu, QStringLiteral("Channel Tab"), QStringLiteral("detachedNewChannelTab"), {}, [this, target]() { if (alive(target)) qt_core_new_tab(target.key, false); });
        addAction(newMenu, QStringLiteral("Server Window"), QStringLiteral("detachedNewServerWindow"), QKeySequence(QStringLiteral("Ctrl+N")), [this, target]() { if (alive(target)) newConversationWindow(target.key, true); });
        addAction(newMenu, QStringLiteral("Channel Window"), QStringLiteral("detachedNewChannelWindow"), {}, [this, target]() { if (alive(target)) newConversationWindow(target.key, false); });
        file->addSeparator();
        auto *load = addAction(file, QStringLiteral("&Load Plugin or Script…"), QStringLiteral("detachedLoadPlugin"), {}, [this, target]() { if (alive(target)) openAddons(target, true); });
        load->setEnabled(qt_core_addons_enabled()); file->addSeparator();
        auto *attach = file->addAction(QStringLiteral("&Attach"));
        connect(attach, &QAction::triggered, this, [this, key]() { attachConversation(key); });
        addAction(file, QStringLiteral("&Close"), QStringLiteral("detachedClose"), QKeySequence(QStringLiteral("Ctrl+W")), [this, target]() { submitTo(target, QStringLiteral("/close")); });
        file->addSeparator();
        addAction(file, QStringLiteral("&Quit"), QStringLiteral("detachedQuit"), QKeySequence(QStringLiteral("Ctrl+Q")), [this]() { requestQuit(); });
        auto *viewMenu = view->menuBar()->addMenu(QStringLiteral("&View")); viewMenu->setObjectName(QStringLiteral("detachedViewMenu"));
        view->menuVisible = viewMenu->addAction(QStringLiteral("Menu Bar")); view->menuVisible->setCheckable(true); view->menuVisible->setChecked(true);
        view->menuVisible->setShortcut(QKeySequence(QStringLiteral("Ctrl+F9"))); view->addAction(view->menuVisible);
        connect(view->menuVisible, &QAction::toggled, view->menuBar(), &QWidget::setVisible);
        auto *topicVisible = viewMenu->addAction(QStringLiteral("Topic Bar")); topicVisible->setObjectName(QStringLiteral("detachedShowTopic")); topicVisible->setCheckable(true); topicVisible->setChecked(true);
        connect(topicVisible, &QAction::toggled, view->topic, &QWidget::setVisible);
        view->showModes = viewMenu->addAction(QStringLiteral("Channel Modes")); view->showModes->setCheckable(true); view->showModes->setChecked(showModes_->isChecked());
        connect(view->showModes, &QAction::toggled, view, [this, key]() { refreshDetachedControls(key); });
        view->showUserButtons = viewMenu->addAction(QStringLiteral("User List Buttons")); view->showUserButtons->setCheckable(true); view->showUserButtons->setChecked(showUserButtons_->isChecked());
        view->showDialogButtons = viewMenu->addAction(QStringLiteral("Dialog Buttons")); view->showDialogButtons->setCheckable(true); view->showDialogButtons->setChecked(showDialogButtons_->isChecked());
        for (auto *action : {view->showUserButtons, view->showDialogButtons}) connect(action, &QAction::toggled, view, [this, key]() { refreshDetachedControls(key); });
        auto *usersVisible = viewMenu->addAction(QStringLiteral("User List")); usersVisible->setCheckable(true); usersVisible->setChecked(true);
        connect(usersVisible, &QAction::toggled, view, [this, key](bool) { refreshDetachedControls(key); });
        view->users->setProperty("userListAction", QVariant::fromValue<QObject *>(usersVisible));
        usersVisible->setObjectName(QStringLiteral("detachedShowUsers"));
        bindNativeVisibility(view->menuVisible, "gui_hide_menu", true);
        bindNativeVisibility(topicVisible, "gui_topicbar");
        bindNativeVisibility(usersVisible, "gui_ulist_hide", true);
        bindNativeVisibility(view->showUserButtons, "gui_ulist_buttons");
        bindNativeVisibility(view->showModes, "gui_mode_buttons");
        viewMenu->removeAction(view->showModes); viewMenu->removeAction(view->showUserButtons); viewMenu->removeAction(view->showDialogButtons); viewMenu->removeAction(usersVisible);
        viewMenu->addAction(usersVisible); viewMenu->addAction(view->showUserButtons); viewMenu->addAction(view->showModes);
        view->showModes->setText(QStringLiteral("Mode Buttons"));
        addMeterMenu(viewMenu, true); viewMenu->addSeparator(); viewMenu->addAction(view->showDialogButtons);
        auto *serverMenu = view->menuBar()->addMenu(QStringLiteral("&Server"));
        addAction(serverMenu, QStringLiteral("Disconnect"), QStringLiteral("detachedDisconnect"), {}, [this, target]() { submitTo(target, QStringLiteral("/discon")); });
        addAction(serverMenu, QStringLiteral("Reconnect"), QStringLiteral("detachedReconnect"), {}, [this, target]() { submitTo(target, QStringLiteral("/reconnect")); });
        addAction(serverMenu, QStringLiteral("Join a Channel…"), QStringLiteral("detachedJoin"), {}, [this, target]() { promptCommandFor(target, QStringLiteral("Join Channel"), QStringLiteral("Channel:"), QStringLiteral("/join ")); });
        addAction(serverMenu, QStringLiteral("Channel List"), QStringLiteral("detachedChannelList"), {}, [this, target, view]() { if (alive(target)) qt_fe_channel_browser(qt_core_session_server(target.key), "", false); });
        auto *detachedAway = addAction(serverMenu, QStringLiteral("Marked Away"), QStringLiteral("detachedAway"), QKeySequence(QStringLiteral("Alt+A")), [this, target]() {
            QtSessionInfo info{};
            if (!alive(target) || !qt_core_session_info(target.key, &info) || !info.connected) return;
            refreshDetachedControls(target.key);
            submitTo(target, info.away ? QStringLiteral("/back") : QStringLiteral("/away"));
        });
        detachedAway->setCheckable(true);
        auto *usermenu = view->menuBar()->addMenu(QStringLiteral("&Usermenu")); usermenu->setObjectName(QStringLiteral("userMenu"));
        if (auto *settingsMenu = findChild<QMenu *>(QStringLiteral("settingsMenu"))) view->menuBar()->addAction(settingsMenu->menuAction());
        auto *tools = view->menuBar()->addMenu(QStringLiteral("&Window"));
        addAction(tools, QStringLiteral("Ban List"), QStringLiteral("detachedBanList"), {}, [this, target, view]() { if (alive(target)) showBanManager(target.key, view); });
        addAction(tools, QStringLiteral("Channel Settings…"), QStringLiteral("detachedChannelSettings"), {}, [this, target, view]() { if (alive(target)) showChannelSettings(target.key, view); });
        addAction(tools, QStringLiteral("Conversation Options…"), QStringLiteral("detachedConversationOptions"), {}, [this, target, view]() { if (alive(target)) displayConversationOptions(target.key); });
        addAction(tools, QStringLiteral("Character Chart"), QStringLiteral("detachedCharacterChart"), {}, [this, target, view]() { showCharacterChart(view, [this, target](const QString &text) { if (!alive(target) || !inputFor(target.key)) return false; inputFor(target.key)->insert(text); return true; }); });
        addAction(tools, QStringLiteral("Direct Chat"), QStringLiteral("detachedDirectChat"), {}, []() { showDirectChats(); });
        addAction(tools, QStringLiteral("File Transfers"), QStringLiteral("detachedTransfers"), {}, []() { showTransfers(); });
        addAction(tools, QStringLiteral("Friends List"), QStringLiteral("detachedFriends"), {}, []() { showFriendsList(); });
        addAction(tools, QStringLiteral("Ignore List"), QStringLiteral("detachedIgnores"), {}, []() { showIgnoreManager(); });
        addAction(tools, QStringLiteral("Plugins and Scripts"), QStringLiteral("detachedAddons"), {}, [this, target]() { openAddons(target, false); });
        addAction(tools, QStringLiteral("Raw Log"), QStringLiteral("detachedRawLog"), {}, [this, target]() { if (alive(target)) showRawLog(qt_core_session_server(target.key)); });
        addAction(tools, QStringLiteral("URL Grabber"), QStringLiteral("detachedUrls"), {}, []() { showUrlHistory(); });
        tools->addSeparator();
        addAction(tools, QStringLiteral("Reset Marker Line"), QStringLiteral("detachedResetMarker"), QKeySequence(QStringLiteral("Ctrl+M")), [this, target]() { if (alive(target)) { placeMarker(conversations_[target.key], conversations_[target.key].document->lastBlock()); updateMarker(); } });
        addAction(tools, QStringLiteral("Move to Marker Line"), QStringLiteral("detachedMoveToMarker"), QKeySequence(QStringLiteral("Ctrl+Shift+M")), [this, target, view]() { if (alive(target) && validMarker(conversations_[target.key])) { view->chat->setTextCursor(QTextCursor(conversations_[target.key].marker)); view->chat->ensureCursorVisible(); } });
        addAction(tools, QStringLiteral("Copy Selection"), QStringLiteral("detachedCopy"), QKeySequence(QStringLiteral("Ctrl+Shift+C")), [view]() { view->chat->copy(); });
        addAction(tools, QStringLiteral("Clear Text"), QStringLiteral("detachedClear"), QKeySequence(QStringLiteral("Ctrl+L")), [this, target]() { if (alive(target)) clear(target.key); });
        addAction(tools, QStringLiteral("Save Text…"), QStringLiteral("detachedSave"), {}, [this, target, view]() { saveConversation(target, view); });
        auto *searchBar = new QWidget(center); auto *searchLayout = new QHBoxLayout(searchBar); searchLayout->setContentsMargins(0, 0, 0, 0);
        view->search = new QLineEdit(searchBar); view->search->setObjectName(QStringLiteral("detachedSearch")); searchLayout->addWidget(view->search, 1);
        view->matchCase = new QCheckBox(QStringLiteral("Case"), searchBar); view->regexSearch = new QCheckBox(QStringLiteral("Regex"), searchBar);
        view->searchStatus = new QLabel(searchBar); view->searchStatus->setTextFormat(Qt::PlainText);
        searchLayout->addWidget(view->matchCase); searchLayout->addWidget(view->regexSearch); searchLayout->addWidget(view->searchStatus);
        auto find = [view](bool backwards) { if (view->search->text().isEmpty()) { view->search->setFocus(); return; } view->searchStatus->setText(searchTranscript(view->chat, view->search->text(), backwards, view->matchCase->isChecked(), view->regexSearch->isChecked())); };
        auto *previous = new QPushButton(QStringLiteral("Previous"), searchBar), *next = new QPushButton(QStringLiteral("Next"), searchBar), *hideSearch = new QPushButton(QStringLiteral("Close"), searchBar);
        searchLayout->addWidget(previous); searchLayout->addWidget(next); searchLayout->addWidget(hideSearch);
        layout->insertWidget(3, searchBar); searchBar->hide();
        connect(previous, &QPushButton::clicked, view, [find]() { find(true); }); connect(next, &QPushButton::clicked, view, [find]() { find(false); });
        connect(view->search, &QLineEdit::returnPressed, view, [find]() { find(false); }); connect(hideSearch, &QPushButton::clicked, searchBar, &QWidget::hide);
        auto *searchMenu = tools->addMenu(QStringLiteral("Search"));
        addAction(searchMenu, QStringLiteral("Search Text…"), QStringLiteral("detachedFind"), QKeySequence(QStringLiteral("Ctrl+F")), [view, searchBar]() { searchBar->show(); view->search->setFocus(); view->search->selectAll(); });
        addAction(searchMenu, QStringLiteral("Search Next"), QStringLiteral("detachedFindNext"), QKeySequence(QStringLiteral("Ctrl+G")), [find]() { find(false); });
        addAction(searchMenu, QStringLiteral("Search Previous"), QStringLiteral("detachedFindPrevious"), QKeySequence(QStringLiteral("Ctrl+Shift+G")), [find]() { find(true); });
        auto *help = view->menuBar()->addMenu(QStringLiteral("&Help"));
        if (auto *about = findChild<QAction *>(QStringLiteral("about"))) help->addAction(about);
        view->menus = new CustomMenus(view->menuBar(), usermenu, view,
            [this, target](const QString &command) { if (alive(target)) qt_core_custom_run(target.key, command.toUtf8().constData()); restoreEngineFocus(); },
            [this, target](void *id, const QString &command, bool checked) { if (alive(target)) qt_core_menu_activate(target.key, id, command.toUtf8().constData(), checked); restoreEngineFocus(); });
        view->activated = [this, target]() { if (alive(target)) { qt_core_activate(target.key); activity(target.key, 0); desktop_->markRead(target.key); updateTrayUnread(); } };
        view->attaching = [this, key]() { attachConversation(key); };
        connect(view->input, &QLineEdit::returnPressed, this, [this, target, view]() {
            if (!alive(target)) return;
            const QString line = view->input->text(); if (line.isEmpty()) return;
            conversations_[target.key].history->record(line); view->input->clear();
            qt_core_submit(target.key, line.toUtf8().constData());
        });
        connect(view->topic, &QLineEdit::returnPressed, this, [this, target, view]() {
            QtSessionInfo info{};
            if (alive(target) && qt_core_session_info(target.key, &info) && info.connected && info.channel)
                qt_core_set_topic(target.key, view->topic->text().toUtf8().constData());
            if (alive(target)) setTopicText(view->topic, conversations_[target.key].topic);
        });
        connect(view->chat, &QTextBrowser::anchorClicked, view, [](const QUrl &url) {
            if (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("irc") || url.scheme() == QStringLiteral("ircs")) QDesktopServices::openUrl(url);
        });
        view->chat->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view->chat, &QWidget::customContextMenuRequested, view, [this, target, view](const QPoint &point) {
            const QString link = view->chat->anchorAt(point);
            std::unique_ptr<QMenu> menu(view->chat->createStandardContextMenu(point));
            if (!link.isEmpty() && alive(target)) {
                menu->addSeparator(); const QUrl url = safeHistoryUrl(link);
                auto *open = menu->addAction(QStringLiteral("Open Link")); open->setEnabled(url.isValid() && !url.isEmpty());
                connect(open, &QAction::triggered, menu.get(), [url]() { QDesktopServices::openUrl(url); });
                connect(menu->addAction(QStringLiteral("Copy Link")), &QAction::triggered, menu.get(), [link]() { QApplication::clipboard()->setText(link); });
                populateCustomPopup(menu.get(), QStringLiteral("$URL"), QT_CONFIG_URL_HANDLERS, popupContext(target, {link}));
            }
            menu->exec(view->chat->viewport()->mapToGlobal(point));
        });
        auto *timer = new QTimer(view);
        connect(timer, &QTimer::timeout, this, [this, target, view]() {
            if (!alive(target)) return;
            const auto &state = conversations_[target.key]; QtSessionInfo info{};
            qt_core_session_info(target.key, &info);
            view->setWindowTitle(conversationTitle(target.key));
            if (!view->topic->hasFocus()) setTopicText(view->topic, state.topic);
            view->topic->setReadOnly(!(info.connected && info.channel));
            refreshNickname(target.key, view->nickname);
            view->users->setModerationEnabled(info.connected && info.channel);
            auto *listAction = qobject_cast<QAction *>(view->users->property("userListAction").value<QObject *>());
            view->userPanel->setVisible(info.channel && (!listAction || listAction->isChecked())); view->users->setState(state.users);
            refreshDetachedControls(target.key);
            view->input->setNicks(state.users->users.keys());
        }); timer->start(250);
        detached_.insert(key, view);
        rebuildButtons(); refreshDetachedControls(key);
        connect(view->users, &QListWidget::itemSelectionChanged, view, [this, key]() { refreshDetachedControls(key); });
        // Keep a single editable view of this conversation.
        active_ = nullptr; chat_->setDocument(emptyDocument_); input_->clear(); input_->setConversation(nullptr, {}); nicks_->setState(nullptr); topic_->clear(); updateStatus(); updateTitle();
        for (auto *item : items_) if (keyForItem(item) != key && !detached_.contains(keyForItem(item))) { select(keyForItem(item)); break; }
        setTopicText(view->topic, state.topic); view->setWindowTitle(state.name + QStringLiteral(" — ZoiteChat"));
        view->restoreGeometry(settings_->value(view->geometryKey + QStringLiteral("/geometry")).toByteArray());
        view->splitter->restoreState(settings_->value(view->geometryKey + QStringLiteral("/splitter")).toByteArray());
        applyDisplay(); view->show();
        auto *scroll = view->chat->verticalScrollBar();
        scroll->setValue(state.scrollPosition < 0 ? scroll->maximum() : qMin(state.scrollPosition, scroll->maximum()));
        view->input->setFocus();
    }
    int sessionWindowStatus(void *key) const
    {
        const QWidget *view = detached_.contains(key) ? static_cast<const QWidget *>(detached_.value(key)) : this;
        return !view->isVisible() || view->isMinimized() ? 2 : view->isActiveWindow() ? 1 : 0;
    }
    void requestTransferSend(void *key, const QString &nick, int maxcps, int passive)
    {
        if (!conversations_.contains(key)) return;
        const Target target{key, conversations_[key].history};
        QTimer::singleShot(0, this, [this, target, nick, maxcps, passive]() { if (alive(target)) chooseTransferSend(target.key, nick, maxcps, passive); });
    }
    void chooseTransferSend(void *key, const QString &nick, int maxcps, int passive)
    {
        if (!conversations_.contains(key)) return;
        const Target target{key, conversations_[key].history};
        const QPointer<MainWindow> guard(this);
        const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Send Files to %1").arg(nick));
        if (!guard) return;
        for (const auto &path : paths) {
            if (!guard || !alive(target)) break;
            char error[1024] = {};
            const bool sent = qt_core_transfer_send(target.key, nick.toUtf8().constData(), path.toUtf8().constData(), maxcps, passive, error, sizeof(error));
            if (!guard) return;
            if (!sent) { statusBar()->showMessage(QString::fromUtf8(error), 8000); break; }
        }
        restoreEngineFocus();
    }
    void promptTransferSend()
    {
        const auto target = captureTarget(); QtSessionInfo info{};
        if (!alive(target) || !qt_core_session_info(target.key, &info) || !info.connected) { statusBar()->showMessage(QStringLiteral("Connect to a server before sending a file."), 6000); return; }
        bool ok = false; const QPointer<MainWindow> guard(this);
        const QString nick = QInputDialog::getText(this, QStringLiteral("Send File"), QStringLiteral("Recipient nickname:"), QLineEdit::Normal,
            info.query ? conversations_[target.key].name : QString(), &ok).trimmed();
        if (guard && ok && alive(target) && !nick.isEmpty() && !nick.contains(QRegularExpression(QStringLiteral("[\\s,]")))) chooseTransferSend(target.key, nick, 0, 0);
    }
    void promptDirectChat()
    {
        const auto target = captureTarget(); QtSessionInfo info{};
        if (!alive(target) || !qt_core_session_info(target.key, &info) || !info.connected) { statusBar()->showMessage(QStringLiteral("Connect to a server before offering direct chat."), 6000); return; }
        bool ok = false; const QPointer<MainWindow> guard(this);
        const QString nick = QInputDialog::getText(this, QStringLiteral("Direct Chat"), QStringLiteral("Recipient nickname:"), QLineEdit::Normal,
            info.query ? conversations_[target.key].name : QString(), &ok).trimmed();
        if (!guard || !ok || !alive(target)) return;
        char error[1024] = {};
        const bool result = qt_core_direct_chat_start(target.key, nick.toUtf8().constData(), 0, error, sizeof(error));
        if (!guard) return;
        if (!result) statusBar()->showMessage(QString::fromUtf8(error), 8000);
        restoreEngineFocus();
    }
    void paletteChanged() {
        loadIrcPalette(QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("colors.conf")));
        applyDisplay(); updateMarker();
        for (auto it = conversations_.cbegin(); it != conversations_.cend(); ++it) activity(it.key(), it->activity);
    }
    void startupState(int level)
    {
        if (level == 0) return;
        const bool tray = level == 2 && desktop_->canHide();
        for (auto *dialog : findChildren<QDialog *>()) if (dialog->isWindow() && dialog->isVisible()) {
            if (tray) dialog->hide(); else dialog->showMinimized();
        }
        if (tray) desktop_->hideWindow(); else showMinimized();
    }
    void connectionChanged() { updateStatus(); }
    void customChanged(int kind) {
        if (customMenus_) customMenus_->schedule(kind == QT_CONFIG_USERMENU, kind == QT_CONFIG_USERMENU);
        for (auto *view : detached_) if (view->menus) view->menus->schedule(kind == QT_CONFIG_USERMENU, kind == QT_CONFIG_USERMENU);
        if (kind == QT_CONFIG_BUTTONS || kind == QT_CONFIG_DIALOG_BUTTONS) { rebuildButtons(); updateStatus(); }
    }
    void menusChanged() { if (customMenus_) customMenus_->schedule(false, true); for (auto *view : detached_) if (view->menus) view->menus->schedule(false, true); }
    void attention(void *key)
    {
        QWidget *target = detached_.contains(key) ? static_cast<QWidget *>(detached_.value(key)) : this;
        QtAlertPolicy policy{};
        if (qt_core_alert_policy(key, 0, 0, &policy) && policy.omit_focused && target->isActiveWindow()) return;
        QApplication::alert(target, 3000);
    }
    void trayFile(const QString &file) { if (!desktop_->setCustomIcon(file)) statusBar()->showMessage(QStringLiteral("Could not load the tray icon image."), 6000); }
    void trayFlash(const QString &first, const QString &second, int interval) { if (!desktop_->setFlashingIcons(first, second, interval)) statusBar()->showMessage(QStringLiteral("Could not load the tray flashing images."), 6000); }
    void trayState(int state) { desktop_->setUnread(state != 0); }
    void trayTooltip(const QString &text) { desktop_->setToolTip(text); }

    void clearUsers(void *key)
    {
        if (!conversations_.contains(key)) return;
        desktop_->forget(key);
        conversations_[key].users = std::make_shared<NickListState>();
        if (key == active_) refreshUsers();
    }

    bool userSelected(void *key, const QString &nick) const
    {
        return conversations_.contains(key) && conversations_[key].users->selected.contains(nick);
    }

    void selectUser(void *key, const QString &nick, bool clear, bool scroll)
    {
        if (!conversations_.contains(key)) return;
        auto &state = *conversations_[key].users;
        if (clear) state.selected.clear();
        if (state.users.contains(nick)) state.selected.insert(nick);
        auto *list = detached_.contains(key) ? detached_.value(key)->users : key == active_ ? nicks_ : nullptr;
        if (list) {
            list->refresh(); if (!nick.isEmpty()) list->selectNick(nick, scroll);
            if (detached_.contains(key)) refreshDetachedControls(key); else updateStatus();
        }
    }

    void activity(void *key, int level)
    {
        if (!items_.contains(key)) return;
        auto *item = items_.value(key);
        level &= 7;
        if ((key == active_ && isActiveWindow()) || (detached_.contains(key) && detached_.value(key)->isActiveWindow())) level = 0;
        if (level) level = qMax(level, conversations_[key].activity);
        conversations_[key].activity = level;
        QFont font = channels_->font();
        font.setBold(level > 0);
        item->setFont(0, font);
        if (level > 0 && trayAllowed(key)) desktop_->setUnread(true);
        if (level == 3) item->setForeground(0, ircPaletteColor(38, channels_->palette()));
        else if (level == 2) item->setForeground(0, ircPaletteColor(39, channels_->palette()));
        else if (level == 1) item->setForeground(0, ircPaletteColor(37, channels_->palette()));
        else if (level == 0) item->setForeground(0, channels_->palette().text());
        refreshTabs();
    }

    bool sessionAttached(void *key) const { return conversations_.contains(key) && !detached_.contains(key); }

    ChatInput *inputFor(void *key) const { return detached_.contains(key) ? detached_.value(key)->input : key == active_ ? input_ : nullptr; }
    const char *inputText(void *key)
    {
        if (!conversations_.contains(key)) return "";
        inputBytes_ = (inputFor(key) ? inputFor(key)->text() : conversations_[key].draft).toUtf8();
        return inputBytes_.constData();
    }

    void setInput(void *key, const QString &text)
    {
        if (!conversations_.contains(key)) return;
        conversations_[key].draft = text;
        conversations_[key].inputCursor = text.size();
        if (inputFor(key)) inputFor(key)->setText(text);
    }

    int inputCursor(void *key) const
    {
        if (!conversations_.contains(key)) return 0;
        const QString text = inputFor(key) ? inputFor(key)->text() : conversations_[key].draft;
        const int pos = inputFor(key) ? inputFor(key)->cursorPosition() : conversations_[key].inputCursor;
        return text.left(pos).toUcs4().size();
    }

    void setInputCursor(void *key, bool relative, int position)
    {
        if (!conversations_.contains(key)) return;
        const QString text = inputFor(key) ? inputFor(key)->text() : conversations_[key].draft;
        qint64 target = relative ? qint64(inputCursor(key)) + position : position;
        if (target < 0) target = relative ? 0 : text.toUcs4().size();
        int utf16 = 0;
        while (utf16 < text.size() && target-- > 0) {
            if (text[utf16].isHighSurrogate() && utf16 + 1 < text.size() && text[utf16 + 1].isLowSurrogate())
                ++utf16;
            ++utf16;
        }
        conversations_[key].inputCursor = utf16;
        if (inputFor(key)) inputFor(key)->setCursorPosition(utf16);
    }

    void notify(const QString &text, bool warning)
    {
        statusBar()->showMessage(text, 12000);
        if (warning) QMessageBox::warning(this, QStringLiteral("ZoiteChat"), text);
        else if (active_) print(active_, text, 0);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (channels_ && (object == channels_->viewport() || object == tabs_)) {
            if (event->type() == QEvent::Wheel && wheelSwitches_) {
                auto *wheel = static_cast<QWheelEvent *>(event);
                const int delta = wheel->angleDelta().y();
                if (delta) { switchConversation(delta < 0 ? 1 : -1); event->accept(); return true; }
            }
            if (event->type() == QEvent::Wheel && object == tabs_) { event->accept(); return true; }
            if (event->type() == QEvent::MouseButtonPress && middleCloses_ && static_cast<QMouseEvent *>(event)->button() == Qt::MiddleButton) {
                event->accept(); return true;
            }
            if (event->type() == QEvent::MouseButtonRelease && middleCloses_) {
                auto *mouse = static_cast<QMouseEvent *>(event);
                if (mouse->button() == Qt::MiddleButton) {
                    void *key = object == tabs_ ? tabKey(tabs_->tabAt(mouse->position().toPoint())) : keyForItem(channels_->itemAt(mouse->position().toPoint()));
                    if (conversations_.contains(key)) submitTo({key, conversations_[key].history}, QStringLiteral("/close"));
                    event->accept(); return true;
                }
            }
        }
        return QMainWindow::eventFilter(object, event);
    }
    void changeEvent(QEvent *event) override
    {
        QMainWindow::changeEvent(event);
        if (event->type() == QEvent::WindowStateChange && isMinimized() && minimizeToTray_ && desktop_ && desktop_->canHide()) {
            QTimer::singleShot(0, this, [this]() { if (isMinimized() && minimizeToTray_) desktop_->hideWindow(); });
        }
        if (event->type() == QEvent::ActivationChange && isActiveWindow() && desktop_ && active_) {
            qt_core_activate(active_); activity(active_, 0); desktop_->markRead(active_);
            updateTrayUnread();
        }
    }
    void closeEvent(QCloseEvent *event) override
    {
        if (!quitRequested_ && desktop_->hideOnClose()) {
            savePreferences(); if (desktop_->hideWindow()) { event->ignore(); return; }
        }
        if (!closing_) {
            closing_ = true;
            savePreferences();
            qt_core_quit();
        }
        event->accept();
    }

private:
    struct Target {
        void *key = nullptr;
        std::shared_ptr<InputHistory> identity;
    };
    Target captureTarget() const
    {
        return {active_, conversations_.contains(active_) ? conversations_[active_].history : nullptr};
    }
    bool alive(const Target &target) const
    {
        return target.key && conversations_.contains(target.key) &&
               conversations_[target.key].history == target.identity;
    }
    void activateUser(const Target &target, const QString &nick)
    {
        if (!alive(target)) return;
        const auto membership = conversations_[target.key].users;
        if (!membership->users.contains(nick)) return;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{};
            if (qt_core_preference_read(i, &pref) && QByteArray(pref.key) == "gui_ulist_doubleclick") {
                const QString command = QString::fromUtf8(pref.value);
                const auto context = popupContext(target, {nick}, membership, true);
                if (!command.trimmed().isEmpty() && context.command) context.command(command);
                return;
            }
        }
    }
    CustomPopupContext popupContext(const Target &target, const QStringList &targets,
                                    const std::shared_ptr<NickListState> &membership = {}, bool nicknames = false)
    {
        if (!alive(target)) return {};
        auto execute = [this, target, targets, membership, nicknames](const QString &command, void *id, bool checked) {
            if (!alive(target) || (membership && conversations_[target.key].users != membership)) return;
            QStringList live;
            for (const auto &name : targets) if (!nicknames || (membership && membership->users.contains(name))) live.append(name);
            if (live.isEmpty()) return;
            const QByteArray all = live.join(QLatin1Char(' ')).toUtf8(), bytes = command.toUtf8();
            if (command.contains(QStringLiteral("%a"))) live = {live.first()};
            for (const auto &name : live) {
                if (!alive(target) || (membership && conversations_[target.key].users != membership)) break;
                if (nicknames && !membership->users.contains(name)) continue;
                const QByteArray nick = name.toUtf8();
                if (id) qt_core_menu_activate_target(target.key, id, bytes.constData(), checked, nick.constData(), all.constData());
                else qt_core_custom_run_target(target.key, bytes.constData(), nick.constData(), all.constData());
            }
            restoreEngineFocus();
        };
        return {[execute](const QString &command) { execute(command, nullptr, false); },
                [execute](void *id, const QString &command, bool checked) { execute(command, id, checked); }};
    }
    QWidget *buildModeRow(QWidget *parent, std::function<Target()> context, QMap<QChar, QToolButton *> &buttons, QToolButton *&key, QToolButton *&limit)
    {
        auto *row = new QWidget(parent); auto *layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(3);
        for (QChar mode : QStringLiteral("cntim")) {
            auto *button = new QToolButton(row); button->setText(QStringLiteral("+") + mode); button->setToolButtonStyle(Qt::ToolButtonTextOnly); button->setCheckable(true);
            button->setObjectName(QStringLiteral("channelMode_%1").arg(mode)); button->setToolTip(QStringLiteral("Toggle channel mode %1; the server must confirm changes").arg(mode));
            buttons.insert(mode, button); layout->addWidget(button);
            connect(button, &QToolButton::clicked, row, [this, context, mode](bool checked) {
                const auto target = context(); QtChannelState state{};
                if (!alive(target) || !qt_core_channel_state(target.key, &state)) { updateStatus(); return; }
                const QByteArray channel(state.channel); const QPointer<MainWindow> guard(this);
                const bool sent = qt_core_channel_mode(target.key, channel.constData(), mode.toLatin1(), checked, "");
                if (!guard) return;
                updateStatus();
                auto *bar = detached_.contains(target.key) ? detached_.value(target.key)->statusBar() : statusBar();
                bar->showMessage(sent ? QStringLiteral("Mode change requested; waiting for server confirmation.") : QStringLiteral("Could not request this channel mode."), 6000);
            });
        }
        key = new QToolButton(row); key->setObjectName(QStringLiteral("channelKeyStatus")); key->setToolTip(QStringLiteral("Open channel key settings; the current key is not displayed here")); layout->addWidget(key);
        limit = new QToolButton(row); limit->setObjectName(QStringLiteral("channelLimitStatus")); layout->addWidget(limit);
        for (auto *button : {key, limit}) connect(button, &QToolButton::clicked, row, [this, context, parent]() { const auto target = context(); if (alive(target)) showChannelSettings(target.key, parent); });
        layout->addStretch(); row->hide(); return row;
    }
    void refreshModes(void *key, QWidget *row, const QMap<QChar, QToolButton *> &buttons, QToolButton *keyButton, QToolButton *limitButton, bool show)
    {
        QtSessionInfo info{}; qt_core_session_info(key, &info);
        row->setVisible(info.channel && show);
        QtChannelState channel{}; const bool confirmed = info.connected && info.channel && qt_core_channel_state(key, &channel);
        const QString modes = confirmed ? QString::fromUtf8(channel.modes).section(QLatin1Char(' '), 0, 0) : QString();
        const QString simple = confirmed ? QString::fromUtf8(channel.simple) : QString();
        const QString supported = confirmed ? QString::fromUtf8(channel.supported) : QString();
        for (auto it = buttons.cbegin(); it != buttons.cend(); ++it) {
            const QSignalBlocker blocker(it.value()); it.value()->setVisible(simple.contains(it.key())); it.value()->setEnabled(confirmed && simple.contains(it.key()));
            it.value()->setChecked(modes.contains(it.key()));
            it.value()->setAccessibleName(QStringLiteral("Channel mode %1: %2").arg(it.key()).arg(modes.contains(it.key()) ? QStringLiteral("enabled") : QStringLiteral("disabled")));
        }
        keyButton->setEnabled(confirmed && supported.contains(QLatin1Char('k'))); keyButton->setText(confirmed && channel.key_set ? QStringLiteral("Key: set") : QStringLiteral("Key: none"));
        limitButton->setEnabled(confirmed && supported.contains(QLatin1Char('l'))); limitButton->setText(confirmed && channel.limit > 0 ? QStringLiteral("Limit: %1").arg(channel.limit) : QStringLiteral("Limit: none"));
    }
    void updateCommandButtons(void *key, NickList *users, QScrollArea *userButtons, QScrollArea *dialogButtons, bool showUsers, bool showDialog)
    {
        QtSessionInfo info{}; const bool valid = key && qt_core_session_info(key, &info);
        userButtons->setVisible(valid && info.channel && showUsers); dialogButtons->setVisible(valid && info.query && showDialog);
        bool selected = false; for (auto *item : users->selectedItems()) if (!item->isHidden()) { selected = true; break; }
        for (auto *area : {userButtons, dialogButtons}) for (auto *button : area->findChildren<QPushButton *>()) {
            const QString command = button->property("command").toString();
            button->setEnabled(valid && info.connected && customCommandSupported(command) &&
                (area == dialogButtons || selected || !command.contains(QRegularExpression(QStringLiteral("%[sahu]")))));
        }
    }
    void refreshDetachedControls(void *key)
    {
        auto *view = detached_.value(key); if (!view) return;
        QtSessionInfo info{}; const bool valid = qt_core_session_info(key, &info);
        const bool connected = valid && info.connected;
        auto enable = [view](const char *name, bool enabled) {
            if (auto *action = view->findChild<QAction *>(QString::fromLatin1(name))) action->setEnabled(enabled);
        };
        enable("detachedDisconnect", valid && (connected || info.connecting || info.reconnect_wait));
        enable("detachedReconnect", valid && info.host && *info.host);
        enable("detachedJoin", connected); enable("detachedChannelList", connected);
        enable("detachedAway", connected);
        if (auto *away = view->findChild<QAction *>(QStringLiteral("detachedAway"))) away->setChecked(connected && info.away);
        enable("detachedBanList", connected && info.channel);
        enable("detachedChannelSettings", connected && info.channel);
        QtConversationOptions options{};
        enable("detachedConversationOptions", valid && qt_core_conversation_options(key, &options));
        enable("detachedRawLog", valid);
        view->topic->setReadOnly(!(connected && info.channel));
        view->users->setModerationEnabled(connected && info.channel);
        refreshNickname(key, view->nickname);
        const int userIndex = userListPosition_ <= 2 ? 0 : 1;
        if (view->splitter->indexOf(view->userPanel) != userIndex) view->splitter->insertWidget(userIndex, view->userPanel);
        refreshUserCount(key, view->users, view->userCount);
        refreshMeters(info, conversations_.contains(key), view->lagStatus, view->queueStatus, view->lagMeter, view->queueMeter);
        auto *listAction = qobject_cast<QAction *>(view->users->property("userListAction").value<QObject *>());
        view->userPanel->setVisible(info.channel && (!listAction || listAction->isChecked()));
        refreshModes(key, view->modeRow, view->modeButtons, view->keyStatus, view->limitStatus, view->showModes->isChecked());
        updateCommandButtons(key, view->users, view->userButtons, view->dialogButtons, view->showUserButtons->isChecked(), view->showDialogButtons->isChecked());
    }
    void rebuildButtons()
    {
        if (!userButtons_ || !dialogButtons_) return;
        auto build = [this](QScrollArea *area, int kind, int columns, std::function<Target()> context) {
            auto *content = new QWidget(area); auto *grid = new QGridLayout(content);
            grid->setContentsMargins(2, 2, 2, 2); grid->setSpacing(2);
            const auto rows = customDefinitions(kind);
            for (int i = 0; i < qMin(64, int(rows.size())); ++i) {
                const auto row = rows[i]; auto *button = new QPushButton(row.first, content);
                button->setObjectName(QStringLiteral("customCommandButton")); button->setProperty("command", row.second);
                button->setToolTip(customCommandSupported(row.second) ? row.second : QStringLiteral("No command is configured."));
                grid->addWidget(button, i / columns, i % columns);
                connect(button, &QPushButton::clicked, this, [this, row, kind, context]() {
                    const auto target = context(); if (!alive(target)) return;
                    const auto state = conversations_[target.key].users;
                    QStringList names;
                    if (kind == QT_CONFIG_BUTTONS) {
                        auto *users = detached_.contains(target.key) ? detached_.value(target.key)->users : nicks_;
                        for (auto *item : users->selectedItems()) if (!item->isHidden()) names.append(item->data(Qt::UserRole).toString());
                        if (names.isEmpty() && !row.second.contains(QRegularExpression(QStringLiteral("%[sahu]")))) names.append(QString());
                    } else names.append(conversations_[target.key].name);
                    const auto context = popupContext(target, names, state, kind == QT_CONFIG_BUTTONS && !names.contains(QString()));
                    context.command(row.second);
                });
            }
            if (auto *old = area->takeWidget()) old->deleteLater();
            area->setWidget(content);
        };
        build(userButtons_, QT_CONFIG_BUTTONS, 2, [this]() { return captureTarget(); }); build(dialogButtons_, QT_CONFIG_DIALOG_BUTTONS, 4, [this]() { return captureTarget(); });
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it) {
            const Target target{it.key(), conversations_[it.key()].history};
            build(it.value()->userButtons, QT_CONFIG_BUTTONS, 2, [target]() { return target; });
            build(it.value()->dialogButtons, QT_CONFIG_DIALOG_BUTTONS, 4, [target]() { return target; });
            refreshDetachedControls(it.key());
        }
    }
    void restoreEngineFocus()
    {
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it)
            if (it.value()->isActiveWindow()) { qt_core_activate(it.key()); return; }
        if (conversations_.contains(active_)) qt_core_activate(active_);
    }
    void submitTo(const Target &target, const QString &line, bool inputSyntax = false)
    {
        if (!alive(target) || line.contains(QLatin1Char('\n')) || line.contains(QLatin1Char('\r'))) return;
        const QByteArray bytes = line.toUtf8();
        if (inputSyntax) qt_core_submit(target.key, bytes.constData());
        else qt_core_submit_command(target.key, bytes.constData());
        // A prompt can outlive a tab switch; keep the engine's front session
        // aligned with the conversation still visible after the command.
        restoreEngineFocus();
        updateStatus();
    }
    void *keyForItem(QTreeWidgetItem *item) const
    {
        if (!item) return nullptr;
        void *key = reinterpret_cast<void *>(item->data(0, Qt::UserRole).value<quintptr>());
        return items_.value(key) == item ? key : nullptr;
    }
    void refreshMeterMenus()
    {
        for (auto it = meterMenus_.begin(); it != meterMenus_.end();) {
            if (!*it) { it = meterMenus_.erase(it); continue; }
            auto *menu = it->data(); auto *group = menu->findChild<QActionGroup *>();
            group->setExclusive(false);
            for (auto *action : menu->actions()) { const QSignalBlocker blocker(action); action->setChecked(lagMeterMode_ == queueMeterMode_ && action->data().toInt() == lagMeterMode_); }
            group->setExclusive(true); ++it;
        }
    }
    void setMeterMode(int mode)
    {
        QList<QtPreferenceChange> changes;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{}; if (!qt_core_preference_read(i, &pref)) continue;
            if (QByteArray(pref.key) != "gui_lagometer" && QByteArray(pref.key) != "gui_throttlemeter") continue;
            const QByteArray value = QByteArray::number(mode); if (QByteArray(pref.value) == value) continue;
            QtPreferenceChange change{}; change.index = i;
            qstrncpy(change.expected, pref.value, sizeof(change.expected)); qstrncpy(change.value, value.constData(), sizeof(change.value)); changes.append(change);
        }
        char error[512]{};
        if (!changes.isEmpty() && !qt_core_preferences_apply(changes.constData(), changes.size(), error, sizeof(error))) statusBar()->showMessage(QString::fromUtf8(error), 8000);
        applyDisplay();
    }
    void addMeterMenu(QMenu *view, bool detached = false)
    {
        auto *menu = view->addMenu(QStringLiteral("&Network Meters"));
        menu->setObjectName(detached ? QStringLiteral("detachedNetworkMetersMenu") : QStringLiteral("networkMetersMenu"));
        auto *group = new QActionGroup(menu); group->setExclusive(true);
        const QStringList labels{QStringLiteral("Off"), QStringLiteral("Graph"), QStringLiteral("Text"), QStringLiteral("Both")};
        for (int mode = 0; mode < labels.size(); ++mode) {
            auto *action = menu->addAction(labels[mode]); action->setCheckable(true); action->setData(mode); group->addAction(action);
            action->setObjectName((detached ? QStringLiteral("detachedNetworkMeter") : QStringLiteral("networkMeter")) + QString::number(mode));
            connect(action, &QAction::triggered, this, [this, mode](bool checked) { if (checked) setMeterMode(mode); });
        }
        meterMenus_.append(menu); refreshMeterMenus();
    }
    QAction *addAction(QMenu *menu, const QString &name, const QString &id,
                       const QKeySequence &shortcut, std::function<void()> callback)
    {
        auto *action = menu->addAction(name, this, std::move(callback));
        action->setObjectName(id);
        if (!shortcut.isEmpty()) action->setShortcut(shortcut);
        return action;
    }
    bool nextUnread(void *source = nullptr)
    {
        if (detached_.contains(source ? source : active_)) return false;
        void *next = qt_core_next_activity();
        if (!sessionAttached(next)) { statusBar()->showMessage(QStringLiteral("No unread conversations"), 3000); return false; }
        select(next);
        return true;
    }
    void switchConversation(int direction, bool unreadOnly = false, void *source = nullptr)
    {
        if (unreadOnly) { nextUnread(source); return; }
        const auto rows = switcherItems(); const int count = rows.size();
        if (!count) return;
        const int row = rows.indexOf(items_.value(source ? source : active_));
        select(keyForItem(rows[(row + direction + count * 2) % count]));
    }
    bool pluginKeypress(const Target &target, const QKeyEvent &event)
    {
        if (!alive(target)) return true;
        const QPointer<MainWindow> guard(this);
        const QPointer<ChatInput> source(inputFor(target.key));
        const auto membership = conversations_[target.key].users;
        const auto key = pluginKey(event);
        const bool consumed = qt_core_keypress(target.key, key.state, key.keyval, key.unicode) != 0;
        return !guard || !source || !alive(target) || inputFor(target.key) != source || conversations_[target.key].users != membership || consumed;
    }
    bool runInputBinding(const InputBinding &binding, void *source = nullptr)
    {
        const auto target = source && conversations_.contains(source) ? Target{source, conversations_[source].history} : captureTarget();
        if (!alive(target)) return false;
        auto *input = inputFor(target.key); if (!input) return false;
        auto *chat = detached_.contains(target.key) ? detached_.value(target.key)->chat : chat_;
        const QString &action = binding.action;
        if (action == QStringLiteral("Run Command")) {
            const auto membership = conversations_[target.key].users; const QPointer<MainWindow> guard(this);
            for (const auto &line : inputBindingCommands(binding.first)) {
                if (!guard || !alive(target) || conversations_[target.key].users != membership) break;
                submitTo(target, line, true);
            }
            return true;
        }
        if (action == QStringLiteral("Insert in Buffer")) { if (binding.first.isEmpty()) return false;
            const ushort control = binding.first.size() == 1 ? binding.first.front().unicode() : 0;
            if (control == 2 || control == 3 || control == 15 || control == 22 || control == 29 || control == 31) input->insertFormatting(binding.first.front());
            else input->insert(binding.first);
            return true; }
        if (action == QStringLiteral("Set Buffer")) { if (binding.first.isEmpty()) return false; input->setText(binding.first); input->setCursorPosition(input->text().size()); return true; }
        if (action == QStringLiteral("Last Command") || action == QStringLiteral("Next Command")) { input->historyStep(action == QStringLiteral("Last Command")); return true; }
        if (action == QStringLiteral("Complete nick/command") || action == QStringLiteral("Change Selected Nick")) { input->completeNick(!binding.first.isEmpty()); return true; }
        if (action == QStringLiteral("Push input line into history")) { conversations_[target.key].history->record(input->text()); input->clear(); return true; }
        if (action == QStringLiteral("Check For Replace")) { input->checkReplacement(); return false; }
        if (action == QStringLiteral("Change Page")) {
            if (binding.first.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0) return nextUnread(target.key);
            bool valid = false; const int number = binding.first.toInt(&valid); if (!valid) return false;
            const auto rows = switcherItems(); const int count = rows.size(); if (!count) return true;
            if (binding.second.isEmpty() && (number < 1 || number > count)) return true;
            const qint64 requested = binding.second.isEmpty() ? qint64(number) - 1 : qint64(rows.indexOf(items_.value(target.key))) + number;
            const int row = int((requested % count + count) % count); select(keyForItem(rows[row])); return true;
        }
        if (action == QStringLiteral("Scroll Page")) {
            auto *scroll = chat->verticalScrollBar(); const QString direction = binding.first.toLower();
            if (direction == QStringLiteral("top")) scroll->setValue(scroll->minimum());
            else if (direction == QStringLiteral("bottom")) scroll->setValue(scroll->maximum());
            else if (direction == QStringLiteral("-1")) scroll->setValue(scroll->value() - scroll->singleStep());
            else if (direction == QStringLiteral("+1")) scroll->setValue(scroll->value() + scroll->singleStep());
            else scroll->setValue(scroll->value() + (direction == QStringLiteral("up") ? -1 : 1) * scroll->pageStep());
            return true;
        }
        if (action == QStringLiteral("Move tab family left") || action == QStringLiteral("Move tab family right")) {
            void *server = qt_core_session_server(target.key); if (!server || detached_.contains(target.key)) return false;
            QList<QTreeWidgetItem *> family, others; int first = -1;
            for (auto *item : switcherItems()) {
                if (qt_core_session_server(keyForItem(item)) == server) { if (first < 0) first = others.size(); family.append(item); }
                else others.append(item);
            }
            const bool left = action.endsWith(QStringLiteral("left"));
            if (first < 0 || (left && first == 0) || (!left && first >= others.size())) return true;
            int insertion = left ? first - 1 : first;
            void *neighbor = qt_core_session_server(keyForItem(others[insertion]));
            if (left) while (insertion > 0 && qt_core_session_server(keyForItem(others[insertion - 1])) == neighbor) --insertion;
            else while (insertion < others.size() && qt_core_session_server(keyForItem(others[insertion])) == neighbor) ++insertion;
            const QSignalBlocker blocker(channels_);
            for (int i = 0; i < family.size(); ++i) others.insert(insertion + i, family[i]);
            rebuildSwitcher(others);
            channels_->setCurrentItem(items_.value(target.key)); return true;
        }
        if (action == QStringLiteral("Move front tab left") || action == QStringLiteral("Move front tab right")) {
            auto *item = items_.value(target.key); if (!item) return false;
            auto *parent = item->parent();
            const int current = parent ? parent->indexOfChild(item) : channels_->indexOfTopLevelItem(item);
            const int next = current + (action.endsWith(QStringLiteral("left")) ? -1 : 1);
            const int count = parent ? parent->childCount() : channels_->topLevelItemCount();
            if (next >= 0 && next < count) {
                const QSignalBlocker blocker(channels_);
                if (parent) { parent->takeChild(current); parent->insertChild(next, item); }
                else { channels_->takeTopLevelItem(current); channels_->insertTopLevelItem(next, item); }
                channels_->setCurrentItem(item); refreshTabs();
            }
            return true;
        }
        return false;
    }
    void editTopic(const Target &target)
    {
        if (!alive(target)) return;
        bool ok = false;
        const QString text = QInputDialog::getText(this, QStringLiteral("Edit Topic"),
            QStringLiteral("Topic (empty clears it; channel permissions apply):"),
            QLineEdit::Normal, displayTopic(conversations_[target.key].topic), &ok);
        if (ok && alive(target)) {
            const QByteArray bytes = text.toUtf8();
            qt_core_set_topic(target.key, bytes.constData());
        }
    }
    void printCompletions(const Target &target, const QStringList &matches)
    {
        if (!alive(target)) return;
        QString line;
        for (const QString &match : matches) {
            if (line.size() + match.size() > 512 && !line.isEmpty()) { print(target.key, line, 0); line.clear(); }
            if (!line.isEmpty()) line += QLatin1Char(' ');
            line += match;
        }
        if (!line.isEmpty()) print(target.key, line, 0);
    }
    void changeNickname(const Target &target)
    {
        QtSessionInfo info{};
        if (!alive(target) || !qt_core_session_info(target.key, &info) || !info.connected) return;
        const QString initial = QString::fromUtf8(info.nick ? info.nick : "");
        bool accepted = false;
        const QString value = QInputDialog::getText(detached_.contains(target.key) ? static_cast<QWidget *>(detached_.value(target.key)) : this,
            QStringLiteral("Change Nickname"), QStringLiteral("Nickname:"), QLineEdit::Normal, initial, &accepted).trimmed();
        if (accepted && !value.isEmpty() && !value.contains(QRegularExpression(QStringLiteral("[\\s\\x00-\\x1f]")))) submitTo(target, QStringLiteral("/nick ") + value);
    }
    void refreshNickname(void *key, QPushButton *button)
    {
        QtSessionInfo info{}; const bool valid = key && qt_core_session_info(key, &info);
        const QString nick = valid ? QString::fromUtf8(info.nick ? info.nick : "") : QString();
        button->setText(QString(nick).replace(QLatin1Char('&'), QStringLiteral("&&")));
        button->setEnabled(valid && info.connected);
        auto palette = QApplication::palette(button);
        if (valid && info.away) palette.setColor(QPalette::ButtonText, ircPaletteColor(40, palette));
        button->setPalette(palette);
        button->setToolTip(valid && info.away ? QStringLiteral("Away — click to change nickname") : QStringLiteral("Change nickname"));
        QString prefix;
        if (valid && info.channel && conversations_.contains(key)) {
            const auto &users = *conversations_[key].users;
            QString ownNick = nick;
            for (auto it = users.details.cbegin(); it != users.details.cend(); ++it) if (it->me) { ownNick = it.key(); break; }
            const QString display = users.users.value(ownNick);
            if (display.endsWith(ownNick)) prefix = display.left(display.size() - ownNick.size()).left(1);
        }
        button->setIcon(inputIcon_ && valid ? rankIcon(prefix, QString::fromUtf8(info.nick_prefixes ? info.nick_prefixes : "")) : QIcon());
    }
    void promptCommandFor(const Target &target, const QString &title, const QString &prompt, const QString &command)
    {
        if (!alive(target)) return;
        bool accepted = false;
        const QString value = QInputDialog::getText(detached_.contains(target.key) ? static_cast<QWidget *>(detached_.value(target.key)) : this, title, prompt, QLineEdit::Normal, {}, &accepted).trimmed();
        if (accepted && !value.isEmpty()) submitTo(target, command + value);
    }
    void promptCommand(const QString &title, const QString &prompt, const QString &command,
                       const QString &initial = QString())
    {
        const auto target = captureTarget();
        if (!alive(target)) return;
        bool ok = false;
        const QString value = QInputDialog::getText(this, title, prompt, QLineEdit::Normal, initial, &ok).trimmed();
        if (ok && !value.isEmpty()) submitTo(target, command + value);
    }
    bool validMarker(const Conversation &conversation) const
    {
        const auto *tag = conversation.marker.isValid() ? dynamic_cast<MarkerTag *>(conversation.marker.userData()) : nullptr;
        return tag && tag->id == conversation.markerId;
    }
    void placeMarker(Conversation &conversation, QTextBlock block)
    {
        if (validMarker(conversation)) conversation.marker.setUserData(nullptr);
        conversation.marker = block;
        conversation.markerId = ++nextMarkerId_;
        if (block.isValid()) block.setUserData(new MarkerTag(conversation.markerId));
    }
    void resetMarker()
    {
        if (!conversations_.contains(active_) || conversations_[active_].document->isEmpty()) return;
        placeMarker(conversations_[active_], conversations_[active_].document->lastBlock());
        updateMarker(); statusBar()->showMessage(QStringLiteral("Marker set"), 3000);
    }
    void updateMarker()
    {
        auto update = [this](QTextBrowser *view, void *key) {
            QList<QTextEdit::ExtraSelection> selections;
            if (showMarker_ && conversations_.contains(key) && validMarker(conversations_[key])) {
                QTextEdit::ExtraSelection marker;
                marker.cursor = QTextCursor(conversations_[key].marker);
                QColor color = ircPaletteColor(36, palette()); color.setAlpha(70);
                marker.format.setBackground(color); marker.format.setProperty(QTextFormat::FullWidthSelection, true);
                selections.append(marker);
            }
            view->setExtraSelections(selections);
        };
        update(chat_, active_);
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it) update(it.value()->chat, it.key());
    }
    void moveToMarker()
    {
        if (!conversations_.contains(active_) || !validMarker(conversations_[active_])) {
            statusBar()->showMessage(QStringLiteral("No marker remains in this conversation's scrollback"), 4000); return;
        }
        chat_->setTextCursor(QTextCursor(conversations_[active_].marker)); chat_->ensureCursorVisible();
    }
    void previewPaste(const QString &text, void *source = nullptr)
    {
        if (pasteQueue_) { statusBar()->showMessage(QStringLiteral("Wait for the current paste to finish"), 4000); return; }
        const auto target = source && conversations_.contains(source) ? Target{source, conversations_[source].history} : captureTarget();
        if (!alive(target)) return;
        if (text.size() > 65536) { statusBar()->showMessage(QStringLiteral("Paste is larger than 64 KiB; split it into smaller parts"), 6000); return; }
        const auto membership = conversations_[target.key].users;
        auto *input = inputFor(target.key); if (!input) return;
        const QString originalDraft = input->text();
        PastePreview dialog(text, conversations_[target.key].name, detached_.contains(target.key) ? static_cast<QWidget *>(detached_.value(target.key)) : this, QString::fromUtf8(qt_core_command_prefix()));
        if (dialog.exec() != QDialog::Accepted) return;
        const QStringList lines = dialog.lines(); const bool commands = dialog.commandsEnabled();
        if (lines.isEmpty() || lines.size() > 100 || lines.join(QLatin1Char('\n')).size() > 65536 ||
            !alive(target) || conversations_[target.key].users != membership) return;
        QtSessionInfo info{};
        if (!qt_core_session_info(target.key, &info) || (!commands && (!info.connected || info.server))) {
            statusBar()->showMessage(QStringLiteral("Select a connected channel or private conversation before sending text"), 6000); return;
        }
        if (inputFor(target.key) && inputFor(target.key)->text() == originalDraft) inputFor(target.key)->clear();
        else if (conversations_[target.key].draft == originalDraft) conversations_[target.key].draft.clear();
        auto *queue = new QTimer(this); queue->setObjectName(QStringLiteral("pasteQueue")); queue->setSingleShot(true);
        pasteQueue_ = queue;
        connect(queue, &QTimer::timeout, this, [this, queue, target, membership, lines, commands, index = 0]() mutable {
            if (index >= lines.size()) return;
            QtSessionInfo info{};
            if (!alive(target) || conversations_[target.key].users != membership ||
                !qt_core_session_info(target.key, &info) || (!commands && !info.connected)) {
                queue->stop(); pasteQueue_ = nullptr; queue->deleteLater(); statusBar()->showMessage(QStringLiteral("Paste stopped: destination changed or disconnected"), 6000); return;
            }
            const QString line = lines[index++];
            conversations_[target.key].history->record(line);
            if (commands) submitTo(target, line, true);
            else {
                const QByteArray bytes = line.toUtf8(); qt_core_submit_literal(target.key, bytes.constData());
                restoreEngineFocus();
            }
            if (index == lines.size()) { queue->stop(); pasteQueue_ = nullptr; queue->deleteLater(); statusBar()->showMessage(QStringLiteral("Paste sent"), 4000); }
            else queue->start(200);
        });
        queue->start(200);
    }

    void requestQuit() { quitRequested_ = true; close(); }
    void setAllServersAway(bool away)
    {
        auto target = captureTarget();
        if (!alive(target)) for (auto it = conversations_.cbegin(); it != conversations_.cend(); ++it) {
            target = {it.key(), it->history}; break;
        }
        if (alive(target)) submitTo(target, away ? QStringLiteral("/allserv away") : QStringLiteral("/allserv back"));
    }
    void bindNativeVisibility(QAction *action, const char *key, bool inverted = false)
    {
        connect(action, &QAction::triggered, this, [this, key, inverted](bool checked) {
            saveNativeBoolean(key, checked != inverted);
            applyDisplay();
        });
    }
    void saveNativeBoolean(const char *key, bool value) { saveNativeValue(key, value ? "1" : "0"); }
    void saveNativeValue(const char *key, const QByteArray &value)
    {
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{}; if (!qt_core_preference_read(i, &pref) || QByteArray(pref.key) != key) continue;
            const char *wanted = value.constData(); if (QByteArray(pref.value) == value) return;
            QtPreferenceChange change{}; change.index = i;
            qstrncpy(change.expected, pref.value, sizeof(change.expected)); qstrncpy(change.value, wanted, sizeof(change.value));
            char error[512]{};
            if (!qt_core_preferences_apply(&change, 1, error, sizeof(error))) statusBar()->showMessage(QString::fromUtf8(error), 8000);
            return;
        }
    }
    bool movePane(PaneDrag::Pane pane, int position)
    {
        if (position < 1 || position > (pane == PaneDrag::Users || switcherLayout_ == 2 ? 4 : 6)) return false;
        int switcher = switcherPosition_, users = userListPosition_;
        if (switcherLayout_ == 2 && (switcher == 5 || switcher == 6)) switcher = 1;
        if (pane == PaneDrag::Users) users = position; else switcher = position;
        // Native drag behavior moves the other pane into the adjoining slot.
        if (switcher == users) {
            if (pane == PaneDrag::Users) switcher = users % 2 ? users + 1 : users - 1;
            else users = switcher % 2 ? switcher + 1 : switcher - 1;
        }
        QList<QtPreferenceChange> changes;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{}; if (!qt_core_preference_read(i, &pref)) continue;
            const QByteArray key(pref.key);
            if (key != "gui_tab_pos" && key != "gui_ulist_pos") continue;
            const QByteArray value = QByteArray::number(key == "gui_tab_pos" ? switcher : users);
            if (value == pref.value) continue;
            QtPreferenceChange change{}; change.index = i;
            qstrncpy(change.expected, pref.value, sizeof(change.expected)); qstrncpy(change.value, value.constData(), sizeof(change.value));
            changes.append(change);
        }
        char error[512]{};
        if (!qt_core_preferences_apply(changes.constData(), changes.size(), error, sizeof(error))) {
            statusBar()->showMessage(QString::fromUtf8(error), 8000); return false;
        }
        applyDisplay();
        return true;
    }
    void configureDesktop()
    {
        if (!desktop_ || !trayEnabled_ || !notifications_ || !previews_ || !closeToTray_) return;
        bool tray = true, close = false, blink = true;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{}; if (!qt_core_preference_read(i, &pref)) continue;
            const bool enabled = QByteArray(pref.value).toInt() != 0;
            if (QByteArray(pref.key) == "gui_tray") tray = enabled;
            else if (QByteArray(pref.key) == "gui_tray_close") close = enabled;
            else if (QByteArray(pref.key) == "gui_tray_away") trayAway_ = enabled;
            else if (QByteArray(pref.key) == "gui_tray_minimize") minimizeToTray_ = enabled;
            else if (QByteArray(pref.key) == "gui_tray_blink") blink = enabled;
        }
        { const QSignalBlocker blocker(trayEnabled_); trayEnabled_->setChecked(tray); }
        { const QSignalBlocker blocker(closeToTray_); closeToTray_->setChecked(close); }
        desktop_->setUnreadBlink(blink);
        desktop_->configure(tray, notifications_->isChecked(), previews_->isChecked(), close);
    }
    void scheduleUsersRefresh()
    {
        if (usersRefreshPending_) return;
        usersRefreshPending_ = true;
        QTimer::singleShot(0, this, [this]() { usersRefreshPending_ = false; refreshUsers(); });
    }
    void refreshUserCount(void *key, NickList *users, QLabel *label)
    {
        label->setVisible(showUserCount_);
        if (!conversations_.contains(key)) { label->setText(QStringLiteral("0 users")); return; }
        const auto state = conversations_[key].users;
        int away = 0;
        for (auto it = state->details.cbegin(); it != state->details.cend(); ++it)
            if (state->users.contains(it.key()) && it.value().away) ++away;
        label->setText(QStringLiteral("%1/%2 users · %3 away").arg(users->visibleCount()).arg(state->users.size()).arg(away));
    }
    void updateUserCount() { refreshUserCount(active_, nicks_, userCount_); }
    static QProgressBar *makeMeter(QWidget *parent, const QString &name, int maximum)
    {
        auto *bar = new QProgressBar(parent); bar->setObjectName(name); bar->setRange(0, maximum);
        bar->setValue(0); bar->setTextVisible(false); bar->setFixedWidth(70); bar->setMaximumHeight(14); bar->hide();
        return bar;
    }
    void refreshMeters(const QtSessionInfo &info, bool valid, QLabel *lagLabel, QLabel *queueLabel, QProgressBar *lagBar, QProgressBar *queueBar)
    {
        const bool connected = valid && info.connected;
        lagLabel->setVisible(connected && (lagMeterMode_ & 2)); queueLabel->setVisible(connected && (queueMeterMode_ & 2));
        lagBar->setVisible(connected && (lagMeterMode_ & 1)); queueBar->setVisible(connected && (queueMeterMode_ & 1));
        lagBar->setValue(connected ? qBound(0, info.lag_ms, 1000) : 0); queueBar->setValue(connected ? qBound(0, info.queued_bytes, 1024) : 0);
        if (!valid) { lagLabel->clear(); queueLabel->clear(); lagBar->setToolTip({}); queueBar->setToolTip({}); return; }
        const QString lag = QString::number(qMax(0, info.lag_ms) / 1000.0, 'f', 1);
        lagLabel->setText(QStringLiteral("Lag: %1%2 s").arg(info.lag_pending ? QStringLiteral("≥ ") : QString(), lag));
        const QString lagTip = lagLabel->text() + (info.lag_pending ? QStringLiteral(" — waiting for PONG (display capped at 30 seconds)") : QStringLiteral(" — latest round-trip time"));
        lagLabel->setToolTip(lagTip); lagBar->setToolTip(lagTip);
        queueLabel->setText(QStringLiteral("Queue: %1 B").arg(qMax(0, info.queued_bytes)));
        const QString queueTip = queueLabel->text() + QStringLiteral(" — bytes waiting in this server's outgoing queue");
        queueLabel->setToolTip(queueTip); queueBar->setToolTip(queueTip);
    }
    void showUserDetails(const Target &target, const QString &nick)
    {
        if (!alive(target)) return;
        const auto membership = conversations_[target.key].users;
        auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setObjectName(QStringLiteral("userDetails")); dialog->setWindowTitle(QStringLiteral("User Details — ") + nick);
        auto *form = new QFormLayout(dialog);
        const QStringList names = {QStringLiteral("Nickname"), QStringLiteral("Status"), QStringLiteral("Host"),
            QStringLiteral("Real name"), QStringLiteral("Account"), QStringLiteral("Server")};
        QList<QLabel *> fields;
        for (const QString &name : names) {
            auto *label = new QLabel(dialog); label->setTextFormat(Qt::PlainText);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse); label->setWordWrap(true);
            label->setObjectName(QStringLiteral("detail") + QString(name).remove(QLatin1Char(' ')));
            fields.append(label); form->addRow(name + QLatin1Char(':'), label);
        }
        auto *note = new QLabel(QStringLiteral("Only information already known to the IRC engine is shown."), dialog);
        note->setWordWrap(true); form->addRow(note);
        auto *buttons = new QHBoxLayout;
        auto *whois = new QPushButton(QStringLiteral("Request WHOIS"), dialog); whois->setObjectName(QStringLiteral("detailsWhois"));
        auto *close = new QPushButton(QStringLiteral("Close"), dialog);
        buttons->addWidget(whois); buttons->addWidget(close); form->addRow(buttons);
        auto refresh = [this, target, membership, nick, fields, note, whois]() {
            const bool live = alive(target) && conversations_[target.key].users == membership && membership->users.contains(nick);
            whois->setEnabled(live);
            if (!live) { note->setText(QStringLiteral("This user or conversation is no longer present. Showing cached details.")); return; }
            const auto details = membership->details.value(nick);
            const QStringList values = {membership->users.value(nick), details.away ? QStringLiteral("Away") : QStringLiteral("Here"),
                details.host, details.realName, details.account == QStringLiteral("*") ? QStringLiteral("Not identified") : details.account, details.server};
            for (int i = 0; i < fields.size(); ++i) fields[i]->setText(values[i].isEmpty() ? QStringLiteral("Unknown") : values[i]);
        };
        connect(whois, &QPushButton::clicked, dialog, [this, target, membership, nick]() {
            if (alive(target) && conversations_[target.key].users == membership && membership->users.contains(nick))
                submitTo(target, QStringLiteral("/whois ") + nick);
        });
        connect(close, &QPushButton::clicked, dialog, &QDialog::close);
        auto *timer = new QTimer(dialog); connect(timer, &QTimer::timeout, dialog, refresh); timer->start(1000);
        refresh(); dialog->show();
    }

    void handleUserAction(NickAction action, const QStringList &nicks, void *source = nullptr)
    {
        const auto target = source && conversations_.contains(source) ? Target{source, conversations_[source].history} : captureTarget();
        if (!alive(target)) return;
        const auto membership = conversations_[target.key].users;
        auto liveTargets = [&]() {
            QStringList live;
            if (!alive(target) || conversations_[target.key].users != membership) return live;
            for (const QString &nick : nicks) if (membership->users.contains(nick)) live.append(nick);
            return live;
        };
        if (action == NickAction::Friend) {
            const auto live = liveTargets();
            if (!live.isEmpty()) qt_fe_friend_ask(live.first().toUtf8().constData(), nullptr);
            return;
        }
        if (action == NickAction::Details) {
            const auto live = liveTargets();
            if (!live.isEmpty()) showUserDetails(target, live.first());
            return;
        }
        if (action == NickAction::Copy) { QApplication::clipboard()->setText(liveTargets().join(QLatin1Char(' '))); return; }
        if (action == NickAction::Insert) {
            const auto live = liveTargets();
            if (!live.isEmpty() && inputFor(target.key)) { inputFor(target.key)->insert(live.join(QLatin1Char(' ')) + QLatin1Char(' ')); inputFor(target.key)->setFocus(); }
            return;
        }
        QString argument;
        bool accepted = true;
        if (action == NickAction::Kick || action == NickAction::KickBan)
            argument = QInputDialog::getText(this, QStringLiteral("Kick Selected Users"),
                QStringLiteral("Reason:"), QLineEdit::Normal, QStringLiteral("Requested"), &accepted);
        else if (action == NickAction::Invite)
            argument = QInputDialog::getText(this, QStringLiteral("Invite Selected Users"),
                QStringLiteral("Destination channel:"), QLineEdit::Normal, QStringLiteral("#"), &accepted).trimmed();
        QtSessionInfo info{};
        if (!accepted || !alive(target) || !qt_core_session_info(target.key, &info) || !info.channel || !info.connected) return;
        if (argument.contains(QLatin1Char('\r')) || argument.contains(QLatin1Char('\n'))) return;
        if (action == NickAction::Invite && (argument.size() < 2 || argument.contains(QRegularExpression(QStringLiteral("\\s"))))) return;
        QString command;
        switch (action) {
        case NickAction::Operator: command = QStringLiteral("/op "); break;
        case NickAction::Deoperator: command = QStringLiteral("/deop "); break;
        case NickAction::Voice: command = QStringLiteral("/voice "); break;
        case NickAction::Devoice: command = QStringLiteral("/devoice "); break;
        case NickAction::Kick: command = QStringLiteral("/kick "); break;
        case NickAction::Ban: command = QStringLiteral("/ban "); break;
        case NickAction::KickBan: command = QStringLiteral("/kickban "); break;
        case NickAction::Invite: command = QStringLiteral("/invite "); break;
        default: return;
        }
        const QStringList live = liveTargets();
        for (const QString &nick : live) {
            if (!alive(target) || conversations_[target.key].users != membership || !membership->users.contains(nick)) continue;
            submitTo(target, command + nick + (argument.isEmpty() ? QString() : QLatin1Char(' ') + argument));
        }
    }
    void openAddons(const Target &target, bool chooseFile)
    {
        showAddons(this, [guard = QPointer<MainWindow>(this), target]() -> AddonContext {
            if (!guard) return {};
            return {target.key, [guard, target]() { return guard && guard->alive(target); },
                [guard]() { if (guard) guard->restoreEngineFocus(); }};
        }, chooseFile);
    }
    void newConversationWindow(void *source, bool server)
    {
        void *previous = active_;
        qt_core_new_tab(source, server);
        if (active_ && active_ != previous) detachConversation(active_);
    }
    void buildActions()
    {
        // Match the supported entries in fe-gtk/menu.c, in original menu order.
        auto root = [this](const QString &title, const QString &id) {
            auto *menu = menuBar()->addMenu(title); menu->setObjectName(id); return menu;
        };
        auto *zoitechat = root(QStringLiteral("&ZoiteChat"), QStringLiteral("zoitechatMenu"));
        auto *view = root(QStringLiteral("&View"), QStringLiteral("viewMenu"));
        auto *server = root(QStringLiteral("&Server"), QStringLiteral("serverMenu"));
        auto *usermenu = root(QStringLiteral("&Usermenu"), QStringLiteral("userMenu"));
        auto *settings = root(QStringLiteral("S&ettings"), QStringLiteral("settingsMenu"));
        auto *window = root(QStringLiteral("&Window"), QStringLiteral("windowMenu"));
        auto *help = root(QStringLiteral("&Help"), QStringLiteral("helpMenu"));

        auto *networks = addAction(zoitechat, QStringLiteral("Network Li&st"), QStringLiteral("networkList"),
            QKeySequence(QStringLiteral("Ctrl+S")), [this]() { showNetworkManager(this); });
        networks->setShortcuts({QKeySequence(QStringLiteral("Ctrl+S")), QKeySequence(QStringLiteral("Ctrl+Shift+N"))});
        zoitechat->addSeparator();
        auto *newMenu = zoitechat->addMenu(QStringLiteral("&New")); newMenu->setObjectName(QStringLiteral("newMenu"));
        addAction(newMenu, QStringLiteral("Server Tab"), QStringLiteral("newServerTab"), QKeySequence(QStringLiteral("Ctrl+T")),
            [this]() { qt_core_new_tab(active_, true); });
        newChannel_ = addAction(newMenu, QStringLiteral("Channel Tab"), QStringLiteral("newChannelTab"), {},
            [this]() { qt_core_new_tab(active_, false); });
        addAction(newMenu, QStringLiteral("Server Window"), QStringLiteral("newServerWindow"), QKeySequence(QStringLiteral("Ctrl+N")),
            [this]() { newConversationWindow(active_, true); });
        addAction(newMenu, QStringLiteral("Channel Window"), QStringLiteral("newChannelWindow"), {},
            [this]() { newConversationWindow(active_, false); });
        zoitechat->addSeparator();
        auto *loadPlugin = addAction(zoitechat, QStringLiteral("&Load Plugin or Script…"), QStringLiteral("loadPlugin"), {}, [this]() { openAddons(captureTarget(), true); });
        loadPlugin->setEnabled(qt_core_addons_enabled());
        zoitechat->addSeparator();
        detachAction_ = addAction(zoitechat, QStringLiteral("&Detach"), QStringLiteral("detachConversation"), {}, [this]() { detachConversation(active_); });
        closeConversation_ = addAction(zoitechat, QStringLiteral("&Close"), QStringLiteral("closeConversation"),
            QKeySequence(QStringLiteral("Ctrl+W")), [this]() { submitTo(captureTarget(), QStringLiteral("/close")); });
        zoitechat->addSeparator();
        addAction(zoitechat, QStringLiteral("&Quit"), QStringLiteral("quit"), QKeySequence(QStringLiteral("Ctrl+Q")),
            [this]() { requestQuit(); });

        auto check = [this](QMenu *menu, const QString &name, const QString &id, QWidget *widget) {
            auto *action = menu->addAction(name); action->setObjectName(id);
            action->setCheckable(true); action->setChecked(true);
            connect(action, &QAction::toggled, widget, &QWidget::setVisible); return action;
        };
        showMenu_ = check(view, QStringLiteral("&Menu Bar"), QStringLiteral("showMenu"), menuBar());
        showMenu_->setShortcut(QKeySequence(QStringLiteral("Ctrl+F9"))); QMainWindow::addAction(showMenu_);
        showTopic_ = check(view, QStringLiteral("&Topic Bar"), QStringLiteral("showTopic"), topic_);
        showModes_ = view->addAction(QStringLiteral("Mode Buttons")); showModes_->setObjectName(QStringLiteral("showChannelModes")); showModes_->setCheckable(true); showModes_->setChecked(true);
        connect(showModes_, &QAction::toggled, this, [this]() { updateStatus(); });
        showUsers_ = check(view, QStringLiteral("&User List"), QStringLiteral("showUsers"), usersPanel_);
        showUserButtons_ = view->addAction(QStringLiteral("User List Buttons")); showUserButtons_->setObjectName(QStringLiteral("showUserButtons")); showUserButtons_->setCheckable(true);
        showDialogButtons_ = view->addAction(QStringLiteral("Dialog Buttons")); showDialogButtons_->setObjectName(QStringLiteral("showDialogButtons")); showDialogButtons_->setCheckable(true);
        connect(showUserButtons_, &QAction::toggled, this, [this]() { updateStatus(); });
        connect(showDialogButtons_, &QAction::toggled, this, [this]() { updateStatus(); });
        bindNativeVisibility(showMenu_, "gui_hide_menu", true);
        bindNativeVisibility(showTopic_, "gui_topicbar");
        bindNativeVisibility(showUsers_, "gui_ulist_hide", true);
        bindNativeVisibility(showUserButtons_, "gui_ulist_buttons");
        bindNativeVisibility(showModes_, "gui_mode_buttons");
        connect(nicks_, &QListWidget::itemSelectionChanged, this, [this]() { updateStatus(); });
        showUsers_->setShortcut(QKeySequence(QStringLiteral("Ctrl+F7")));
        connect(showUsers_, &QAction::toggled, this, [this]() { updateSwitcherVisibility(); });
        view->removeAction(showModes_); view->removeAction(showDialogButtons_); view->addAction(showModes_);
        auto *switcher = view->addMenu(QStringLiteral("&Channel Switcher")); switcher->setObjectName(QStringLiteral("channelSwitcherMenu"));
        auto *layouts = new QActionGroup(switcher); layouts->setExclusive(true);
        layoutTabs_ = switcher->addAction(QStringLiteral("&Tabs")); layoutTabs_->setObjectName(QStringLiteral("switcherTabs")); layoutTabs_->setCheckable(true); layouts->addAction(layoutTabs_);
        layoutTree_ = switcher->addAction(QStringLiteral("T&ree")); layoutTree_->setObjectName(QStringLiteral("switcherTree")); layoutTree_->setCheckable(true); layouts->addAction(layoutTree_);
        connect(layoutTabs_, &QAction::triggered, this, [this](bool checked) { if (checked) { saveNativeValue("gui_tab_layout", "0"); applyDisplay(); } });
        connect(layoutTree_, &QAction::triggered, this, [this](bool checked) { if (checked) { saveNativeValue("gui_tab_layout", "2"); applyDisplay(); } });
        switcher->addSeparator();
        showChannels_ = switcher->addAction(QStringLiteral("Show Channel Switcher")); showChannels_->setObjectName(QStringLiteral("showChannels")); showChannels_->setCheckable(true); showChannels_->setChecked(true);
        connect(showChannels_, &QAction::toggled, this, [this](bool checked) { if (checked && switcherPosition_ == 7) { saveNativeValue("gui_tab_pos", "1"); applyDisplay(); } else updateSwitcherVisibility(); });
        addMeterMenu(view); view->addSeparator(); view->addAction(showDialogButtons_);
        auto *display = view->addMenu(QStringLiteral("Text Display")); display->setObjectName(QStringLiteral("textDisplayMenu"));
        autoScroll_ = display->addAction(QStringLiteral("Follow New Messages"));
        autoScroll_->setObjectName(QStringLiteral("autoScroll")); autoScroll_->setCheckable(true); autoScroll_->setChecked(true);
        addAction(display, QStringLiteral("Jump to Latest Message"), QStringLiteral("jumpLatest"),
            QKeySequence(QStringLiteral("Ctrl+End")), [this]() { chat_->verticalScrollBar()->setValue(chat_->verticalScrollBar()->maximum()); });
        display->addSeparator();
        addAction(display, QStringLiteral("Larger Chat Text"), QStringLiteral("zoomIn"), QKeySequence::ZoomIn, [this]() { zoomChat(1); });
        addAction(display, QStringLiteral("Smaller Chat Text"), QStringLiteral("zoomOut"), QKeySequence::ZoomOut, [this]() { zoomChat(-1); });

        disconnect_ = addAction(server, QStringLiteral("&Disconnect"), QStringLiteral("disconnect"), {},
            [this]() { submitTo(captureTarget(), QStringLiteral("/discon")); });
        reconnect_ = addAction(server, QStringLiteral("&Reconnect"), QStringLiteral("reconnect"), {},
            [this]() { submitTo(captureTarget(), QStringLiteral("/reconnect")); });
        addAction(server, QStringLiteral("&Join a Channel…"), QStringLiteral("joinChannel"), QKeySequence(QStringLiteral("Ctrl+J")), [this]() {
            const auto target = captureTarget(); bool ok = false;
            const QString channel = QInputDialog::getText(this, QStringLiteral("Join a Channel"), QStringLiteral("Channel:"),
                QLineEdit::Normal, QStringLiteral("#"), &ok).trimmed();
            if (ok && !channel.isEmpty()) submitTo(target, QStringLiteral("/join ") + channel);
        });
        channelBrowser_ = addAction(server, QStringLiteral("Channel &List"), QStringLiteral("channelBrowserAction"),
            QKeySequence(QStringLiteral("Ctrl+Shift+L")), [this]() { qt_fe_channel_browser(qt_core_session_server(active_), "", false); });
        server->addSeparator();
        away_ = addAction(server, QStringLiteral("Marked &Away"), QStringLiteral("away"), QKeySequence(QStringLiteral("Alt+A")), [this]() {
            QtSessionInfo info{};
            if (!qt_core_session_info(active_, &info) || !info.connected) return;
            away_->setChecked(info.away);
            submitTo(captureTarget(), info.away ? QStringLiteral("/back") : QStringLiteral("/away"));
        });
        away_->setCheckable(true);
        server->addSeparator();
        auto *serverMore = server->addMenu(QStringLiteral("More")); serverMore->setObjectName(QStringLiteral("serverMoreMenu"));
        addAction(serverMore, QStringLiteral("&Quick Connect…"), QStringLiteral("quickConnect"), {}, [this]() {
            const auto target = captureTarget(); bool ok = false;
            const QString host = QInputDialog::getText(this, QStringLiteral("Connect to IRC"),
                QStringLiteral("Server hostname (optionally :port):"), QLineEdit::Normal, QStringLiteral("irc.libera.chat"), &ok).trimmed();
            if (ok && !host.isEmpty()) submitTo(target, QStringLiteral("/server ") + host);
        });
        nickname_ = addAction(serverMore, QStringLiteral("Change &Nickname…"), QStringLiteral("nickname"), {}, [this]() {
            changeNickname(captureTarget());
        });
        flushQueue_ = addAction(serverMore, QStringLiteral("Clear Outgoing Queue"), QStringLiteral("flushQueue"), {}, [this]() {
            submitTo(captureTarget(), QStringLiteral("/flushq"));
        });
        awayReason_ = addAction(serverMore, QStringLiteral("Away Message…"), QStringLiteral("awayReason"), {}, [this]() {
            promptCommand(QStringLiteral("Set Away"), QStringLiteral("Away message:"), QStringLiteral("/away "), QStringLiteral("Away"));
        });

        addAction(settings, QStringLiteral("&Preferences"), QStringLiteral("preferences"), {}, [this]() { displayPreferences(); });
        settings->addSeparator();
        addAction(settings, QStringLiteral("Auto Replace"), QStringLiteral("autoReplaceEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_REPLACE, this); });
        addAction(settings, QStringLiteral("CTCP Replies"), QStringLiteral("ctcpEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_CTCP, this); });
        addAction(settings, QStringLiteral("Dialog Buttons"), QStringLiteral("dialogButtonsEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_DIALOG_BUTTONS, this); });
        addAction(settings, QStringLiteral("Keyboard Shortcuts"), QStringLiteral("keyBindingsEditorAction"), {}, [this]() { keyBindings_->edit(this); });
        addAction(settings, QStringLiteral("Text Events"), QStringLiteral("textEventsEditor"), {}, [this]() { showTextEvents(this); });
        addAction(settings, QStringLiteral("URL Handlers"), QStringLiteral("urlHandlersEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_URL_HANDLERS, this); });
        addAction(settings, QStringLiteral("User Commands"), QStringLiteral("commandsEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_COMMANDS, this); });
        addAction(settings, QStringLiteral("User List Buttons"), QStringLiteral("userButtonsEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_BUTTONS, this); });
        addAction(settings, QStringLiteral("User List Popup"), QStringLiteral("userPopupEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_POPUP, this); });
        addAction(settings, QStringLiteral("Channel Tab Popup"), QStringLiteral("tabPopupEditor"), {}, [this]() { showConfigEditor(QT_CONFIG_TAB_POPUP, this); });
        settings->addSeparator();
        auto *userSettings = settings->addMenu(QStringLiteral("User List")); userSettings->setObjectName(QStringLiteral("userListSettingsMenu"));
        rankUsers_ = userSettings->addAction(QStringLiteral("Sort Users by Privilege"));
        rankUsers_->setObjectName(QStringLiteral("rankUsers")); rankUsers_->setCheckable(true);
        connect(rankUsers_, &QAction::toggled, this, [this](bool enabled) {
            for (int i = 0; i < qt_core_preference_count(); ++i) {
                QtPreference pref{}; if (!qt_core_preference_read(i, &pref) || QByteArray(pref.key) != "gui_ulist_sort") continue;
                QtPreferenceChange change{}; change.index = i;
                qstrncpy(change.expected, pref.value, sizeof(change.expected)); qstrncpy(change.value, enabled ? "0" : "1", sizeof(change.value));
                char error[512]{};
                if (!qt_core_preferences_apply(&change, 1, error, sizeof(error))) statusBar()->showMessage(QString::fromUtf8(error), 8000);
                applyDisplay(); break;
            }
        });
        auto *desktopMenu = settings->addMenu(QStringLiteral("Desktop")); desktopMenu->setObjectName(QStringLiteral("desktopSettingsMenu"));
        auto desktopOption = [this, desktopMenu](const QString &text, const QString &id) {
            auto *action = desktopMenu->addAction(text); action->setObjectName(id); action->setCheckable(true);
            connect(action, &QAction::toggled, this, [this, id](bool checked) {
                if (id == QStringLiteral("trayEnabled")) saveNativeBoolean("gui_tray", checked);
                else if (id == QStringLiteral("closeToTray")) saveNativeBoolean("gui_tray_close", checked);
                configureDesktop();
            }); return action;
        };
        trayEnabled_ = desktopOption(QStringLiteral("Show Tray Icon"), QStringLiteral("trayEnabled"));
        notifications_ = desktopOption(QStringLiteral("Enable Desktop Notifications"), QStringLiteral("notifications"));
        previews_ = desktopOption(QStringLiteral("Include Message Previews"), QStringLiteral("notificationPreviews"));
        closeToTray_ = desktopOption(QStringLiteral("Close Window to Tray"), QStringLiteral("closeToTray"));
        desktopMenu->addSeparator();
        addAction(desktopMenu, QStringLiteral("Show / Hide Window"), QStringLiteral("toggleWindow"), {}, [this]() { desktop_->toggleWindow(); });

        banManager_ = addAction(window, QStringLiteral("&Ban List"), QStringLiteral("banManagerAction"), {}, [this]() { showBanManager(active_, this); });
        addAction(window, QStringLiteral("Character Chart"), QStringLiteral("characterChartAction"), {}, [this]() {
            showCharacterChart(this, [guard = QPointer<MainWindow>(this)](const QString &text) {
                if (!guard || !guard->conversations_.contains(guard->active_)) return false;
                guard->input_->insert(text); return true;
            });
        });
        addAction(window, QStringLiteral("Direct Chat"), QStringLiteral("directChatsAction"), {}, []() { showDirectChats(); });
        addAction(window, QStringLiteral("File Transfers"), QStringLiteral("transfersAction"), {}, []() { showTransfers(); });
        addAction(window, QStringLiteral("Friends List"), QStringLiteral("friendsAction"), {}, []() { showFriendsList(); });
        addAction(window, QStringLiteral("Ignore List"), QStringLiteral("ignoreManagerAction"), {}, []() { showIgnoreManager(); });
        addAction(window, QStringLiteral("Plugins and Scripts"), QStringLiteral("addonsAction"), {}, [this]() { openAddons(captureTarget(), false); });
        rawLog_ = addAction(window, QStringLiteral("&Raw Log"), QStringLiteral("rawLogAction"), {}, [this]() { showRawLog(qt_core_session_server(active_)); });
        addAction(window, QStringLiteral("&URL Grabber"), QStringLiteral("urlHistoryAction"), {}, []() { showUrlHistory(); });
        window->addSeparator();
        addAction(window, QStringLiteral("Reset Marker Line"), QStringLiteral("resetMarker"), QKeySequence(QStringLiteral("Ctrl+M")),
            [this]() { resetMarker(); });
        addAction(window, QStringLiteral("Move to Marker Line"), QStringLiteral("moveToMarker"), QKeySequence(QStringLiteral("Ctrl+Shift+M")),
            [this]() { moveToMarker(); });
        addAction(window, QStringLiteral("&Copy Selection"), QStringLiteral("copyChat"), QKeySequence(QStringLiteral("Ctrl+Shift+C")), [this]() { chat_->copy(); });
        addAction(window, QStringLiteral("C&lear Text"), QStringLiteral("clearChat"), QKeySequence(QStringLiteral("Ctrl+L")), [this]() { clear(active_); });
        addAction(window, QStringLiteral("Save Text…"), QStringLiteral("saveChat"), {}, [this]() { saveChat(); });
        auto *search = window->addMenu(QStringLiteral("Search")); search->setObjectName(QStringLiteral("searchMenu"));
        addAction(search, QStringLiteral("Search Text…"), QStringLiteral("find"), QKeySequence(QStringLiteral("Ctrl+F")), [this]() { showSearch(); });
        auto *next = addAction(search, QStringLiteral("Search Next"), QStringLiteral("findNext"), QKeySequence(QStringLiteral("Ctrl+G")), [this]() { findChat(false); });
        next->setShortcuts({QKeySequence(QStringLiteral("Ctrl+G")), QKeySequence(Qt::Key_F3)});
        auto *previous = addAction(search, QStringLiteral("Search Previous"), QStringLiteral("findPrevious"), QKeySequence(QStringLiteral("Ctrl+Shift+G")), [this]() { findChat(true); });
        previous->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Shift+G")), QKeySequence(QStringLiteral("Shift+F3"))});
        window->addSeparator();
        auto *channel = window->addMenu(QStringLiteral("Channel")); channel->setObjectName(QStringLiteral("channelMenu"));
        conversationOptions_ = addAction(channel, QStringLiteral("Conversation Options…"), QStringLiteral("conversationOptionsAction"), {}, [this]() { displayConversationOptions(); });
        topicAction_ = addAction(channel, QStringLiteral("Edit &Topic…"), QStringLiteral("editTopic"), {}, [this]() { editTopic(captureTarget()); });
        channelSettings_ = addAction(channel, QStringLiteral("Channel &Settings…"), QStringLiteral("channelSettingsAction"), {}, [this]() { showChannelSettings(active_, this); });
        inviteUser_ = addAction(channel, QStringLiteral("&Invite User…"), QStringLiteral("inviteUserAction"), {}, [this]() {
            const auto target = captureTarget();
            if (!alive(target)) return;
            const auto membership = conversations_[target.key].users;
            bool accepted = false;
            const QString nick = QInputDialog::getText(this, QStringLiteral("Invite User"), QStringLiteral("Nickname:"),
                QLineEdit::Normal, QString(), &accepted).trimmed();
            QtSessionInfo info{};
            if (accepted && alive(target) && conversations_[target.key].users == membership &&
                qt_core_session_info(target.key, &info) && info.connected && info.channel && !nick.isEmpty() &&
                !nick.contains(QRegularExpression(QStringLiteral("\\s")))) submitTo(target, QStringLiteral("/invite ") + nick);
        });
        leave_ = addAction(channel, QStringLiteral("&Leave Channel"), QStringLiteral("leave"), {}, [this]() { submitTo(captureTarget(), QStringLiteral("/close")); });
        auto *navigation = window->addMenu(QStringLiteral("Navigation")); navigation->setObjectName(QStringLiteral("navigationMenu"));
        addAction(navigation, QStringLiteral("Previous Conversation"), QStringLiteral("previousConversation"), QKeySequence(QStringLiteral("Ctrl+PgUp")), [this]() { switchConversation(-1); });
        addAction(navigation, QStringLiteral("Next Conversation"), QStringLiteral("nextConversation"), QKeySequence(QStringLiteral("Ctrl+PgDown")), [this]() { switchConversation(1); });
        addAction(navigation, QStringLiteral("Next Unread Conversation"), QStringLiteral("nextUnread"), QKeySequence(QStringLiteral("Alt+`")), [this]() { switchConversation(1, true); });
        addAction(navigation, QStringLiteral("Focus Message Input"), QStringLiteral("focusInput"), QKeySequence(QStringLiteral("Alt+I")), [this]() { input_->setFocus(); });
        addAction(window, QStringLiteral("Select All Text"), QStringLiteral("selectChat"), {}, [this]() { chat_->selectAll(); });
        for (int i = 1; i <= 9; ++i) {
            auto *action = new QAction(this);
            action->setShortcut(QKeySequence(QStringLiteral("Alt+") + QString::number(i)));
            QMainWindow::addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() {
                const auto rows = switcherItems(); if (i <= rows.size()) select(keyForItem(rows[i - 1]));
            });
        }
        channels_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(channels_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
            auto *item = point.x() < 0 ? channels_->currentItem() : channels_->itemAt(point);
            showConversationPopup(keyForItem(item), point.x() < 0 ? channels_->mapToGlobal(channels_->rect().center()) : channels_->viewport()->mapToGlobal(point));
        });

        addAction(help, QStringLiteral("&About"), QStringLiteral("about"), {}, [this]() {
            QMessageBox::about(this, QStringLiteral("ZoiteChat"), QStringLiteral(
                "ZoiteChat\nExperimental Qt 6 frontend using the ZoiteChat IRC engine.\n\n"
                "Use the menus to see keyboard shortcuts.\nLicensed under GPL version 2 or later."));
        });
        customMenus_ = new CustomMenus(menuBar(), usermenu, this,
            [this](const QString &command) {
                const auto target = captureTarget();
                if (alive(target)) qt_core_custom_run(target.key, command.toUtf8().constData());
                restoreEngineFocus();
            }, [this](void *identity, const QString &command, bool checked) {
                const auto target = captureTarget();
                if (alive(target)) qt_core_menu_activate(target.key, identity, command.toUtf8().constData(), checked);
                restoreEngineFocus();
            });
    }

    void buildSearch(QWidget *parent, QVBoxLayout *layout)
    {
        searchBar_ = new QWidget(parent);
        auto *searchLayout = new QVBoxLayout(searchBar_);
        searchLayout->setContentsMargins(0, 0, 0, 0);
        auto *queryRow = new QHBoxLayout;
        auto *row = new QHBoxLayout;
        searchLayout->addLayout(queryRow); searchLayout->addLayout(row);
        search_ = new QLineEdit(searchBar_);
        search_->setObjectName(QStringLiteral("chatSearch"));
        search_->setPlaceholderText(QStringLiteral("Find in this conversation"));
        matchCase_ = new QCheckBox(QStringLiteral("Match case"), searchBar_);
        matchCase_->setObjectName(QStringLiteral("matchCase"));
        regexSearch_ = new QCheckBox(QStringLiteral("Regex"), searchBar_);
        regexSearch_->setObjectName(QStringLiteral("regexSearch"));
        regexSearch_->setToolTip(QStringLiteral("Interpret the search as a regular expression"));
        auto *results = new QPushButton(QStringLiteral("Show Matching Lines"), searchBar_);
        results->setObjectName(QStringLiteral("showMatchingLines"));
        connect(results, &QPushButton::clicked, this, [this]() {
            if (search_->text().isEmpty()) { searchResult_->setText(QStringLiteral("Enter a search string")); return; }
            QString options = QStringLiteral("/lastlog -h ");
            if (matchCase_->isChecked()) options += QStringLiteral("-m ");
            if (regexSearch_->isChecked()) options += QStringLiteral("-r ");
            submitTo(captureTarget(), options + QStringLiteral("-- ") + search_->text());
        });
        auto *previous = new QPushButton(QStringLiteral("Previous"), searchBar_);
        auto *next = new QPushButton(QStringLiteral("Next"), searchBar_);
        auto *close = new QPushButton(QStringLiteral("Close"), searchBar_);
        searchResult_ = new QLabel(searchBar_);
        searchResult_->setObjectName(QStringLiteral("searchResult"));
        queryRow->addWidget(search_, 1);
        row->addWidget(matchCase_);
        row->addWidget(regexSearch_);
        row->addWidget(results);
        queryRow->addWidget(previous);
        queryRow->addWidget(next);
        row->addStretch(); row->addWidget(searchResult_);
        queryRow->addWidget(close);
        layout->addWidget(searchBar_);
        searchBar_->hide();
        connect(search_, &QLineEdit::returnPressed, this, [this]() { findChat(false); });
        connect(search_, &QLineEdit::textChanged, searchResult_, &QLabel::clear);
        connect(previous, &QPushButton::clicked, this, [this]() { findChat(true); });
        connect(next, &QPushButton::clicked, this, [this]() { findChat(false); });
        auto hideSearch = [this]() { searchBar_->hide(); input_->setFocus(); };
        connect(close, &QPushButton::clicked, this, hideSearch);
        auto *escape = new QAction(searchBar_);
        escape->setShortcut(QKeySequence(Qt::Key_Escape));
        escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        searchBar_->addAction(escape);
        connect(escape, &QAction::triggered, this, hideSearch);
    }
    void showSearch()
    {
        searchBar_->show();
        search_->setFocus();
        search_->selectAll();
    }
    void findChat(bool backwards)
    {
        if (!conversations_.contains(active_)) return;
        if (search_->text().isEmpty()) { showSearch(); return; }
        searchResult_->setText(searchTranscript(chat_, search_->text(), backwards, matchCase_->isChecked(), regexSearch_->isChecked()));
    }
    void saveChat() { saveConversation(captureTarget(), this); }
    void saveConversation(const Target &target, QWidget *parent)
    {
        if (!alive(target)) return;
        // Capture text before opening the dialog; switching/closing tabs cannot change the export.
        const QByteArray text = conversations_[target.key].document->toPlainText().toUtf8();
        const QString file = QFileDialog::getSaveFileName(parent, QStringLiteral("Save Text"),
            QStringLiteral("chat.txt"), QStringLiteral("Text files (*.txt);;All files (*)"));
        if (file.isEmpty()) return;
        QSaveFile output(file);
        if (!output.open(QIODevice::WriteOnly) || output.write(text) != text.size() || !output.commit())
            QMessageBox::warning(parent, QStringLiteral("Save Text"), output.errorString());
        else statusBar()->showMessage(QStringLiteral("Chat saved"), 4000);
    }
    void updateStatus()
    {
        QtSessionInfo info{};
        const bool valid = active_ && qt_core_session_info(active_, &info);
        const bool connected = valid && info.connected;
        refreshNickname(active_, inputNickname_);
        updateTitle();
        QtConversationOptions options{};
        conversationOptions_->setEnabled(valid && qt_core_conversation_options(active_, &options));
        refreshModes(active_, channelModes_, modeButtons_, channelKey_, channelLimit_, showModes_ && showModes_->isChecked());
        updateCommandButtons(active_, nicks_, userButtons_, dialogButtons_, showUserButtons_ && showUserButtons_->isChecked(), showDialogButtons_ && showDialogButtons_->isChecked());
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it) refreshDetachedControls(it.key());
        topic_->setReadOnly(!(connected && info.channel));
        topic_->setPlaceholderText(connected && info.channel ? QStringLiteral("No topic set — type a topic and press Enter") : QStringLiteral("No topic set"));
        channelSettings_->setEnabled(connected && info.channel);
        banManager_->setEnabled(connected && info.channel);
        inviteUser_->setEnabled(connected && info.channel);
        nicks_->setModerationEnabled(connected && info.channel);
        channelBrowser_->setEnabled(connected);
        rawLog_->setEnabled(valid);
        topicAction_->setEnabled(connected && info.channel);
        leave_->setEnabled(connected && info.channel);
        detachAction_->setEnabled(active_ && conversations_.contains(active_));
        closeConversation_->setEnabled(valid);
        newChannel_->setEnabled(valid);
        disconnect_->setEnabled(valid && (connected || info.connecting || info.reconnect_wait));
        reconnect_->setEnabled(valid && info.host && *info.host);
        nickname_->setEnabled(valid);
        away_->setEnabled(connected);
        away_->setChecked(connected && info.away);
        awayReason_->setEnabled(connected);
        if (!connection_) return;
        connectionProgress_->setVisible(valid && (info.connecting || info.registering));
        connectionProgress_->setToolTip(info.registering ? QStringLiteral("Waiting for server registration to finish") : QStringLiteral("Connecting to the server"));
        refreshMeters(info, valid, lagStatus_, queueStatus_, lagMeter_, queueMeter_);
        flushQueue_->setEnabled(valid && info.queued_bytes > 0);
        if (!valid) { connection_->clear(); return; }
        const QString state = info.reconnect_wait ? QStringLiteral("Waiting to reconnect") :
            info.connecting ? QStringLiteral("Connecting") : info.registering ? QStringLiteral("Registering") :
            connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected");
        connection_->setText(QStringLiteral("%1 · %2 · %3%4").arg(state,
            QString::fromUtf8(info.host ? info.host : ""), QString::fromUtf8(info.nick ? info.nick : ""),
            info.away ? QStringLiteral(" · Away") : QString()));
        connection_->setToolTip(connection_->text());
    }
    void *tabKey(int index) const { return index >= 0 && index < tabs_->count() ? reinterpret_cast<void *>(tabs_->tabData(index).value<quintptr>()) : nullptr; }
    void syncTabSelection()
    {
        const QSignalBlocker blocker(tabs_);
        for (int i = 0; i < tabs_->count(); ++i) {
            auto *item = items_.value(tabKey(i));
            if (item) tabs_->setTabTextColor(i, conversations_.value(tabKey(i)).activity > 0 ? item->foreground(0).color() : tabs_->palette().color(QPalette::WindowText));
            if (tabKey(i) == active_) tabs_->setCurrentIndex(i);
        }
    }
    void updateSwitcherVisibility()
    {
        if (!showChannels_ || !showUsers_) return;
        const bool showSwitcher = showChannels_->isChecked() && switcherPosition_ != 7;
        channels_->setVisible(showSwitcher && switcherLayout_ == 2);
        tabs_->setVisible(showSwitcher && switcherLayout_ != 2);
        usersPanel_->setVisible(showUsers_->isChecked());
        auto visibleChild = [](QSplitter *dock) { for (int i = 0; i < dock->count(); ++i) if (!dock->widget(i)->isHidden()) return true; return false; };
        leftDock_->setVisible(visibleChild(leftDock_)); rightDock_->setVisible(visibleChild(rightDock_));
    }
    void placeConversationPanes()
    {
        int position = qBound(1, switcherPosition_, 7), users = qBound(1, userListPosition_, 4);
        if (switcherLayout_ == 2 && (position == 5 || position == 6)) position = 1;
        if (position == users) position = users == 3 ? 4 : 3;
        QWidget *switcher = switcherLayout_ == 2 ? static_cast<QWidget *>(channels_) : static_cast<QWidget *>(tabs_);
        const bool horizontal = switcherLayout_ != 2 && (position == 5 || position == 6);
        const auto shape = horizontal ? (position == 5 ? QTabBar::RoundedNorth : QTabBar::RoundedSouth) : position <= 2 ? QTabBar::RoundedWest : QTabBar::RoundedEast;
        tabs_->setShape(shape); tabs_->setMinimumWidth(horizontal ? 0 : 130); tabs_->setMaximumWidth(horizontal ? QWIDGETSIZE_MAX : 340);
        const bool wasHiddenPosition = placedPosition_ == 7;
        const bool same = placedSwitcher_ == switcher && placedPosition_ == position && placedUsers_ == users;
        if (!same) {
            // Widgets remain alive while moving between layouts and splitters.
            middleLayout_->removeWidget(tabs_);
            channels_->setParent(centralWidget()); tabs_->setParent(centralWidget()); usersPanel_->setParent(centralWidget());
            QList<QPair<int, QWidget *>> panes{{users, usersPanel_}};
            if (position <= 4) panes.append({position, switcher});
            std::sort(panes.begin(), panes.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
            for (const auto &pane : panes) (pane.first <= 2 ? leftDock_ : rightDock_)->addWidget(pane.second);
            if (horizontal) { tabs_->setParent(middle_); middleLayout_->insertWidget(position == 5 ? 0 : middleLayout_->count(), tabs_); }
            placedSwitcher_ = switcher; placedPosition_ = position; placedUsers_ = users;
        }
        if (position == 7 || wasHiddenPosition) { const QSignalBlocker blocker(showChannels_); showChannels_->setChecked(position != 7); }
        updateSwitcherVisibility();
    }
    void refreshTabs()
    {
        if (!tabs_) return;
        const QSignalBlocker blocker(tabs_); const auto rows = switcherItems();
        bool aligned = tabs_->count() == rows.size();
        for (int i = 0; aligned && i < rows.size(); ++i) aligned = tabKey(i) == keyForItem(rows[i]);
        if (!aligned) {
            while (tabs_->count()) tabs_->removeTab(0);
            for (auto *item : rows) {
                const int index = tabs_->addTab(item->text(0));
                tabs_->setTabData(index, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(keyForItem(item))));
            }
        }
        for (int i = 0; i < rows.size(); ++i) { tabs_->setTabText(i, QString(rows[i]->text(0)).replace(QLatin1Char('&'), QStringLiteral("&&"))); tabs_->setTabToolTip(i, rows[i]->toolTip(0)); }
        syncTabSelection();
    }
    void showConversationPopup(void *key, const QPoint &global)
    {
        if (!conversations_.contains(key)) return;
        select(key); const Target target{key, conversations_[key].history}; const QPointer<MainWindow> guard(this);
        QMenu menu;
        auto *topic = menu.addAction(QStringLiteral("Edit Topic…"));
        auto *detach = menu.addAction(detached_.contains(key) ? QStringLiteral("Attach Conversation") : QStringLiteral("Detach Conversation"));
        auto *leave = menu.addAction(QStringLiteral("Leave Channel"));
        const auto context = popupContext(target, {conversations_[key].name}, conversations_[key].users);
        populateCustomPopup(&menu, QStringLiteral("$TAB"), QT_CONFIG_TAB_POPUP, context);
        QtSessionInfo info{}; qt_core_session_info(key, &info);
        if (info.channel) populateCustomPopup(&menu, QStringLiteral("$CHAN"), -1, context);
        if (!guard) return;
        auto *close = menu.addAction(QStringLiteral("Close Conversation"));
        topic->setEnabled(info.connected && info.channel); leave->setEnabled(info.connected && info.channel);
        auto *chosen = menu.exec(global);
        if (!guard || !alive(target)) return;
        if (chosen == detach) { if (detached_.contains(key)) attachConversation(key); else detachConversation(key); }
        else if (chosen == topic) editTopic(target);
        else if (chosen == leave || chosen == close) submitTo(target, QStringLiteral("/close"));
    }
    QList<QTreeWidgetItem *> switcherItems() const
    {
        QList<QTreeWidgetItem *> rows;
        std::function<void(QTreeWidgetItem *)> append = [&](QTreeWidgetItem *item) {
            rows.append(item); for (int i = 0; i < item->childCount(); ++i) append(item->child(i));
        };
        for (int i = 0; i < channels_->topLevelItemCount(); ++i) append(channels_->topLevelItem(i));
        return rows;
    }
    void rebuildSwitcher(const QList<QTreeWidgetItem *> &rows)
    {
        const QSignalBlocker blocker(channels_);
        auto *current = channels_->currentItem();
        QMap<QTreeWidgetItem *, bool> expanded;
        const auto previous = switcherItems();
        for (auto *item : previous) if (item->childCount()) expanded.insert(item, item->isExpanded());
        // Detach children before deleting or reparenting any family root.
        for (auto *item : previous) item->takeChildren();
        while (channels_->topLevelItemCount()) channels_->takeTopLevelItem(0);
        QList<void *> order; QMap<void *, QList<QTreeWidgetItem *>> families;
        for (auto *item : rows) {
            void *key = keyForItem(item), *server = qt_core_session_server(key); if (!server) server = key;
            if (!families.contains(server)) order.append(server);
            families[server].append(item);
        }
        for (void *server : order) {
            auto family = families.value(server); auto *root = family.takeFirst();
            if (sortConversations_) std::stable_sort(family.begin(), family.end(), [this](QTreeWidgetItem *a, QTreeWidgetItem *b) {
                const auto ak = keyForItem(a), bk = keyForItem(b); QtSessionInfo ai{}, bi{};
                qt_core_session_info(ak, &ai); qt_core_session_info(bk, &bi);
                const int ar = ai.server ? 0 : ai.query ? 2 : 1, br = bi.server ? 0 : bi.query ? 2 : 1;
                if (ar != br) return ar < br;
                return conversations_.value(ak).name.toUtf8().toLower().compare(conversations_.value(bk).name.toUtf8().toLower()) < 0;
            });
            channels_->addTopLevelItem(root); root->addChildren(family);
            root->setExpanded(expanded.value(root, true));
        }
        if (rows.contains(current)) channels_->setCurrentItem(current);
        refreshTabs();
    }
    void applySwitcherAppearance()
    {
        auto scaled = [this](QFont font) {
            const qreal factor = switcherSmall_ == 1 ? 0.8333333333333 : switcherSmall_ > 1 ? 0.6944444444444 : 1.0;
            if (font.pointSizeF() > 0) font.setPointSizeF(qMax(qreal(1), font.pointSizeF() * factor));
            else if (font.pixelSize() > 0) font.setPixelSize(qMax(1, qRound(font.pixelSize() * factor)));
            return font;
        };
        channels_->setFont(scaled(switcherNativeStyle_ ? chatFont_ : QApplication::font(channels_)));
        channels_->setPalette(switcherNativeStyle_ ? ircDisplayPalette(QApplication::palette(channels_)) : QApplication::palette(channels_));
        channels_->setTreeLines(switcherLines_);
        tabs_->setFont(scaled(QApplication::font(tabs_)));
        for (auto it = items_.cbegin(); it != items_.cend(); ++it) {
            QFont font = channels_->font(); font.setBold(conversations_.value(it.key()).activity > 0); it.value()->setFont(0, font);
            const int level = conversations_.value(it.key()).activity;
            it.value()->setForeground(0, level ? ircPaletteColor(level == 3 ? 38 : level == 2 ? 39 : 37, channels_->palette()) : channels_->palette().color(QPalette::Text));
            QtSessionInfo info{}; QIcon icon;
            if (switcherIcons_ && qt_core_session_info(it.key(), &info)) {
                const QString name = info.server ? QStringLiteral("server") : info.channel || info.channel_session ? QStringLiteral("channel") : QStringLiteral("dialog");
                icon = QIcon(QStringLiteral(":/zoitechat/tree_%1.png").arg(name));
            }
            it.value()->setIcon(0, icon);
        }
    }
    void refreshSwitcherLabels()
    {
        for (auto it = items_.cbegin(); it != items_.cend(); ++it) {
            const QString name = conversations_.value(it.key()).name;
            int end = 0;
            for (int count = 0; end < name.size() && count < switcherTruncation_; ++count) {
                if (name[end].isHighSurrogate() && end + 1 < name.size() && name[end + 1].isLowSurrogate()) end += 2;
                else ++end;
            }
            it.value()->setText(0, switcherTruncation_ > 2 && end < name.size() ? name.left(end) + QStringLiteral("..") : name);
            it.value()->setToolTip(0, Qt::convertFromPlainText(name));
        }
        refreshTabs();
    }
    void configureConversationView(ChatTranscript *chat, ChatInput *input, NickList *users, QPushButton *nickname)
    {
        auto palette = ircDisplayPalette(QApplication::palette(chat));
        if (!backgroundTexture_.isNull()) palette.setBrush(QPalette::Base, QBrush(palette.color(QPalette::Base), backgroundTexture_));
        chat->setPalette(palette);
        chat->setFont(chatFont_);
        chat->setLineWrapMode(wrap_ ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
        users->setSpacing(visualOptions_.value(QStringLiteral("compactLists")) ? 0 : 2);
        const QString commandPrefix = QString::fromUtf8(qt_core_command_prefix());
        input->setCommandPrefix(commandPrefix);
        input->setPlaceholderText(commandPrefix.isEmpty() ? QStringLiteral("Message") : QStringLiteral("Message or %1command").arg(commandPrefix));
        int amount = 5, order = 0, maxIndent = 256, userSort = 0; bool coloredNicks = false, indent = false, spelling = false, inputStyle = false, userStyle = false, nickBox = false, userIcons = false, userHosts = false; QString spellingLanguages, completionSuffix;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{};
            if (!qt_core_preference_read(i, &pref)) continue;
            if (QByteArray(pref.key) == "completion_amount") amount = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "completion_suffix") completionSuffix = QString::fromUtf8(pref.value);
            else if (QByteArray(pref.key) == "completion_sort") order = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_ulist_color") coloredNicks = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_indent") indent = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_max_indent") maxIndent = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_input_attr") input->setIrcRendering(QByteArray(pref.value).toInt() != 0);
            else if (QByteArray(pref.key) == "gui_input_style") inputStyle = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_ulist_style") userStyle = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_show_marker") showMarker_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_win_nick") titleNick_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_win_modes") titleModes_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_win_ucount") titleUsers_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_autocopy_text") chat->autoCopy = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_autocopy_stamp") chat->copyTimestamps = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_autocopy_color") chat->copyColors = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_tab_icons") switcherIcons_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_tab_dots") switcherLines_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_tab_small") switcherSmall_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_tab_pos") switcherPosition_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_ulist_pos") userListPosition_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_tab_layout") switcherLayout_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_tab_sort") sortConversations_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_tab_newtofront") newConversationFocus_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_tab_trunc") switcherTruncation_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_tab_scrollchans") wheelSwitches_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_tab_middleclose") middleCloses_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_ulist_count") showUserCount_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_lagometer") lagMeterMode_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_throttlemeter") queueMeterMode_ = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_ulist_sort") userSort = QByteArray(pref.value).toInt();
            else if (QByteArray(pref.key) == "gui_ulist_icons") userIcons = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_ulist_show_hosts") userHosts = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_input_icon") inputIcon_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_input_nick") nickBox = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_input_spell") spelling = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "text_spell_langs") spellingLanguages = QString::fromUtf8(pref.value);
        }
        switcherNativeStyle_ = userStyle;
        input->setFont(inputStyle ? chatFont_ : QApplication::font(input));
        input->setPalette(inputStyle ? ircDisplayPalette(QApplication::palette(input)) : QApplication::palette(input));
        users->setFont(userStyle ? chatFont_ : QApplication::font(users));
        users->setPalette(userStyle ? ircDisplayPalette(QApplication::palette(users)) : QApplication::palette(users));
        users->setAwayColor(ircPaletteColor(40, users->palette()));
        indentWidth_ = indent ? qMax(0, maxIndent) : 0;
        if (coloredNicks) users->setNicknameColors([fallback = users->palette()](const QString &nick) {
            const int color = qt_core_nick_color(nick.toUtf8().constData());
            return color < 0 ? fallback.color(QPalette::Text) : ircPaletteColor(color, fallback);
        });
        else users->setNicknameColors({});
        users->setDisplayOptions(userIcons, userHosts);
        users->setSortMode(userSort);
        { const QSignalBlocker blocker(rankUsers_); rankUsers_->setChecked(userSort == 0 || userSort == 2); }
        spellChecker_->configure(spelling, spellingLanguages);
        input->setSpellChecker(spellChecker_, spelling, ircPaletteColor(41, input->palette()));
        input->setToolTip(spelling ? spellChecker_->status() : QString());
        input->setCompletionOptions(visualOptions_.value(QStringLiteral("nickCompletion")), completionSuffix);
        input->setCompletionThreshold(amount, order);
        input->setAutomaticReplacement(visualOptions_.value(QStringLiteral("autoReplace")));
        nickname->setVisible(nickBox);
    }
    void reloadBackground()
    {
        QString error;
        if (!loadChatBackground(backgroundPath_, backgroundImage_, error)) statusBar()->showMessage(error, 8000);
    }
    void applyDisplay()
    {
        bool showUsermenu = false;
        QHash<QByteArray, bool> nativeVisibility;
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{};
            if (!qt_core_preference_read(i, &pref)) continue;
            nativeVisibility.insert(pref.key, QByteArray(pref.value).toInt() != 0);
            if (QByteArray(pref.key) == "text_stripcolor_topic") stripTopics_ = QByteArray(pref.value).toInt() != 0;
            else if (QByteArray(pref.key) == "gui_usermenu") showUsermenu = QByteArray(pref.value).toInt() != 0;
        }
        auto updateUsermenu = [showUsermenu](QMenuBar *bar) {
            for (auto *action : bar->actions())
                if (action->menu() && action->menu()->objectName() == QStringLiteral("userMenu")) action->setVisible(showUsermenu);
        };
        updateUsermenu(menuBar());
        for (auto *view : detached_) updateUsermenu(view->menuBar());
        auto applyVisibility = [&nativeVisibility](QAction *action, const char *key, bool inverted = false) {
            if (action && nativeVisibility.contains(key)) action->setChecked(nativeVisibility.value(key) != inverted);
        };
        applyVisibility(showMenu_, "gui_hide_menu", true);
        applyVisibility(showTopic_, "gui_topicbar");
        applyVisibility(showUsers_, "gui_ulist_hide", true);
        applyVisibility(showUserButtons_, "gui_ulist_buttons");
        applyVisibility(showModes_, "gui_mode_buttons");
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it) {
            auto *view = it.value();
            applyVisibility(view->menuVisible, "gui_hide_menu", true);
            applyVisibility(view->findChild<QAction *>(QStringLiteral("detachedShowTopic")), "gui_topicbar");
            applyVisibility(view->findChild<QAction *>(QStringLiteral("detachedShowUsers")), "gui_ulist_hide", true);
            applyVisibility(view->showUserButtons, "gui_ulist_buttons");
            applyVisibility(view->showModes, "gui_mode_buttons");
            refreshDetachedControls(it.key());
        }
        if (conversations_.contains(active_)) setTopicText(topic_, conversations_[active_].topic);
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it)
            if (conversations_.contains(it.key())) setTopicText(it.value()->topic, conversations_[it.key()].topic);
        backgroundTexture_ = {};
        if (!backgroundImage_.isNull()) {
            QImage composite(backgroundImage_.size(), QImage::Format_RGB32);
            composite.fill(ircDisplayPalette(QApplication::palette(chat_)).color(QPalette::Base));
            QPainter painter(&composite); painter.drawImage(0, 0, backgroundImage_); painter.end();
            backgroundTexture_ = QPixmap::fromImage(composite);
        }
        configureConversationView(chat_, input_, nicks_, inputNickname_);
        setWindowOpacity(opacity_ / 100.0);
        channels_->setUniformRowHeights(visualOptions_.value(QStringLiteral("compactLists")));
        for (auto *view : detached_) {
            configureConversationView(view->chat, view->input, view->users, view->nickname);
            view->setWindowOpacity(opacity_ / 100.0);
        }
        for (auto &conversation : conversations_) {
            conversation.document->setDefaultFont(chatFont_);
            conversation.document->setMaximumBlockCount(scrollback_);
            setChatIndentation(conversation.document, indentWidth_);
        }
        refreshNickname(active_, inputNickname_);
        for (auto it = detached_.cbegin(); it != detached_.cend(); ++it) refreshNickname(it.key(), it.value()->nickname);
        { const QSignalBlocker tabsBlock(layoutTabs_), treeBlock(layoutTree_); layoutTabs_->setChecked(switcherLayout_ != 2); layoutTree_->setChecked(switcherLayout_ == 2); }
        applySwitcherAppearance(); placeConversationPanes(); refreshMeterMenus(); configureDesktop(); rebuildSwitcher(switcherItems()); refreshSwitcherLabels(); updateUserCount(); updateStatus();
        updateMarker(); updateTitle();
    }
    void zoomChat(int delta)
    {
        chatFont_.setPointSize(qBound(6, (chatFont_.pointSize() > 0 ? chatFont_.pointSize() : 10) + delta, 48));
        applyDisplay();
    }
    void displayPreferences()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Preferences"));
        dialog.setObjectName(QStringLiteral("preferencesDialog"));
        dialog.resize(900, 660);
        auto *outer = new QVBoxLayout(&dialog);
        auto *body = new QHBoxLayout;
        auto *categories = new QListWidget(&dialog); categories->setObjectName(QStringLiteral("preferencesCategories"));
        categories->setMaximumWidth(190); body->addWidget(categories);
        auto *pages = new QStackedWidget(&dialog); body->addWidget(pages, 1); outer->addLayout(body, 1);
        connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
        auto page = [&](const QString &name) {
            categories->addItem(name);
            auto *area = new QScrollArea(pages); area->setWidgetResizable(true);
            auto *content = new QWidget(area); auto *form = new QFormLayout(content);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            form->setRowWrapPolicy(QFormLayout::WrapLongRows);
            area->setWidget(content); pages->addWidget(area); return form;
        };
        auto *form = page(QStringLiteral("Appearance"));
        auto *family = new QFontComboBox(&dialog);
        family->setCurrentFont(chatFont_);
        auto *size = new QSpinBox(&dialog);
        size->setObjectName(QStringLiteral("fontSize"));
        size->setRange(6, 48);
        size->setValue(chatFont_.pointSize() > 0 ? chatFont_.pointSize() : 10);
        auto *blocks = new QSpinBox(&dialog);
        blocks->setObjectName(QStringLiteral("scrollbackLimit"));
        blocks->setRange(100, 50000);
        blocks->setValue(scrollback_);
        auto *timestamps = new QCheckBox(QStringLiteral("Show timestamps on new messages"), &dialog);
        timestamps->setObjectName(QStringLiteral("timestamps"));
        timestamps->setChecked(timestamps_);
        auto *wrap = new QCheckBox(QStringLiteral("Wrap long lines"), &dialog);
        wrap->setObjectName(QStringLiteral("wrap"));
        wrap->setChecked(wrap_);
        form->addRow(QStringLiteral("Chat font:"), family);
        form->addRow(QStringLiteral("Font size:"), size);
        form->addRow(QStringLiteral("Scrollback lines (reducing discards older text):"), blocks);
        form->addRow(timestamps);
        form->addRow(wrap);
        auto *timestampFormat = new QLineEdit(timestampFormat_, &dialog); timestampFormat->setObjectName(QStringLiteral("timestampFormat"));
        timestampFormat->setMaxLength(512); form->addRow(QStringLiteral("Timestamp format (strftime):"), timestampFormat);
        auto *timestampPreview = new QLabel(&dialog); timestampPreview->setTextFormat(Qt::PlainText); form->addRow(QStringLiteral("Preview:"), timestampPreview);
        auto previewTimestamp = [timestampFormat, timestampPreview]() { const QString result = formatChatTimestamp(0, timestampFormat->text()); timestampPreview->setText(result.isEmpty() ? QStringLiteral("Invalid or empty format") : result); };
        connect(timestampFormat, &QLineEdit::textChanged, &dialog, previewTimestamp); previewTimestamp();
        auto *background = new QLineEdit(backgroundPath_, &dialog); background->setObjectName(QStringLiteral("chatBackground")); background->setMaxLength(4096);
        auto *backgroundRow = new QWidget(&dialog); auto *backgroundLayout = new QHBoxLayout(backgroundRow); backgroundLayout->setContentsMargins(0, 0, 0, 0);
        backgroundLayout->addWidget(background, 1);
        auto *browseBackground = new QPushButton(QStringLiteral("Browse…"), backgroundRow); auto *clearBackground = new QPushButton(QStringLiteral("Clear"), backgroundRow);
        backgroundLayout->addWidget(browseBackground); backgroundLayout->addWidget(clearBackground); form->addRow(QStringLiteral("Chat background image (tiled):"), backgroundRow);
        auto *backgroundPreview = new QLabel(&dialog); backgroundPreview->setObjectName(QStringLiteral("backgroundPreview")); backgroundPreview->setTextFormat(Qt::PlainText); form->addRow(backgroundPreview);
        auto previewBackground = [background, backgroundPreview]() { QImage image; QString error;
            if (!loadChatBackground(background->text(), image, error)) { backgroundPreview->setPixmap({}); backgroundPreview->setText(error); }
            else if (image.isNull()) { backgroundPreview->setPixmap({}); backgroundPreview->setText(QStringLiteral("Use chat background color")); }
            else backgroundPreview->setPixmap(QPixmap::fromImage(image.scaled(180, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        };
        connect(background, &QLineEdit::textChanged, &dialog, previewBackground); previewBackground();
        connect(clearBackground, &QPushButton::clicked, background, &QLineEdit::clear);
        connect(browseBackground, &QPushButton::clicked, &dialog, [&dialog, background]() { const QString path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Chat Background"), background->text(), QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif);;All files (*)")); if (!path.isEmpty()) background->setText(path); });
        auto *opacity = new QSpinBox(&dialog); opacity->setObjectName(QStringLiteral("windowOpacity")); opacity->setRange(25, 100); opacity->setSuffix(QStringLiteral("%")); opacity->setValue(opacity_);
        form->addRow(QStringLiteral("Window opacity:"), opacity);
        QMap<QString, QCheckBox *> visualEditors;
        auto visualOption = [&](QFormLayout *layout, const QString &key, const QString &label) {
            auto *check = new QCheckBox(label, &dialog); check->setObjectName(key); check->setChecked(visualOptions_.value(key));
            layout->addRow(check); visualEditors.insert(key, check);
        };
        visualOption(form, QStringLiteral("compactLists"), QStringLiteral("Compact user lists"));
        auto *inputForm = page(QStringLiteral("Input Box"));
        auto *spellingStatus = new QLabel(SpellChecker::compiled() ? QStringLiteral("Installed dictionaries: ") + spellChecker_->availableLanguages().join(QStringLiteral(", ")) + QStringLiteral("\nCurrent status: ") + spellChecker_->status() : QStringLiteral("This build has no Enchant spell-checking support."), &dialog);
        spellingStatus->setWordWrap(true); spellingStatus->setTextFormat(Qt::PlainText); inputForm->addRow(spellingStatus);
        visualOption(inputForm, QStringLiteral("nickCompletion"), QStringLiteral("Enable nickname and command completion"));
        visualOption(inputForm, QStringLiteral("autoReplace"), QStringLiteral("Automatically replace words"));
        auto *colors = new QPushButton(QStringLiteral("Configure Chat Colors…"), &dialog);
        colors->setObjectName(QStringLiteral("paletteSettingsButton")); auto *colorsForm = page(QStringLiteral("Colors")); colorsForm->addRow(colors);
        connect(colors, &QPushButton::clicked, &dialog, [this, &dialog]() {
            showPaletteEditor(QDir(QString::fromUtf8(qt_core_config_dir())).filePath(QStringLiteral("colors.conf")), &dialog,
                [guard = QPointer<MainWindow>(this)]() { if (guard) guard->paletteChanged(); });
        });
        auto *sounds = new QPushButton(QStringLiteral("Configure Event Sounds…"), &dialog);
        sounds->setObjectName(QStringLiteral("soundSettingsButton")); page(QStringLiteral("Sounds"))->addRow(sounds);
        connect(sounds, &QPushButton::clicked, &dialog, [&dialog]() { showSoundSettings(&dialog); });
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        struct CoreEditor { int index; QByteArray expected; QWidget *widget; int type; };
        QList<CoreEditor> editors;
        QMap<QString, QFormLayout *> corePages{{QStringLiteral("Input Box"), inputForm}, {QStringLiteral("Appearance"), form}, {QStringLiteral("Colors"), colorsForm}};
        for (int i = 0; i < qt_core_preference_count(); ++i) {
            QtPreference pref{}; if (!qt_core_preference_read(i, &pref)) continue;
            const QString group = QString::fromUtf8(pref.group);
            if (!corePages.contains(group)) {
                auto *layout = page(group); corePages.insert(group, layout);
                if (group == QStringLiteral("Network")) {
                    auto *note = new QLabel(QStringLiteral("Connection settings apply to new connections. Reconnect to use changes."), &dialog);
                    note->setWordWrap(true); layout->addRow(note);
                }
            }
            auto *layout = corePages.value(group); QWidget *editor = nullptr;
            if (pref.type == 2) {
                auto *check = new QCheckBox(QString::fromUtf8(pref.label), &dialog);
                check->setChecked(QByteArray(pref.value).toInt() != 0);
                if (QByteArray(pref.key) == "gui_input_spell" && !SpellChecker::compiled()) { check->setEnabled(false); check->setToolTip(QStringLiteral("Built without Enchant support.")); }
                layout->addRow(check); editor = check;
            } else if (pref.type == 1 && pref.choices && *pref.choices) {
                auto *combo = new QComboBox(&dialog); combo->addItems(QString::fromUtf8(pref.choices).split(QLatin1Char('|')));
                int current = QByteArray(pref.value).toInt();
                for (int row = 0; row < combo->count(); ++row) combo->setItemData(row, QByteArray(pref.key) == "gui_tab_layout" && row == 1 ? 2 : QByteArray(pref.key) == "gui_tab_pos" || QByteArray(pref.key) == "gui_ulist_pos" ? row + 1 : row);
                const int selected = combo->findData(current);
                if (selected < 0) { combo->addItem(QStringLiteral("Current value (%1)").arg(current), current); combo->setCurrentIndex(combo->count() - 1); }
                else combo->setCurrentIndex(selected);
                layout->addRow(QString::fromUtf8(pref.label), combo); editor = combo;
            } else if (pref.type == 1) {
                auto *spin = new QSpinBox(&dialog); spin->setRange(qMin(0, QByteArray(pref.value).toInt()), qMax(pref.maximum, QByteArray(pref.value).toInt())); spin->setValue(QByteArray(pref.value).toInt());
                layout->addRow(QString::fromUtf8(pref.label), spin); editor = spin;
            } else {
                auto *entry = new QLineEdit(QString::fromUtf8(pref.value), &dialog);
                if (QByteArray(pref.key) == "net_proxy_pass") entry->setEchoMode(QLineEdit::Password);
                if (QByteArray(pref.key) == "text_spell_langs" && !SpellChecker::compiled()) entry->setReadOnly(true);
                layout->addRow(QString::fromUtf8(pref.label), entry); editor = entry;
            }
            editor->setObjectName(QStringLiteral("pref_") + QString::fromUtf8(pref.key));
            editors.append({i, pref.value, editor, pref.type});
        }
        QList<QPair<QAction *, QCheckBox *>> localOptions;
        auto localPage = [&](const QString &name, const QList<QAction *> &actions) {
            auto *layout = corePages.contains(name) ? corePages.value(name) : page(name);
            for (auto *action : actions) {
                auto *check = new QCheckBox(QString(action->text()).remove(QLatin1Char('&')), &dialog);
                check->setChecked(action->isChecked()); check->setObjectName(QStringLiteral("pref_") + action->objectName());
                layout->addRow(check); localOptions.append({action, check});
            }
        };
        localPage(QStringLiteral("Interface"), {showChannels_, showDialogButtons_, autoScroll_});
        localPage(QStringLiteral("Alerts"), {notifications_, previews_});
        auto *errorLabel = new QLabel(&dialog); errorLabel->setObjectName(QStringLiteral("preferencesError"));
        errorLabel->setWordWrap(true); errorLabel->setTextFormat(Qt::PlainText); outer->addWidget(errorLabel);
        outer->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        categories->setCurrentRow(0);
        QImage acceptedBackground;
        for (;;) {
            if (dialog.exec() != QDialog::Accepted) return;
            QString backgroundError;
            if (!loadChatBackground(background->text(), acceptedBackground, backgroundError)) { errorLabel->setText(backgroundError); continue; }
            if (formatChatTimestamp(0, timestampFormat->text()).isEmpty()) {
                errorLabel->setText(QStringLiteral("Use a valid, nonempty timestamp format.")); continue;
            }
            QList<QtPreferenceChange> changes;
            bool tooLong = false;
            for (const auto &editor : editors) {
                QByteArray value;
                if (editor.type == 2) value = static_cast<QCheckBox *>(editor.widget)->isChecked() ? "1" : "0";
                else if (auto *combo = qobject_cast<QComboBox *>(editor.widget)) value = QByteArray::number(combo->currentData().toInt());
                else if (editor.type == 1) value = QByteArray::number(static_cast<QSpinBox *>(editor.widget)->value());
                else value = static_cast<QLineEdit *>(editor.widget)->text().toUtf8();
                if (value == editor.expected) continue;
                if (value.size() >= QT_PREFERENCE_CAPACITY || value.contains('\0')) { tooLong = true; break; }
                QtPreferenceChange change{}; change.index = editor.index;
                qstrncpy(change.expected, editor.expected.constData(), sizeof(change.expected));
                qstrncpy(change.value, value.constData(), sizeof(change.value)); changes.append(change);
            }
            char error[512] = {};
            if (tooLong) errorLabel->setText(QStringLiteral("A text setting is too long or contains a null character."));
            else if (qt_core_preferences_apply(changes.constData(), changes.size(), error, sizeof(error))) break;
            else errorLabel->setText(QString::fromUtf8(error));
        }
        for (const auto &option : localOptions) option.first->setChecked(option.second->isChecked());
        backgroundPath_ = background->text(); backgroundImage_ = acceptedBackground;
        timestampFormat_ = timestampFormat->text(); opacity_ = opacity->value();
        for (auto it = visualEditors.cbegin(); it != visualEditors.cend(); ++it) visualOptions_[it.key()] = it.value()->isChecked();
        chatFont_ = family->currentFont();
        chatFont_.setPointSize(size->value());
        scrollback_ = blocks->value();
        timestamps_ = timestamps->isChecked();
        wrap_ = wrap->isChecked();
        applyDisplay(); updateTrayUnread();
        savePreferences();
    }
    void restorePreferences()
    {
        chatFont_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        chatFont_.fromString(settings_->value(QStringLiteral("display/font"), chatFont_.toString()).toString());
        scrollback_ = qBound(100, settings_->value(QStringLiteral("display/scrollback"), 5000).toInt(), 50000);
        timestamps_ = settings_->value(QStringLiteral("display/timestamps"), true).toBool();
        wrap_ = settings_->value(QStringLiteral("display/wrap"), true).toBool();
        backgroundPath_ = settings_->value(QStringLiteral("display/background")).toString(); reloadBackground();
        timestampFormat_ = settings_->value(QStringLiteral("display/timestampFormat"), QStringLiteral("[%H:%M] ")).toString();
        if (formatChatTimestamp(0, timestampFormat_).isEmpty()) timestampFormat_ = QStringLiteral("[%H:%M] ");
        opacity_ = qBound(25, settings_->value(QStringLiteral("display/opacity"), 100).toInt(), 100);
        for (auto it = visualOptions_.begin(); it != visualOptions_.end(); ++it) it.value() = settings_->value(QStringLiteral("appearance/") + it.key(), it.value()).toBool();
        showChannels_->setChecked(settings_->value(QStringLiteral("view/channels"), true).toBool());
        showDialogButtons_->setChecked(settings_->value(QStringLiteral("view/dialogButtons"), true).toBool());
        notifications_->setChecked(settings_->value(QStringLiteral("desktop/notifications"), true).toBool());
        previews_->setChecked(settings_->value(QStringLiteral("desktop/previews"), false).toBool());
        configureDesktop();
        autoScroll_->setChecked(settings_->value(QStringLiteral("view/follow"), true).toBool());
        restoreGeometry(settings_->value(QStringLiteral("window/geometry")).toByteArray());
        split_->restoreState(settings_->value(QStringLiteral("window/splitter")).toByteArray());
        applyDisplay();
        leftDock_->restoreState(settings_->value(QStringLiteral("window/splitterLeft")).toByteArray());
        rightDock_->restoreState(settings_->value(QStringLiteral("window/splitterRight")).toByteArray());
    }
    void savePreferences()
    {
        for (auto *view : detached_) {
            settings_->setValue(view->geometryKey + QStringLiteral("/geometry"), view->saveGeometry());
            settings_->setValue(view->geometryKey + QStringLiteral("/splitter"), view->splitter->saveState());
        }
        settings_->setValue(QStringLiteral("window/geometry"), saveGeometry());
        settings_->setValue(QStringLiteral("window/splitter"), split_->saveState());
        settings_->setValue(QStringLiteral("window/splitterLeft"), leftDock_->saveState());
        settings_->setValue(QStringLiteral("window/splitterRight"), rightDock_->saveState());
        settings_->setValue(QStringLiteral("display/font"), chatFont_.toString());
        settings_->setValue(QStringLiteral("display/scrollback"), scrollback_);
        settings_->setValue(QStringLiteral("display/timestamps"), timestamps_);
        settings_->setValue(QStringLiteral("display/wrap"), wrap_);
        settings_->setValue(QStringLiteral("display/background"), backgroundPath_);
        settings_->setValue(QStringLiteral("display/timestampFormat"), timestampFormat_);
        settings_->setValue(QStringLiteral("display/opacity"), opacity_);
        for (auto it = visualOptions_.cbegin(); it != visualOptions_.cend(); ++it) settings_->setValue(QStringLiteral("appearance/") + it.key(), it.value());
        settings_->setValue(QStringLiteral("view/channels"), showChannels_->isChecked());
        settings_->remove(QStringLiteral("view/userButtons"));
        settings_->setValue(QStringLiteral("view/dialogButtons"), showDialogButtons_->isChecked());
        settings_->remove(QStringLiteral("view/menu"));
        settings_->remove(QStringLiteral("view/users"));
        settings_->remove(QStringLiteral("view/channelModes"));
        settings_->remove(QStringLiteral("view/topic"));
        settings_->setValue(QStringLiteral("view/follow"), autoScroll_->isChecked());
        settings_->setValue(QStringLiteral("desktop/notifications"), notifications_->isChecked());
        settings_->setValue(QStringLiteral("desktop/previews"), previews_->isChecked());
        settings_->sync();
        if (settings_->status() != QSettings::NoError)
            statusBar()->showMessage(QStringLiteral("Could not save Qt display settings"), 10000);
    }

    void submit(const QString &line) { submitTo(captureTarget(), line); }
    void refreshUsers()
    {
        if (!conversations_.contains(active_)) { nicks_->setState(nullptr); updateUserCount(); input_->setNicks({}); return; }
        nicks_->setState(conversations_[active_].users);
        updateUserCount();
        input_->setNicks(conversations_[active_].users->users.keys());
    }
    QString conversationTitle(void *key) const
    {
        QString title = QStringLiteral("ZoiteChat — ") + (conversations_.contains(key) ? conversations_.value(key).name : QStringLiteral("Qt 6"));
        QtSessionInfo info{};
        if (!conversations_.contains(key) || !qt_core_session_info(key, &info)) return title;
        if (titleNick_ && info.nick && *info.nick) title += QStringLiteral(" — ") + QString::fromUtf8(info.nick);
        if (titleUsers_ && info.channel) title += QStringLiteral(" [%1 users]").arg(conversations_.value(key).users->users.size());
        QtChannelState channel{};
        if (titleModes_ && info.channel && qt_core_channel_state(key, &channel) && *channel.modes)
            title += QStringLiteral(" ") + QString::fromUtf8(channel.modes);
        return title;
    }
    void updateTitle() { setWindowTitle(conversationTitle(active_)); }

    std::unique_ptr<QSettings> settings_;
    std::shared_ptr<SpellChecker> spellChecker_;
    QFont chatFont_;
    int scrollback_ = 5000;
    bool timestamps_ = true, wrap_ = true, stripTopics_ = false;
    QString timestampFormat_ = QStringLiteral("[%H:%M] ");
    int opacity_ = 100, indentWidth_ = 0;
    bool inputIcon_ = true, showUserCount_ = true, wheelSwitches_ = false, middleCloses_ = true, sortConversations_ = true;
    int newConversationFocus_ = 2, switcherTruncation_ = 20;
    int lagMeterMode_ = 2, queueMeterMode_ = 2;
    QString backgroundPath_;
    QImage backgroundImage_;
    QPixmap backgroundTexture_;
    QList<QPointer<QMenu>> meterMenus_;
    bool minimizeToTray_ = false, trayAway_ = false;
    bool showMarker_ = true, titleNick_ = false, titleModes_ = false, titleUsers_ = false;
    QMap<QString, bool> visualOptions_ = {{QStringLiteral("compactLists"), true},
        {QStringLiteral("nickCompletion"), true}, {QStringLiteral("autoReplace"), true}};
    QPushButton *inputNickname_ = nullptr;
    QSplitter *split_ = nullptr;
    QWidget *searchBar_ = nullptr;
    QLineEdit *search_ = nullptr;
    QCheckBox *matchCase_ = nullptr, *regexSearch_ = nullptr;
    QLabel *searchResult_ = nullptr, *connection_ = nullptr;
    QLabel *lagStatus_ = nullptr, *queueStatus_ = nullptr;
    QProgressBar *lagMeter_ = nullptr, *queueMeter_ = nullptr;
    QProgressBar *connectionProgress_ = nullptr;
    QAction *detachAction_ = nullptr;
    QAction *flushQueue_ = nullptr;
    QAction *newChannel_ = nullptr;
    QAction *channelSettings_ = nullptr, *banManager_ = nullptr, *inviteUser_ = nullptr;
    QAction *channelBrowser_ = nullptr, *rawLog_ = nullptr;
    QAction *disconnect_ = nullptr, *reconnect_ = nullptr, *nickname_ = nullptr;
    QAction *away_ = nullptr, *awayReason_ = nullptr, *topicAction_ = nullptr, *leave_ = nullptr;
    QAction *closeConversation_ = nullptr, *showChannels_ = nullptr, *showUsers_ = nullptr;
    QWidget *channelModes_ = nullptr;
    QMap<QChar, QToolButton *> modeButtons_;
    QToolButton *channelKey_ = nullptr, *channelLimit_ = nullptr;
    QAction *showModes_ = nullptr;
    QAction *conversationOptions_ = nullptr;
    QAction *showTopic_ = nullptr, *autoScroll_ = nullptr;
    CustomMenus *customMenus_ = nullptr;
    DesktopIntegration *desktop_ = nullptr;
    QScrollArea *userButtons_ = nullptr, *dialogButtons_ = nullptr;
    QAction *showMenu_ = nullptr;
    QAction *showUserButtons_ = nullptr, *showDialogButtons_ = nullptr;
    QWidget *usersPanel_ = nullptr;
    QLineEdit *userFilter_ = nullptr;
    QLabel *userCount_ = nullptr;
    QAction *rankUsers_ = nullptr, *trayEnabled_ = nullptr, *notifications_ = nullptr, *previews_ = nullptr, *closeToTray_ = nullptr;
    bool usersRefreshPending_ = false, quitRequested_ = false;
    QPointer<QTimer> pasteQueue_;
    quint64 nextMarkerId_ = 0;
    ConversationTree *channels_ = nullptr;
    bool switcherIcons_ = true, switcherLines_ = false, switcherNativeStyle_ = false;
    int switcherSmall_ = 0;
    ConversationTabs *tabs_ = nullptr;
    QWidget *middle_ = nullptr, *placedSwitcher_ = nullptr;
    QVBoxLayout *middleLayout_ = nullptr;
    PaneDrag *paneDrag_ = nullptr;
    QSplitter *leftDock_ = nullptr, *rightDock_ = nullptr;
    int switcherPosition_ = 1, userListPosition_ = 3, placedPosition_ = -1, placedUsers_ = -1;
    QAction *layoutTabs_ = nullptr, *layoutTree_ = nullptr;
    int switcherLayout_ = 2;
    ChatTranscript *chat_ = nullptr;
    QTextDocument *emptyDocument_ = nullptr;
    ChatInput *input_ = nullptr;
    InputBindings *keyBindings_ = nullptr;
    NickList *nicks_ = nullptr;
    TopicEntry *topic_ = nullptr;
    QHash<void *, Conversation> conversations_;
    QHash<void *, ConversationWindow *> detached_;
    QHash<void *, QTreeWidgetItem *> items_;
    void *active_ = nullptr;
    QByteArray inputBytes_;
    bool closing_ = false;
};

QApplication *app = nullptr;
MainWindow *window = nullptr;
QByteArray pickedFile;

MainWindow *ui()
{
    if (!window) {
        window = new MainWindow();
        window->show();
    }
    return window;
}
} // namespace

extern "C" {
void qt_fe_initialize(int *argc, char **argv)
{
    if (!app) {
        app = new QApplication(*argc, argv);
        app->setApplicationName(QStringLiteral("zoitechat-qt"));
        app->setDesktopFileName(QStringLiteral("net.zoite.Zoitechat.Qt"));
        app->setWindowIcon(QIcon::fromTheme(QStringLiteral("net.zoite.Zoitechat"), QIcon(QStringLiteral(":/zoitechat/zoitechat.png"))));
        app->setOrganizationName(QStringLiteral("ZoiteChat"));
    }
    (void)ui();
}
void qt_fe_startup_state(int level)
{
    if (window && level >= 0 && level <= 2) QTimer::singleShot(0, window, [level]() { if (window) window->startupState(level); });
}
int qt_fe_run(void) { return app ? app->exec() : 1; }
void qt_fe_quit(void) { if (app) app->quit(); }
void qt_fe_network_manager(void) { showNetworkManager(ui()); }
void qt_fe_new_session(void *sess, const char *name, int focus)
{ ui()->addSession(sess, QString::fromUtf8(name ? name : "Server"), focus != 0); }
void qt_fe_remove_session(void *sess) { qt_fe_moderation_closed(sess); if (window) window->removeSession(sess); }
void qt_fe_rename_session(void *sess, const char *name)
{ if (window) window->renameSession(sess, QString::fromUtf8(name ? name : "Server")); }
void qt_fe_select_session(void *sess) { if (window) window->select(sess); }
void qt_fe_print(void *sess, const char *text, time_t stamp)
{
    if (window && text && *text)
        window->print(sess, QString::fromUtf8(text), stamp);
}
void qt_fe_lastlog(void *source, void *destination, const char *search, int flags)
{ if (window) window->lastlog(source, destination, QString::fromUtf8(search ? search : ""), flags); }
void qt_fe_clear_lines(void *sess, int lines) { if (window) window->clear(sess, lines); }
void qt_fe_gui_control(void *sess, int action, int argument)
{ if (window) window->guiControl(sess, action, argument); }
int qt_fe_session_window_status(void *session) { return window ? window->sessionWindowStatus(session) : 2; }
int qt_fe_window_status(void)
{ return !window || !window->isVisible() || window->isMinimized() ? 2 : window->isActiveWindow() ? 1 : 0; }
void qt_fe_clear(void *sess) { if (window) window->clear(sess); }
void qt_fe_topic(void *sess, const char *topic)
{ if (window) window->topic(sess, QString::fromUtf8(topic ? topic : "")); }
void qt_fe_user_add(void *sess, const char *nick, const char *prefix, int select)
{ if (window) window->addUser(sess, QString::fromUtf8(nick ? nick : ""),
                              QString::fromUtf8(prefix ? prefix : ""), select != 0); }
void qt_fe_user_remove(void *sess, const char *nick)
{ if (window) window->removeUser(sess, QString::fromUtf8(nick ? nick : "")); }
void qt_fe_user_details(void *sess, const char *nick, const char *host, const char *real_name,
                        const char *account, const char *server, int away, int me, int rank)
{
    if (!window) return;
    NickDetails details;
    details.host = QString::fromUtf8(host ? host : ""); details.realName = QString::fromUtf8(real_name ? real_name : "");
    details.account = QString::fromUtf8(account ? account : ""); details.server = QString::fromUtf8(server ? server : "");
    details.away = away != 0; details.me = me != 0; details.rank = rank;
    window->setUserDetails(sess, QString::fromUtf8(nick ? nick : ""), details);
}
void qt_fe_message_alert(void *sess, const char *text, int mention, int private_message)
{ if (window) window->messageAlert(sess, plainIrcText(text), mention != 0, private_message != 0); }
void qt_fe_attention(void *sess) { if (window) window->attention(sess); }
void qt_fe_beep(void) { QApplication::beep(); }
void qt_fe_tray_file(const char *file) { if (window) window->trayFile(QString::fromUtf8(file ? file : "")); }
void qt_fe_tray_flash(const char *first, const char *second, int interval)
{ if (window) window->trayFlash(QString::fromUtf8(first ? first : ""), QString::fromUtf8(second ? second : ""), interval); }
void qt_fe_tray_state(int state) { if (window) window->trayState(state); }
void qt_fe_tray_tooltip(const char *text) { if (window) window->trayTooltip(QString::fromUtf8(text ? text : "")); }
int qt_fe_user_selected(void *sess, const char *nick)
{ return window && window->userSelected(sess, QString::fromUtf8(nick ? nick : "")); }
void qt_fe_user_select(void *sess, const char *nick, int clear, int scroll)
{ if (window) window->selectUser(sess, QString::fromUtf8(nick ? nick : ""), clear != 0, scroll != 0); }
void qt_fe_users_clear(void *sess) { if (window) window->clearUsers(sess); }
int qt_fe_session_attached(void *sess) { return window && window->sessionAttached(sess); }
void qt_fe_activity(void *sess, int level) { if (window) window->activity(sess, level); }
void qt_fe_transfer_send_request(void *session, const char *nick, int maxcps, int passive)
{ if (window) window->requestTransferSend(session, QString::fromUtf8(nick ? nick : ""), maxcps, passive); }
void qt_fe_connection_changed(void) { if (window) window->connectionChanged(); }
void qt_fe_message(const char *text, int warning)
{ ui()->notify(QString::fromUtf8(text ? text : ""), warning != 0); }
void qt_fe_set_input(void *sess, const char *text)
{ if (window) window->setInput(sess, QString::fromUtf8(text ? text : "")); }
const char *qt_fe_get_input(void *sess) { return window ? window->inputText(sess) : ""; }
int qt_fe_get_cursor(void *sess) { return window ? window->inputCursor(sess) : 0; }
void qt_fe_set_cursor(void *sess, int relative, int position)
{ if (window) window->setInputCursor(sess, relative != 0, position); }
void qt_fe_open_url(const char *url)
{ if (url) QDesktopServices::openUrl(QUrl::fromUserInput(QString::fromUtf8(url))); }
int qt_fe_confirm(const char *message)
{
    return QMessageBox::question(ui(), QStringLiteral("ZoiteChat"),
        QString::fromUtf8(message ? message : "")) == QMessageBox::Yes;
}
const char *qt_fe_pick_file(const char *title, const char *initial, int flags)
{
    const QString caption = QString::fromUtf8(title ? title : "Choose file");
    const QString start = QString::fromUtf8(initial ? initial : "");
    QString chosen;
    if (flags & 8) chosen = QFileDialog::getExistingDirectory(ui(), caption, start);
    else if (flags & 1) chosen = QFileDialog::getSaveFileName(ui(), caption, start);
    else chosen = QFileDialog::getOpenFileName(ui(), caption, start);
    pickedFile = chosen.toUtf8();
    return chosen.isEmpty() ? nullptr : pickedFile.constData();
}
void qt_fe_customization_changed(int kind) { if (window) window->customChanged(kind); }
void qt_fe_menus_changed(void) { if (window) window->menusChanged(); }
int qt_fe_is_active(void) { return window && window->isActiveWindow() ? 1 : 0; }
}

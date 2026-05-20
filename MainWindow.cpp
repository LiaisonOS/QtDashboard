//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : QtDashboard main window — Desktop Mode and Touch Mode.
//

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QApplication>
#include <QStyle>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QScroller>

#include <memory>
#include <tuple>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_userConfig(new UserConfig(this))
    , m_supervisor(new SupervisorClient(this))
    , m_modeLoader(new ModeLoader(this))
    , m_menu(nullptr)
    , m_statusTimer(new QTimer(this))
    , m_clockTimer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle("LiaisonOS Dashboard");
    setWindowOpacity(0.92);

    m_language = m_userConfig->language();
    m_modeLoader->load();
    m_menu = new MenuLoader(m_modeLoader, this);
    connect(m_menu, &MenuLoader::menuChanged,
            this, &MainWindow::onMenuChanged,
            Qt::QueuedConnection);

    connect(m_userConfig, &UserConfig::touchModeChanged,
            this, &MainWindow::onTouchModeChanged);
    connect(m_userConfig, &UserConfig::configChanged,
            this, &MainWindow::refreshOperator);
    connect(m_userConfig, &UserConfig::configChanged,
            this, &MainWindow::refreshRecentModes);
    // tracking() change → GPS label needs to flip between green/yellow
    connect(m_userConfig, &UserConfig::configChanged,
            this, &MainWindow::refreshInterfaces);
    connect(m_supervisor, &SupervisorClient::statusReceived,
            this, &MainWindow::onStatusReceived,
            Qt::QueuedConnection);
    connect(m_statusTimer, &QTimer::timeout,
            this, &MainWindow::refreshStatus);
    connect(m_clockTimer, &QTimer::timeout,
            this, &MainWindow::updateClock);
    connect(QApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, &MainWindow::onScreenGeometryChanged);

    m_statusTimer->start(5000);
    startClockTimer();

    // GPS notify socket — receives JSON/text from QtGpsSync via /tmp/et-gps-notify.sock
    {
        int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/tmp/et-gps-notify.sock", sizeof(addr.sun_path) - 1);
        ::unlink("/tmp/et-gps-notify.sock");
        if (::bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            delete m_gpsNotifySocket;
            m_gpsNotifySocket = new QUdpSocket(this);
            m_gpsNotifySocket->setSocketDescriptor(fd);
            connect(m_gpsNotifySocket, &QUdpSocket::readyRead,
                    this, &MainWindow::onGpsNotify);
        } else {
            ::close(fd);
        }
    }

    applyStyleSheet();

    if (m_userConfig->touchMode())
        buildTouchUI();
    else
        buildDesktopUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Style — dark grey palette matching et-dashboard
// ---------------------------------------------------------------------------

void MainWindow::applyStyleSheet()
{
    setStyleSheet(
        "QMainWindow, QWidget { background-color: #1a1a1a; color: #e0e0e0; }"

        "QPushButton {"
        "  background-color: #1a1a1a; color: #ffffff;"
        "  border: 1px solid #404040; border-radius: 4px;"
        "  padding: 9px 14px; font-size: 15px; text-align: left;"
        "}"
        "QPushButton:hover   { background-color: #3d3d3d; }"
        "QPushButton:pressed { background-color: #484848; }"

        "QPushButton[active=\"true\"] {"
        "  background-color: #1a3a1a; color: #4ade80;"
        "  border: 1px solid #4ade80;"
        "}"

        "QPushButton#stopBtn {"
        "  background-color: #7a1a1a; color: #ffffff;"
        "  border: 1px solid #cc0000; border-radius: 4px;"
        "  padding: 7px 14px; text-align: center;"
        "}"
        "QPushButton#stopBtn:hover   { background-color: #8a2020; }"
        "QPushButton#stopBtn:pressed { background-color: #aa2020; }"

        "QPushButton#utilBtn {"
        "  background-color: #252525; color: #9e9e9e;"
        "  border: 1px solid #404040; border-radius: 4px;"
        "  padding: 5px 11px; font-size: 14px; text-align: center;"
        "}"
        "QPushButton#utilBtn:hover   { background-color: #333333; color: #e0e0e0; }"
        "QPushButton#utilBtn:checked { background-color: #2E7D32; color: #ffffff; border: 1px solid #4ade80; }"

        "QPushButton#sectionHeader {"
        "  background-color: #252525; color: #b0b0b0;"
        "  border: none; border-bottom: 1px solid #333333;"
        "  border-radius: 0px; padding: 7px 10px;"
        "  font-size: 14px; font-weight: bold; text-align: left;"
        "}"
        "QPushButton#sectionHeader:hover { background-color: #2d2d2d; }"

        "QWidget#clickableSection { background-color: #202020; border: 1px solid #333333; border-radius: 4px; }"

        "QPushButton#modemBtn {"
        "  background-color: #1a1a1a; color: #9e9e9e;"
        "  border: 1px solid #353535; border-radius: 4px;"
        "  padding: 6px 9px; font-size: 14px; text-align: center;"
        "}"
        "QPushButton#modemBtn:hover   { background-color: #2d2d2d; color: #e0e0e0; }"
        "QPushButton#modemBtn:pressed { background-color: #1a3a1a; color: #4ade80; }"

        "QLabel#clockLabel      { font-size: 13px; font-weight: bold; color: #ffa500; font-family: monospace; }"
        "QLabel#sectionTitle    { font-size: 13px; font-weight: bold; color: #888888; }"
        "QLabel#infoLabel       { font-size: 14px; color: #9e9e9e; }"
        "QLabel#callsignLabel   { font-size: 18px; font-weight: bold; color: #ffa500; }"
        "QLabel#gridLabel       { font-size: 15px; color: #9e9e9e; }"
        "QLabel#activeModeLabel { font-size: 15px; color: #4ade80; }"
        "QLabel#gpsLabel        { font-size: 14px; }"

        "QScrollArea { border: none; }"
        "QScrollBar:vertical { width: 4px; background: #1a1a1a; }"
        "QScrollBar::handle:vertical { background: #404040; border-radius: 2px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"

        "QFrame#separator { color: #2d2d2d; }"
    );
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QFrame* makeSep(QWidget *parent)
{
    QFrame *f = new QFrame(parent);
    f->setObjectName("separator");
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    return f;
}

void MainWindow::clearUI()
{
    m_modeButtons.clear();
    m_clockLabel      = nullptr;
    m_gpsLabel        = nullptr;
    m_trackBtn        = nullptr;
    m_callsignLabel   = nullptr;
    m_gridLabel       = nullptr;
    m_radioLabel      = nullptr;
    m_catLabel        = nullptr;
    m_audioLabel      = nullptr;
    m_tncLabel        = nullptr;
    m_activeModeLabel = nullptr;
    m_stopBtn         = nullptr;
    m_processBox        = nullptr;
    m_recentLayout      = nullptr;
    m_recentTouchLayout = nullptr;
    m_recentStripLayout = nullptr;
    m_operatorEditor    = nullptr;
    m_ifaceEditor     = nullptr;
    m_radioCombo      = nullptr;
    m_radioNotesLabel = nullptr;
    m_editCallsign    = nullptr;
    m_editGrid        = nullptr;
    m_editName        = nullptr;
    m_editPassword    = nullptr;
    m_editLang        = nullptr;

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
}

// ---------------------------------------------------------------------------
// Desktop UI
// ---------------------------------------------------------------------------

void MainWindow::buildDesktopUI()
{
    clearUI();
    setWindowOpacity(1.0);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    QWidget *central = centralWidget();
    QVBoxLayout *main = new QVBoxLayout(central);
    main->setContentsMargins(8, 8, 8, 8);
    main->setSpacing(6);

    // -----------------------------------------------------------------------
    // TOP 1 — UTC clock | Sync | Track | GPS status | Touch toggle
    // -----------------------------------------------------------------------
    {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(4);

        m_clockLabel = new QLabel("UTC --:--:--", central);
        m_clockLabel->setObjectName("clockLabel");
        updateClock();
        row->addWidget(m_clockLabel);
        row->addStretch();

        QPushButton *syncBtn = new QPushButton("Sync", central);
        syncBtn->setObjectName("utilBtn");
        connect(syncBtn, &QPushButton::clicked, this, []() {
            QProcess::execute("xhost", {"+local:root"});
            QProcess::startDetached("sudo", {"-E", "QtGpsSync"});
        });
        row->addWidget(syncBtn);

        m_trackBtn = new QPushButton("Track", central);
        m_trackBtn->setObjectName("utilBtn");
        m_trackBtn->setCheckable(true);
        m_trackBtn->setChecked(m_userConfig->tracking());
        connect(m_trackBtn, &QPushButton::toggled, this, [this](bool on) {
            m_userConfig->setTracking(on);
            refreshOperator();
        });
        row->addWidget(m_trackBtn);

        m_gpsLabel = new QLabel("GPS: —", central);
        m_gpsLabel->setObjectName("gpsLabel");
        row->addWidget(m_gpsLabel);

        QPushButton *touchToggle = new QPushButton("Touch", central);
        touchToggle->setObjectName("utilBtn");
        connect(touchToggle, &QPushButton::clicked, this, &MainWindow::onToggleTouchMode);
        row->addWidget(touchToggle);

        main->addLayout(row);
    }
    main->addWidget(makeSep(central));

    // -----------------------------------------------------------------------
    // TOP 2 — OPERATOR (click to expand inline editor)
    // -----------------------------------------------------------------------
    {
        // Summary row (always visible)
        QWidget *section = new QWidget(central);
        section->setObjectName("clickableSection");
        section->setCursor(Qt::PointingHandCursor);
        QVBoxLayout *vl = new QVBoxLayout(section);
        vl->setContentsMargins(6, 4, 6, 4);
        vl->setSpacing(2);

        QHBoxLayout *hdr = new QHBoxLayout();
        QLabel *title = new QLabel("OPERATOR", section);
        title->setObjectName("sectionTitle");
        title->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold;");
        QLabel *arrow = new QLabel("  ✎", section);
        arrow->setObjectName("sectionTitle");
        hdr->addWidget(title);
        hdr->addStretch();
        hdr->addWidget(arrow);
        vl->addLayout(hdr);

        QHBoxLayout *info = new QHBoxLayout();
        m_callsignLabel = new QLabel(m_userConfig->callsign(), section);
        m_callsignLabel->setObjectName("callsignLabel");
        QString gs = m_userConfig->gridSquare();
        m_gridLabel = new QLabel(gs.isEmpty() ? "—" : gs, section);
        m_gridLabel->setObjectName("gridLabel");
        QLabel *sepLabel = new QLabel(" — ", section);
        sepLabel->setStyleSheet("color: #555555; font-size: 14px;");
        info->addWidget(m_callsignLabel);
        info->addWidget(sepLabel);
        info->addWidget(m_gridLabel);
        info->addStretch();
        vl->addLayout(info);

        connect(section, &QWidget::customContextMenuRequested, this, &MainWindow::toggleOperatorEditor);
        section->installEventFilter(this);
        section->setProperty("action", "operator-edit");
        main->addWidget(section);
        refreshOperator();

        // Inline editor (hidden until section clicked)
        m_operatorEditor = new QWidget(central);
        m_operatorEditor->setVisible(false);
        m_operatorEditor->setStyleSheet("background-color: #222; border: 1px solid #444; border-radius: 4px;");
        QVBoxLayout *el = new QVBoxLayout(m_operatorEditor);
        el->setContentsMargins(8, 8, 8, 8);
        el->setSpacing(6);

        auto addField = [&](const QString &lbl, QLineEdit *&field, const QString &placeholder, bool password = false) {
            QLabel *l = new QLabel(lbl, m_operatorEditor);
            l->setStyleSheet("color: #888; font-size: 10px;");
            el->addWidget(l);
            field = new QLineEdit(m_operatorEditor);
            field->setPlaceholderText(placeholder);
            field->setStyleSheet("background: #2d2d2d; color: #e0e0e0; border: 1px solid #444; border-radius: 3px; padding: 4px;");
            if (password) field->setEchoMode(QLineEdit::Password);
            el->addWidget(field);
        };

        addField("Callsign",         m_editCallsign, "e.g. VA2OPS");
        addField("Grid Square",      m_editGrid,     "e.g. FN35fl");
        addField("Name",             m_editName,     "Your name (optional)");
        addField("Winlink Password", m_editPassword, "••••••••", true);

        QLabel *ll = new QLabel("Language", m_operatorEditor);
        ll->setStyleSheet("color: #888; font-size: 10px;");
        el->addWidget(ll);
        m_editLang = new QComboBox(m_operatorEditor);
        m_editLang->addItem("English", "en");
        m_editLang->addItem("Français", "fr");
        m_editLang->setStyleSheet("background: #2d2d2d; color: #e0e0e0; border: 1px solid #444; border-radius: 3px; padding: 3px;");
        el->addWidget(m_editLang);

        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *saveBtn   = new QPushButton("Save",   m_operatorEditor);
        QPushButton *cancelBtn = new QPushButton("Cancel", m_operatorEditor);
        saveBtn->setStyleSheet("background: #2E7D32; color: #fff; border-radius: 3px; padding: 5px;");
        cancelBtn->setStyleSheet("background: #444; color: #e0e0e0; border-radius: 3px; padding: 5px;");
        btnRow->addWidget(saveBtn);
        btnRow->addWidget(cancelBtn);
        el->addLayout(btnRow);

        connect(saveBtn,   &QPushButton::clicked, this, &MainWindow::saveOperator);
        connect(cancelBtn, &QPushButton::clicked, this, &MainWindow::toggleOperatorEditor);

        main->addWidget(m_operatorEditor);
    }
    main->addWidget(makeSep(central));

    // -----------------------------------------------------------------------
    // TOP 3 — INTERFACES (click to expand inline radio editor)
    // -----------------------------------------------------------------------
    {
        QWidget *section = new QWidget(central);
        section->setObjectName("clickableSection");
        section->setCursor(Qt::PointingHandCursor);
        section->setProperty("action", "iface-edit");
        section->installEventFilter(this);
        QVBoxLayout *vl = new QVBoxLayout(section);
        vl->setContentsMargins(6, 4, 6, 4);
        vl->setSpacing(2);

        QHBoxLayout *hdr = new QHBoxLayout();
        QLabel *title = new QLabel("INTERFACES", section);
        title->setObjectName("sectionTitle");
        title->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold;");
        QLabel *arrow = new QLabel("  ✎", section);
        arrow->setObjectName("sectionTitle");
        hdr->addWidget(title);
        hdr->addStretch();
        hdr->addWidget(arrow);
        vl->addLayout(hdr);

        QHBoxLayout *row1 = new QHBoxLayout();
        m_radioLabel = new QLabel("Radio: —", section);
        m_radioLabel->setObjectName("infoLabel");
        row1->addWidget(m_radioLabel);
        row1->addStretch();
        vl->addLayout(row1);

        QHBoxLayout *row2 = new QHBoxLayout();
        m_catLabel = new QLabel("CAT: —", section);
        m_catLabel->setObjectName("infoLabel");
        m_audioLabel = new QLabel("Audio: —", section);
        m_audioLabel->setObjectName("infoLabel");
        row2->addWidget(m_catLabel);
        row2->addSpacing(10);
        row2->addWidget(m_audioLabel);
        row2->addStretch();
        vl->addLayout(row2);

        m_tncLabel = new QLabel("", section);
        m_tncLabel->setObjectName("infoLabel");
        m_tncLabel->setVisible(false);
        vl->addWidget(m_tncLabel);

        main->addWidget(section);
        refreshInterfaces();

        // ---- Inline radio editor ----
        m_ifaceEditor = new QWidget(central);
        m_ifaceEditor->setVisible(false);
        m_ifaceEditor->setStyleSheet("background-color: #222; border: 1px solid #444; border-radius: 4px;");
        QVBoxLayout *el = new QVBoxLayout(m_ifaceEditor);
        el->setContentsMargins(8, 8, 8, 8);
        el->setSpacing(6);

        QLabel *rl = new QLabel("Select Radio", m_ifaceEditor);
        rl->setStyleSheet("color: #888; font-size: 10px;");
        el->addWidget(rl);

        m_radioCombo = new QComboBox(m_ifaceEditor);
        m_radioCombo->setStyleSheet("background: #2d2d2d; color: #e0e0e0; border: 1px solid #444; border-radius: 3px; padding: 3px;");
        el->addWidget(m_radioCombo);



        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *applyBtn  = new QPushButton("Apply",  m_ifaceEditor);
        QPushButton *cancelBtn = new QPushButton("Cancel", m_ifaceEditor);
        applyBtn->setStyleSheet("background: #2E7D32; color: #fff; border-radius: 3px; padding: 5px;");
        cancelBtn->setStyleSheet("background: #444; color: #e0e0e0; border-radius: 3px; padding: 5px;");
        btnRow->addWidget(applyBtn);
        btnRow->addWidget(cancelBtn);
        el->addLayout(btnRow);


        connect(applyBtn,  &QPushButton::clicked, this, &MainWindow::applyRadio);
        connect(cancelBtn, &QPushButton::clicked, this, &MainWindow::toggleIfaceEditor);

        main->addWidget(m_ifaceEditor);
    }
    main->addWidget(makeSep(central));

    // -----------------------------------------------------------------------
    // TOP 4 — Active mode label + STOP (both hidden when no mode running)
    // -----------------------------------------------------------------------
    {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(4);

        m_activeModeLabel = new QLabel("", central);
        m_activeModeLabel->setObjectName("activeModeLabel");
        m_activeModeLabel->setVisible(false);
        if (!m_activeMode.isEmpty()) {
            m_activeModeLabel->setText("● " + (m_activeModeName.isEmpty() ? m_activeMode : m_activeModeName));
            m_activeModeLabel->setVisible(true);
        }
        row->addWidget(m_activeModeLabel, 1);

        m_stopBtn = new QPushButton("STOP", central);
        m_stopBtn->setObjectName("stopBtn");
        m_stopBtn->setMinimumWidth(54);
        m_stopBtn->setVisible(!m_activeMode.isEmpty());
        connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
        row->addWidget(m_stopBtn);

        main->addLayout(row);

        // Process list — shown below active mode label, hidden when idle
        m_processBox = new QWidget(central);
        m_processBox->setVisible(false);
        QVBoxLayout *procLayout = new QVBoxLayout(m_processBox);
        procLayout->setContentsMargins(4, 0, 4, 4);
        procLayout->setSpacing(1);
        main->addWidget(m_processBox);
    }
    main->addWidget(makeSep(central));

    // -----------------------------------------------------------------------
    // MODE SECTIONS — accordion, touch-scrollable
    // -----------------------------------------------------------------------
    QScrollArea *scroll = new QScrollArea(central);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

    QWidget *sc = new QWidget();
    QVBoxLayout *sl = new QVBoxLayout(sc);
    sl->setContentsMargins(0, 0, 0, 60);
    sl->setSpacing(4);

    // Accordion: track currently open {header, content}
    using AccordionPair = std::pair<QPushButton*, QWidget*>;
    auto accordion = std::make_shared<AccordionPair>(nullptr, nullptr);

    auto addSection = [&](const QString &title, bool expanded = false) -> QVBoxLayout* {
        QPushButton *hdr = new QPushButton((expanded ? "  v  " : "  >  ") + title, sc);
        hdr->setObjectName("sectionHeader");
        QWidget *content = new QWidget(sc);
        content->setVisible(expanded);
        QVBoxLayout *cl = new QVBoxLayout(content);
        cl->setContentsMargins(6, 6, 6, 8);
        cl->setSpacing(5);

        if (expanded) {
            accordion->first  = hdr;
            accordion->second = content;
        }

        connect(hdr, &QPushButton::clicked, this, [hdr, content, title, accordion]() {
            if (accordion->second && accordion->second != content) {
                accordion->second->setVisible(false);
                QString t = accordion->first->text();
                t.replace("  v  ", "  >  ");
                accordion->first->setText(t);
            }
            bool opening = !content->isVisible();
            content->setVisible(opening);
            hdr->setText((opening ? "  v  " : "  >  ") + title);
            accordion->first  = opening ? hdr     : nullptr;
            accordion->second = opening ? content : nullptr;
        });

        sl->addWidget(hdr);
        sl->addWidget(content);
        return cl;
    };

    // Direct-launch button
    auto addModeBtn = [&](QVBoxLayout *cl, const QString &modeId, const QString &label) {
        QPushButton *btn = new QPushButton(label, sc);
        btn->setProperty("active", m_activeMode == modeId);
        connect(btn, &QPushButton::clicked, this, [this, modeId]() {
            onModeButtonClicked(modeId);
        });
        m_modeButtons[modeId] = btn;
        cl->addWidget(btn);
    };

    // Multi-mode expander: each modem button launches a different modeId
    using ModeList = QList<QPair<QString,QString>>;
    auto addMultiMode = [&](QVBoxLayout *cl, const QString &groupLabel, const ModeList &modes) {
        QPushButton *groupBtn = new QPushButton(groupLabel, sc);

        QWidget *modemWidget = new QWidget(sc);
        modemWidget->setVisible(false);
        QGridLayout *mg = new QGridLayout(modemWidget);
        mg->setContentsMargins(8, 2, 4, 6);
        mg->setSpacing(3);

        int col = 0, row = 0;
        for (const auto &m : modes) {
            QString modeId = m.first;
            QString mlbl   = m.second;
            QPushButton *mb = new QPushButton(mlbl, modemWidget);
            mb->setObjectName("modemBtn");
            mb->setProperty("active", m_activeMode == modeId);
            m_modeButtons[modeId] = mb;
            connect(mb, &QPushButton::clicked, this, [this, modeId, modemWidget]() {
                modemWidget->setVisible(false);
                onModeButtonClicked(modeId);
            });
            mg->addWidget(mb, row, col++);
            if (col >= 3) { col = 0; row++; }
        }

        connect(groupBtn, &QPushButton::clicked, this, [modemWidget]() {
            modemWidget->setVisible(!modemWidget->isVisible());
        });

        cl->addWidget(groupBtn);
        cl->addWidget(modemWidget);
    };

    // Param-mode expander: all modem buttons launch same modeId with different param
    using ModemList = QList<QPair<QString,QString>>;
    auto addParamMode = [&](QVBoxLayout *cl, const QString &modeId,
                            const QString &label, const ModemList &modems) {
        QPushButton *modeBtn = new QPushButton(label, sc);
        modeBtn->setProperty("active", m_activeMode == modeId);
        m_modeButtons[modeId] = modeBtn;

        QWidget *modemWidget = new QWidget(sc);
        modemWidget->setVisible(false);
        QGridLayout *mg = new QGridLayout(modemWidget);
        mg->setContentsMargins(8, 2, 4, 6);
        mg->setSpacing(3);

        int col = 0, row = 0;
        for (const auto &m : modems) {
            QString key  = m.first;
            QString mlbl = m.second;
            QPushButton *mb = new QPushButton(mlbl, modemWidget);
            mb->setObjectName("modemBtn");
            connect(mb, &QPushButton::clicked, this, [this, modeId, key, mlbl, modemWidget]() {
                modemWidget->setVisible(false);
                QJsonObject params;
                params["modem"] = key;
                m_supervisor->startMode(modeId, params);
                m_userConfig->addToRecent(modeId + ":" + key);
                refreshRecentModes();
                setActiveMode(modeId, m_modeLoader->nameForId(modeId, m_language) + " (" + mlbl + ")");
                fastPoll();
            });
            mg->addWidget(mb, row, col++);
            if (col >= 3) { col = 0; row++; }
        }

        connect(modeBtn, &QPushButton::clicked, this, [modemWidget]() {
            modemWidget->setVisible(!modemWidget->isVisible());
        });

        cl->addWidget(modeBtn);
        cl->addWidget(modemWidget);
    };

    // --- RECENT MODES ---
    {
        QLabel *recentTitle = new QLabel((m_language == "fr" ? "⭐ RÉCENTS" : "⭐ RECENT"), sc);
        recentTitle->setObjectName("sectionTitle");
        recentTitle->setContentsMargins(8, 6, 0, 2);
        sl->addWidget(recentTitle);

        QWidget *recentBox = new QWidget(sc);
        m_recentLayout = new QVBoxLayout(recentBox);
        m_recentLayout->setContentsMargins(6, 6, 6, 8);
        m_recentLayout->setSpacing(5);
        sl->addWidget(recentBox);
        sl->addWidget(makeSep(sc));

        refreshRecentModes();
    }

    // --- JSON-driven groups (system default + user overrides merged by MenuLoader) ---
    for (const MenuLoader::Group &g : m_menu->menu()) {
        const QString title = (m_language == "fr" && !g.titleFr.isEmpty()) ? g.titleFr : g.titleEn;
        QVBoxLayout *cl = addSection(title);
        for (const MenuLoader::Item &it : g.items) {
            const QString label = MenuLoader::labelFor(it.label, it.labelTouch, /*touch*/ false);
            switch (it.type) {
            case MenuLoader::ItemType::Mode:
                addModeBtn(cl, it.id, label);
                break;
            case MenuLoader::ItemType::Multi: {
                ModeList ml;
                for (const MenuLoader::ModeRef &m : it.modes) {
                    ml.append({m.id, MenuLoader::labelFor(m.label, m.labelTouch, false)});
                }
                addMultiMode(cl, label, ml);
                break;
            }
            case MenuLoader::ItemType::Param: {
                ModemList opts;
                for (const MenuLoader::ParamOption &o : it.options) {
                    opts.append({o.value, MenuLoader::labelFor(o.label, o.labelTouch, false)});
                }
                addParamMode(cl, it.id, label, opts);
                break;
            }
            }
        }
    }

    sl->addStretch();
    scroll->setWidget(sc);
    main->addWidget(scroll, 1);

    // Bottom bar — STRIP collapse trigger on the left, LiaisonOS version on
    // the right. Whole bar is clickable; the labels are mouse-transparent so
    // clicks pass through to the QPushButton beneath. Comment out this block
    // to ship without Strip mode (the version label disappears with it).
    {
        QString ver;
        QFile mf("/opt/emcomm-tools/manifest.json");
        if (mf.open(QFile::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(mf.readAll());
            ver = doc.object().value("version").toString();
        }
        if (ver.isEmpty()) {
            QFile vf("/opt/emcomm-tools/version.txt");
            if (vf.open(QFile::ReadOnly))
                ver = QString::fromUtf8(vf.readAll()).trimmed();
        }

        QPushButton *bottomBar = new QPushButton(central);
        bottomBar->setFixedHeight(28);
        bottomBar->setCursor(Qt::PointingHandCursor);
        bottomBar->setStyleSheet(
            "QPushButton { background: #1a3a2a; color: #7de8b0; border: 1px solid #2a5a3a;"
            "  border-radius: 4px; text-align: left; padding: 0; }"
            "QPushButton:hover { background: #1f4432; }"
        );
        bottomBar->setToolTip("Collapse to strip mode");

        QHBoxLayout *bl = new QHBoxLayout(bottomBar);
        bl->setContentsMargins(10, 0, 10, 0);
        bl->setSpacing(0);

        QLabel *stripLbl = new QLabel("⮕ STRIP", bottomBar);
        stripLbl->setStyleSheet(
            "color: #7de8b0; font-size: 11px; font-weight: bold; background: transparent; border: none;"
        );
        stripLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        bl->addWidget(stripLbl);

        bl->addStretch();

        if (!ver.isEmpty()) {
            QLabel *verLbl = new QLabel("LiaisonOS v" + ver, bottomBar);
            verLbl->setStyleSheet(
                "color: #5a8a6a; font-size: 10px; background: transparent; border: none;"
            );
            verLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            bl->addWidget(verLbl);
        }

        connect(bottomBar, &QPushButton::clicked, this, [this]() { setStripMode(true); });
        main->addWidget(bottomBar);
    }

    // Snap to right edge, full available height (respects panel)
    QRect geo = QApplication::primaryScreen()->availableGeometry();
    setFixedSize(380, geo.height());
    move(geo.right() - 380 + 1, geo.top());
    show();
}

// ---------------------------------------------------------------------------
// Touch UI
// ---------------------------------------------------------------------------

void MainWindow::buildTouchUI()
{
    clearUI();
    setWindowOpacity(1.0);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    // Fill available screen (respects XFCE panel), stay below other apps
    QRect geo = QApplication::primaryScreen()->availableGeometry();
    setFixedSize(geo.width(), geo.height());
    move(geo.topLeft());
    show();

    QWidget *central = centralWidget();
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // -----------------------------------------------------------------------
    // TOP BAR
    // -----------------------------------------------------------------------
    QWidget *topBar = new QWidget(central);
    topBar->setStyleSheet("background-color: #111111;");
    topBar->setFixedHeight(70);
    QHBoxLayout *tb = new QHBoxLayout(topBar);
    tb->setContentsMargins(16, 8, 16, 8);
    tb->setSpacing(12);

    m_clockLabel = new QLabel("UTC --:--:--", topBar);
    m_clockLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #ffa500; font-family: monospace;");
    updateClock();
    tb->addWidget(m_clockLabel);

    tb->addStretch();

    static const QString topBtnSS =
        "QPushButton { background: #2d2d2d; color: #e0e0e0; border: 1px solid #404040;"
        "  border-radius: 6px; font-size: 15px; padding: 0 14px; }"
        "QPushButton:pressed { background: #444; }"
        "QPushButton:checked { background: #1a3a1a; color: #4ade80; border-color: #4ade80; }";

    QPushButton *syncBtn = new QPushButton("Sync", topBar);
    syncBtn->setFixedHeight(48);
    syncBtn->setStyleSheet(topBtnSS);
    connect(syncBtn, &QPushButton::clicked, this, []() {
        QProcess::execute("xhost", {"+local:root"});
        QProcess::startDetached("sudo", {"-E", "QtGpsSync"});
    });
    tb->addWidget(syncBtn);

    tb->addSpacing(6);

    m_trackBtn = new QPushButton("Track", topBar);
    m_trackBtn->setFixedHeight(48);
    m_trackBtn->setCheckable(true);
    m_trackBtn->setChecked(m_userConfig->tracking());
    m_trackBtn->setStyleSheet(topBtnSS);
    connect(m_trackBtn, &QPushButton::toggled, this, [this](bool on) {
        m_userConfig->setTracking(on);
        refreshOperator();
    });
    tb->addWidget(m_trackBtn);

    tb->addSpacing(6);

    m_gpsLabel = new QLabel("GPS: —", topBar);
    m_gpsLabel->setObjectName("gpsLabel");
    m_gpsLabel->setStyleSheet("font-size: 13px; color: #666;");
    tb->addWidget(m_gpsLabel);

    tb->addSpacing(12);

    QPushButton *desktopBtn = new QPushButton("☰", topBar);
    desktopBtn->setFixedSize(48, 48);
    desktopBtn->setStyleSheet("QPushButton { background: #2d2d2d; color: #e0e0e0; border-radius: 6px; font-size: 20px; }"
                              "QPushButton:pressed { background: #444; }");
    desktopBtn->setToolTip("Switch to Desktop Mode");
    connect(desktopBtn, &QPushButton::clicked, this, &MainWindow::onToggleTouchMode);
    tb->addWidget(desktopBtn);

    root->addWidget(topBar);

    // -----------------------------------------------------------------------
    // ACTIVE MODE BAR (hidden when idle)
    // -----------------------------------------------------------------------
    QWidget *modeBar = new QWidget(central);
    modeBar->setStyleSheet("background-color: #0d1f0d; border-bottom: 1px solid #1a4a1a;");
    modeBar->setFixedHeight(54);
    QHBoxLayout *mb = new QHBoxLayout(modeBar);
    mb->setContentsMargins(16, 6, 16, 6);

    m_activeModeLabel = new QLabel("", modeBar);
    m_activeModeLabel->setObjectName("activeModeLabel");
    m_activeModeLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #4ade80;");
    mb->addWidget(m_activeModeLabel, 1);

    m_processBox = new QWidget(modeBar);
    QHBoxLayout *pb = new QHBoxLayout(m_processBox);
    pb->setContentsMargins(0, 0, 0, 0);
    pb->setSpacing(8);
    mb->addWidget(m_processBox);

    m_stopBtn = new QPushButton("■ STOP", modeBar);
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setFixedHeight(40);
    m_stopBtn->setMinimumWidth(90);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    mb->addWidget(m_stopBtn);

    modeBar->setVisible(false);
    root->addWidget(modeBar);

    // Store modeBar pointer via property for setActiveMode to find
    central->setProperty("touchModeBar", QVariant::fromValue((QWidget*)modeBar));

    // -----------------------------------------------------------------------
    // SCROLL AREA — section cards
    // -----------------------------------------------------------------------
    QScrollArea *scroll = new QScrollArea(central);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet("QScrollArea { border: none; }");
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

    QWidget *sc = new QWidget();
    sc->setStyleSheet("background-color: #1a1a1a;");
    QVBoxLayout *sl = new QVBoxLayout(sc);
    sl->setContentsMargins(12, 12, 12, 12);
    sl->setSpacing(8);

    // Accordion tracking
    using AccordionPair = std::pair<QPushButton*, QWidget*>;
    auto accordion = std::make_shared<AccordionPair>(nullptr, nullptr);

    // Stylesheets
    const QString cardSS =
        "QPushButton { background-color: #252525; color: #e0e0e0;"
        "  border: 1px solid #3a3a3a; border-radius: 10px;"
        "  font-size: 14px; font-weight: bold; }"
        "QPushButton:pressed { background-color: #1a3a1a; color: #4ade80; border-color: #4ade80; }"
        "QPushButton[active=\"true\"] { background-color: #1a3a1a; color: #4ade80; border: 1px solid #4ade80; }";

    const QString modemBtnSS =
        "QPushButton { background-color: #1e2a1e; color: #7ec87e;"
        "  border: 1px solid #2a4a2a; border-radius: 10px; font-size: 15px; font-weight: bold; }"
        "QPushButton:pressed { background-color: #2a4a2a; color: #4ade80; border-color: #4ade80; }";

    // makeRow — creates a QWidget row inside a section, returns its QHBoxLayout
    auto makeRow = [](QVBoxLayout *sectionVL, QWidget *parent) -> QHBoxLayout* {
        QWidget *rowW = new QWidget(parent);
        QHBoxLayout *hl = new QHBoxLayout(rowW);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);
        sectionVL->addWidget(rowW);
        return hl;
    };

    // makePicker — creates a hidden picker widget below a row (with mode label)
    auto makePicker = [](QVBoxLayout *sectionVL, QWidget *parent) -> std::tuple<QWidget*, QLabel*, QHBoxLayout*> {
        QWidget *pw = new QWidget(parent);
        pw->setStyleSheet("background-color: #161f16; border-radius: 10px;");
        QVBoxLayout *vl = new QVBoxLayout(pw);
        vl->setContentsMargins(10, 8, 10, 8);
        vl->setSpacing(6);
        QLabel *titleLbl = new QLabel(pw);
        titleLbl->setAlignment(Qt::AlignCenter);
        titleLbl->setStyleSheet("color: #aaffaa; font-size: 13px; font-weight: bold; background: transparent;");
        vl->addWidget(titleLbl);
        QWidget *btnRow = new QWidget(pw);
        QHBoxLayout *hl = new QHBoxLayout(btnRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);
        vl->addWidget(btnRow);
        pw->setVisible(false);
        sectionVL->addWidget(pw);
        return {pw, titleLbl, hl};
    };

    // addTouchSection — returns the content QVBoxLayout
    auto addTouchSection = [&](const QString &title, bool expanded = false) -> QVBoxLayout* {
        QPushButton *hdr = new QPushButton((expanded ? "▼  " : "▶  ") + title, sc);
        hdr->setStyleSheet(
            "QPushButton { background-color: #2a2a2a; color: #cccccc; border: none;"
            "  border-radius: 8px; padding: 14px 16px; font-size: 15px; font-weight: bold; text-align: left; }"
            "QPushButton:pressed { background-color: #333; }");
        hdr->setMinimumHeight(52);

        QWidget *content = new QWidget(sc);
        content->setStyleSheet("background-color: #1e1e1e; border-radius: 0 0 8px 8px;");
        content->setVisible(expanded);
        QVBoxLayout *cl = new QVBoxLayout(content);
        cl->setContentsMargins(10, 8, 10, 10);
        cl->setSpacing(8);

        if (expanded) {
            accordion->first  = hdr;
            accordion->second = content;
        }

        connect(hdr, &QPushButton::clicked, sc, [accordion, hdr, content]() {
            if (accordion->second && accordion->second != content) {
                accordion->second->setVisible(false);
                accordion->first->setText(accordion->first->text().replace("▼  ", "▶  "));
            }
            bool open = !content->isVisible();
            content->setVisible(open);
            hdr->setText((open ? "▼  " : "▶  ") + hdr->text().mid(3));
            if (open) { accordion->first = hdr; accordion->second = content; }
            else      { accordion->first = nullptr; accordion->second = nullptr; }
        });

        sl->addWidget(hdr);
        sl->addWidget(content);
        return cl;
    };

    // -----------------------------------------------------------------------
    // OPERATOR + INTERFACES — side by side on one row, editors expand below
    // -----------------------------------------------------------------------
    {
        static const QString fieldSS =
            "background: #2d2d2d; color: #e0e0e0; border: 1px solid #444;"
            "border-radius: 6px; padding: 8px; font-size: 16px;";
        static const QString fieldLabelSS = "color: #888; font-size: 12px;";
        static const QString cardHdrSS =
            "background-color: #2a2a2a; border-radius: 8px;";

        // ---- Header row: OPERATOR card | INTERFACES card ----
        // Wrap in a QWidget so the layout has a proper parent before adding children
        QWidget *headerRow = new QWidget(sc);
        QHBoxLayout *headerHL = new QHBoxLayout(headerRow);
        headerHL->setContentsMargins(0, 0, 0, 0);
        headerHL->setSpacing(8);
        sl->addWidget(headerRow);

        // OPERATOR card
        QWidget *opSection = new QWidget(headerRow);
        opSection->setStyleSheet(cardHdrSS);
        opSection->setCursor(Qt::PointingHandCursor);
        opSection->installEventFilter(this);
        opSection->setProperty("action", "operator-edit");
        QVBoxLayout *opVL = new QVBoxLayout(opSection);
        opVL->setContentsMargins(14, 10, 14, 10);
        opVL->setSpacing(3);

        QHBoxLayout *opHdr = new QHBoxLayout();
        QLabel *opTitle = new QLabel("OPERATOR", opSection);
        opTitle->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
        QLabel *opPen = new QLabel("✎", opSection);
        opPen->setStyleSheet("color: #666; font-size: 16px;");
        opHdr->addWidget(opTitle);
        opHdr->addStretch();
        opHdr->addWidget(opPen);
        opVL->addLayout(opHdr);

        QHBoxLayout *opInfo = new QHBoxLayout();
        m_callsignLabel = new QLabel(m_userConfig->callsign(), opSection);
        m_callsignLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #ffa500;");
        QLabel *opSep = new QLabel(" — ", opSection);
        opSep->setStyleSheet("color: #444; font-size: 14px;");
        QString opGs = m_userConfig->gridSquare();
        m_gridLabel = new QLabel(opGs.isEmpty() ? "—" : opGs, opSection);
        m_gridLabel->setStyleSheet("font-size: 13px; color: #9e9e9e;");
        opInfo->addWidget(m_callsignLabel);
        opInfo->addWidget(opSep);
        opInfo->addWidget(m_gridLabel);
        opInfo->addStretch();
        opVL->addLayout(opInfo);
        headerHL->addWidget(opSection, 1);

        // INTERFACES card
        QWidget *ifSection = new QWidget(headerRow);
        ifSection->setStyleSheet(cardHdrSS);
        ifSection->setCursor(Qt::PointingHandCursor);
        ifSection->installEventFilter(this);
        ifSection->setProperty("action", "iface-edit");
        QVBoxLayout *ifVL = new QVBoxLayout(ifSection);
        ifVL->setContentsMargins(14, 10, 14, 10);
        ifVL->setSpacing(3);

        QHBoxLayout *ifHdr = new QHBoxLayout();
        QLabel *ifTitle = new QLabel("INTERFACES", ifSection);
        ifTitle->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
        QLabel *ifPen = new QLabel("✎", ifSection);
        ifPen->setStyleSheet("color: #666; font-size: 16px;");
        ifHdr->addWidget(ifTitle);
        ifHdr->addStretch();
        ifHdr->addWidget(ifPen);
        ifVL->addLayout(ifHdr);

        m_radioLabel = new QLabel("Radio: —", ifSection);
        m_radioLabel->setStyleSheet("font-size: 13px; color: #9e9e9e;");
        ifVL->addWidget(m_radioLabel);

        QHBoxLayout *ifRow2 = new QHBoxLayout();
        m_catLabel   = new QLabel("CAT: —", ifSection);
        m_audioLabel = new QLabel("Audio: —", ifSection);
        m_catLabel->setStyleSheet("font-size: 12px; color: #666;");
        m_audioLabel->setStyleSheet("font-size: 12px; color: #666;");
        ifRow2->addWidget(m_catLabel);
        ifRow2->addSpacing(10);
        ifRow2->addWidget(m_audioLabel);
        ifRow2->addStretch();
        ifVL->addLayout(ifRow2);

        m_tncLabel = new QLabel("", ifSection);
        m_tncLabel->setStyleSheet("font-size: 12px; color: #4ade80;");
        m_tncLabel->setVisible(false);
        ifVL->addWidget(m_tncLabel);

        headerHL->addWidget(ifSection, 1);

        refreshInterfaces();

        // ---- OPERATOR inline editor (below the row) ----
        m_operatorEditor = new QWidget(sc);
        m_operatorEditor->setVisible(false);
        m_operatorEditor->setStyleSheet("background-color: #1e1e1e; border: 1px solid #444; border-radius: 8px;");
        QVBoxLayout *oel = new QVBoxLayout(m_operatorEditor);
        oel->setContentsMargins(14, 14, 14, 14);
        oel->setSpacing(10);

        auto addTouchField = [&](const QString &lbl, QLineEdit *&field,
                                  const QString &placeholder, bool pw = false) {
            QLabel *l = new QLabel(lbl, m_operatorEditor);
            l->setStyleSheet(fieldLabelSS);
            oel->addWidget(l);
            field = new QLineEdit(m_operatorEditor);
            field->setPlaceholderText(placeholder);
            field->setMinimumHeight(46);
            field->setStyleSheet(fieldSS);
            if (pw) field->setEchoMode(QLineEdit::Password);
            oel->addWidget(field);
        };

        addTouchField("Callsign",         m_editCallsign, "e.g. VA2OPS");
        addTouchField("Grid Square",      m_editGrid,     "e.g. FN35fl");
        addTouchField("Name",             m_editName,     "Your name (optional)");
        addTouchField("Winlink Password", m_editPassword, "••••••••", true);

        QLabel *ll = new QLabel("Language", m_operatorEditor);
        ll->setStyleSheet(fieldLabelSS);
        oel->addWidget(ll);
        m_editLang = new QComboBox(m_operatorEditor);
        m_editLang->addItem("English", "en");
        m_editLang->addItem("Français", "fr");
        m_editLang->setMinimumHeight(46);
        m_editLang->setStyleSheet("background: #2d2d2d; color: #e0e0e0; border: 1px solid #444; border-radius: 6px; padding: 6px; font-size: 16px;");
        oel->addWidget(m_editLang);

        QHBoxLayout *opBtnRow = new QHBoxLayout();
        QPushButton *opSave   = new QPushButton("Save",   m_operatorEditor);
        QPushButton *opCancel = new QPushButton("Cancel", m_operatorEditor);
        opSave->setMinimumHeight(52);
        opCancel->setMinimumHeight(52);
        opSave->setStyleSheet("background: #2E7D32; color: #fff; border-radius: 6px; font-size: 16px; font-weight: bold;");
        opCancel->setStyleSheet("background: #444; color: #e0e0e0; border-radius: 6px; font-size: 16px;");
        opBtnRow->addWidget(opSave);
        opBtnRow->addWidget(opCancel);
        oel->addLayout(opBtnRow);

        connect(opSave,   &QPushButton::clicked, this, &MainWindow::saveOperator);
        connect(opCancel, &QPushButton::clicked, this, &MainWindow::toggleOperatorEditor);
        sl->addWidget(m_operatorEditor);

        // ---- INTERFACES inline editor (below the row) ----
        m_ifaceEditor = new QWidget(sc);
        m_ifaceEditor->setVisible(false);
        m_ifaceEditor->setStyleSheet("background-color: #1e1e1e; border: 1px solid #444; border-radius: 8px;");
        QVBoxLayout *iel = new QVBoxLayout(m_ifaceEditor);
        iel->setContentsMargins(14, 14, 14, 14);
        iel->setSpacing(10);

        QLabel *rl = new QLabel("Select Radio", m_ifaceEditor);
        rl->setStyleSheet("color: #888; font-size: 12px;");
        iel->addWidget(rl);

        m_radioCombo = new QComboBox(m_ifaceEditor);
        m_radioCombo->setMinimumHeight(50);
        m_radioCombo->setStyleSheet("background: #2d2d2d; color: #e0e0e0; border: 1px solid #444; border-radius: 6px; padding: 6px; font-size: 16px;");
        iel->addWidget(m_radioCombo);

        QHBoxLayout *ifBtnRow = new QHBoxLayout();
        QPushButton *ifApply  = new QPushButton("Apply",  m_ifaceEditor);
        QPushButton *ifCancel = new QPushButton("Cancel", m_ifaceEditor);
        ifApply->setMinimumHeight(52);
        ifCancel->setMinimumHeight(52);
        ifApply->setStyleSheet("background: #2E7D32; color: #fff; border-radius: 6px; font-size: 16px; font-weight: bold;");
        ifCancel->setStyleSheet("background: #444; color: #e0e0e0; border-radius: 6px; font-size: 16px;");
        ifBtnRow->addWidget(ifApply);
        ifBtnRow->addWidget(ifCancel);
        iel->addLayout(ifBtnRow);

        connect(ifApply,  &QPushButton::clicked, this, &MainWindow::applyRadio);
        connect(ifCancel, &QPushButton::clicked, this, &MainWindow::toggleIfaceEditor);
        sl->addWidget(m_ifaceEditor);
    }

    // addCard — simple mode card, captured by value in slot
    auto addCard = [&](QHBoxLayout *row, const QString &modeId, const QString &label) {
        QPushButton *btn = new QPushButton(label, sc);
        btn->setMinimumHeight(80);
        btn->setStyleSheet(cardSS);
        connect(btn, &QPushButton::clicked, this, [this, modeId]() { onModeButtonClicked(modeId); });
        m_modeButtons[modeId] = btn;
        row->addWidget(btn, 1);
    };

    // --- RECENT MODES (touch) ---
    {
        QVBoxLayout *cl = addTouchSection((m_language == "fr" ? "⭐ RÉCENTS" : "⭐ RECENT"), true);
        QWidget *cw = qobject_cast<QWidget*>(cl->parent());
        QHBoxLayout *row = makeRow(cl, cw);
        m_recentTouchLayout = new QHBoxLayout();
        m_recentTouchLayout->setSpacing(8);
        row->addLayout(m_recentTouchLayout, 1);
        refreshRecentModes();
    }

    // --- JSON-driven groups (system default + user overrides merged by MenuLoader) ---
    // Cards distribute 3-per-row inside each section, matching today's layout.
    const int CARDS_PER_ROW = 3;
    for (const MenuLoader::Group &g : m_menu->menu()) {
        const QString title = (m_language == "fr" && !g.titleFr.isEmpty()) ? g.titleFr : g.titleEn;
        QVBoxLayout *cl = addTouchSection(title);
        QWidget *cw = qobject_cast<QWidget*>(cl->parent());

        // One picker per section, shared by all multi/param items in this group.
        // Created lazily so groups with only simple modes don't get an empty picker widget.
        QWidget       *pw  = nullptr;
        QLabel        *lbl = nullptr;
        QHBoxLayout   *phl = nullptr;
        auto ensurePicker = [&]() {
            if (!pw) { auto t = makePicker(cl, cw); pw = std::get<0>(t); lbl = std::get<1>(t); phl = std::get<2>(t); }
        };

        QHBoxLayout *row = nullptr;
        int colsInRow = 0;
        auto nextRow = [&]() {
            if (row && colsInRow > 0 && colsInRow < CARDS_PER_ROW)
                row->addStretch(CARDS_PER_ROW - colsInRow);
            row = makeRow(cl, cw);
            colsInRow = 0;
        };

        for (const MenuLoader::Item &it : g.items) {
            if (colsInRow >= CARDS_PER_ROW || !row) nextRow();
            const QString itemLabel = MenuLoader::labelFor(it.label, it.labelTouch, /*touch*/ true);

            switch (it.type) {
            case MenuLoader::ItemType::Mode:
                addCard(row, it.id, itemLabel);
                break;

            case MenuLoader::ItemType::Multi: {
                ensurePicker();
                QPushButton *btn = new QPushButton(itemLabel, cw);
                btn->setMinimumHeight(80);
                btn->setStyleSheet(cardSS);
                // Capture modes by value so the lambda owns its own copy
                QList<QPair<QString,QString>> ml;
                for (const MenuLoader::ModeRef &m : it.modes) {
                    ml.append({m.id, MenuLoader::labelFor(m.label, m.labelTouch, true)});
                }
                connect(btn, &QPushButton::clicked, this,
                        [this, pw, lbl, phl, modemBtnSS, ml, itemLabel]() {
                    bool open = pw->isVisible();
                    QLayoutItem *qi; while ((qi=phl->takeAt(0))) { if(qi->widget()) qi->widget()->deleteLater(); delete qi; }
                    pw->setVisible(!open);
                    if (!open) {
                        lbl->setText("— " + itemLabel + " —");
                        for (const auto &m : ml) {
                            QPushButton *mb = new QPushButton(m.second, pw);
                            mb->setMinimumHeight(68); mb->setStyleSheet(modemBtnSS);
                            QString mid = m.first;
                            connect(mb, &QPushButton::clicked, this, [this, mid, pw](){
                                onModeButtonClicked(mid); pw->setVisible(false);
                            });
                            phl->addWidget(mb, 1);
                        }
                        phl->addStretch();
                    }
                });
                row->addWidget(btn, 1);
                break;
            }

            case MenuLoader::ItemType::Param: {
                ensurePicker();
                QPushButton *btn = new QPushButton(itemLabel, cw);
                btn->setMinimumHeight(80);
                btn->setStyleSheet(cardSS);
                const QString modeId   = it.id;
                const QString paramKey = it.paramKey.isEmpty() ? "modem" : it.paramKey;
                QList<QPair<QString,QString>> ol;
                for (const MenuLoader::ParamOption &o : it.options) {
                    ol.append({o.value, MenuLoader::labelFor(o.label, o.labelTouch, true)});
                }
                connect(btn, &QPushButton::clicked, this,
                        [this, pw, lbl, phl, modemBtnSS, ol, itemLabel, modeId, paramKey]() {
                    bool open = pw->isVisible();
                    QLayoutItem *qi; while ((qi=phl->takeAt(0))) { if(qi->widget()) qi->widget()->deleteLater(); delete qi; }
                    pw->setVisible(!open);
                    if (!open) {
                        lbl->setText("— " + itemLabel + " —");
                        for (const auto &o : ol) {
                            QPushButton *mb = new QPushButton(o.second, pw);
                            mb->setMinimumHeight(68); mb->setStyleSheet(modemBtnSS);
                            QString k = o.first;
                            connect(mb, &QPushButton::clicked, this,
                                    [this, modeId, paramKey, k, pw]() {
                                QJsonObject p; p[paramKey] = k;
                                m_supervisor->startMode(modeId, p);
                                setActiveMode(modeId, m_modeLoader->nameForId(modeId, m_language));
                                fastPoll();
                                pw->setVisible(false);
                            });
                            phl->addWidget(mb, 1);
                        }
                        phl->addStretch();
                    }
                });
                row->addWidget(btn, 1);
                break;
            }
            }
            ++colsInRow;
        }
        // Fill the last row to keep card sizing consistent (matches old hardcoded `row1->addStretch(1)`)
        if (row && colsInRow > 0 && colsInRow < CARDS_PER_ROW)
            row->addStretch(CARDS_PER_ROW - colsInRow);
    }

    sl->addStretch();
    scroll->setWidget(sc);
    root->addWidget(scroll, 1);

    refreshOperator();
}

// ---------------------------------------------------------------------------
// Strip UI — 80px collapsed sidebar (Desktop mode only)
// ---------------------------------------------------------------------------

void MainWindow::buildStripUI()
{
    clearUI();
    setWindowOpacity(1.0);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    const int STRIP_W = 80;
    const int TILE_H  = 50;

    QWidget *central = new QWidget(this);
    central->setStyleSheet(
        "QWidget         { background: #0d0d0d; color: #e0e0e0; }"
        "QPushButton     { background: #1a1a1a; color: #e0e0e0; border: 1px solid #2a2a2a; border-radius: 4px; }"
        "QPushButton:hover { background: #2a2a2a; }"
        "QLabel#tile_label { color: #888; font-size: 9px; }"
        "QLabel#tile_value { color: #e0e0e0; font-size: 11px; font-weight: bold; }"
    );
    setCentralWidget(central);

    QVBoxLayout *col = new QVBoxLayout(central);
    col->setContentsMargins(4, 4, 4, 4);
    col->setSpacing(4);

    // Helper: build a stacked tile (small label on top, big value below)
    auto makeTile = [&](QLabel *&valueOut, const QString &labelText) {
        QFrame *frame = new QFrame(central);
        frame->setFixedHeight(TILE_H);
        frame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
        QVBoxLayout *vl = new QVBoxLayout(frame);
        vl->setContentsMargins(4, 2, 4, 2);
        vl->setSpacing(0);
        if (!labelText.isEmpty()) {
            QLabel *lbl = new QLabel(labelText, frame);
            lbl->setObjectName("tile_label");
            lbl->setAlignment(Qt::AlignCenter);
            vl->addWidget(lbl);
        }
        valueOut = new QLabel(frame);
        valueOut->setObjectName("tile_value");
        valueOut->setAlignment(Qt::AlignCenter);
        vl->addWidget(valueOut, 1);
        col->addWidget(frame);
    };

    // Clock (HH:MM only — smaller font to fit at 80px)
    makeTile(m_clockLabel, "");
    m_clockLabel->setStyleSheet("color: #ffa500; font-size: 13px; font-weight: bold;");

    // GPS state — no sub-label, the colored text already says "GPS"
    makeTile(m_gpsLabel, "");

    // Operator tile — callsign in yellow on top, grid below in dim
    {
        QFrame *frame = new QFrame(central);
        frame->setFixedHeight(TILE_H);
        frame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
        QVBoxLayout *vl = new QVBoxLayout(frame);
        vl->setContentsMargins(4, 2, 4, 2);
        vl->setSpacing(0);
        m_callsignLabel = new QLabel(m_userConfig->callsign(), frame);
        m_callsignLabel->setAlignment(Qt::AlignCenter);
        m_callsignLabel->setStyleSheet("color: #fcc419; font-size: 12px; font-weight: bold;");
        vl->addWidget(m_callsignLabel);
        m_gridLabel = new QLabel(m_userConfig->gridSquare(), frame);
        m_gridLabel->setAlignment(Qt::AlignCenter);
        m_gridLabel->setStyleSheet("color: #888; font-size: 10px;");
        vl->addWidget(m_gridLabel);
        col->addWidget(frame);
    }

    // Rig model tile — short form (first word, parens stripped) populated by refreshInterfaces
    {
        QFrame *frame = new QFrame(central);
        frame->setFixedHeight(TILE_H - 22);  // single-line, compact
        frame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
        QVBoxLayout *vl = new QVBoxLayout(frame);
        vl->setContentsMargins(4, 2, 4, 2);
        vl->setSpacing(0);
        m_radioLabel = new QLabel(frame);
        m_radioLabel->setAlignment(Qt::AlignCenter);
        m_radioLabel->setStyleSheet("color: #9dbfad; font-size: 11px; font-weight: bold;");
        vl->addWidget(m_radioLabel);
        col->addWidget(frame);
    }

    // CAT + AUDIO side-by-side — saves vertical space; refreshInterfaces
    // will set text/colour. Strip mode uses short labels ("CAT ✓" / "AUD ✓").
    {
        QFrame *frame = new QFrame(central);
        frame->setFixedHeight(TILE_H - 14);  // shorter — single-line content
        frame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
        QHBoxLayout *hl = new QHBoxLayout(frame);
        hl->setContentsMargins(2, 0, 2, 0);
        hl->setSpacing(2);
        m_catLabel = new QLabel(frame);
        m_catLabel->setAlignment(Qt::AlignCenter);
        m_catLabel->setStyleSheet("font-size: 10px; font-weight: bold;");
        hl->addWidget(m_catLabel, 1);
        m_audioLabel = new QLabel(frame);
        m_audioLabel->setAlignment(Qt::AlignCenter);
        m_audioLabel->setStyleSheet("font-size: 10px; font-weight: bold;");
        hl->addWidget(m_audioLabel, 1);
        col->addWidget(frame);
    }

    // Active mode + Stop button — appears only when a mode is running
    QFrame *modeFrame = new QFrame(central);
    modeFrame->setFixedHeight(TILE_H + 4);
    modeFrame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
    QVBoxLayout *modeVL = new QVBoxLayout(modeFrame);
    modeVL->setContentsMargins(4, 2, 4, 2);
    modeVL->setSpacing(2);
    m_activeModeLabel = new QLabel(modeFrame);
    m_activeModeLabel->setObjectName("tile_value");
    m_activeModeLabel->setAlignment(Qt::AlignCenter);
    // Same green as the desktop UI's #activeModeLabel selector for consistency
    m_activeModeLabel->setStyleSheet("color: #4ade80; font-size: 11px; font-weight: bold;");
    m_activeModeLabel->setVisible(false);
    modeVL->addWidget(m_activeModeLabel);

    // Square stop button — width = height
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    m_stopBtn = new QPushButton("■", modeFrame);
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setFixedSize(26, 26);
    m_stopBtn->setStyleSheet("background: #c92a2a; color: white; border-radius: 4px; font-weight: bold;");
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    btnRow->addStretch();
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    modeVL->addLayout(btnRow);
    col->addWidget(modeFrame);

    // Recent modes — populated by refreshRecentModes() using m_recentStripLayout
    {
        QFrame *frame = new QFrame(central);
        frame->setStyleSheet("QFrame { background: #131313; border-radius: 4px; }");
        m_recentStripLayout = new QVBoxLayout(frame);
        m_recentStripLayout->setContentsMargins(4, 6, 4, 6);
        m_recentStripLayout->setSpacing(6);  // breathing room between square buttons
        col->addWidget(frame);
    }

    col->addStretch();

    // Expand button at bottom
    QPushButton *expandBtn = new QPushButton("⮜", central);
    expandBtn->setFixedHeight(32);
    expandBtn->setStyleSheet("background: #1f4432; color: #7de8b0; border: 1px solid #2a5a3a; border-radius: 4px; font-size: 16px; font-weight: bold;");
    expandBtn->setToolTip("Expand dashboard");
    connect(expandBtn, &QPushButton::clicked, this, [this]() { setStripMode(false); });
    col->addWidget(expandBtn);

    // Snap to right edge at strip width
    QRect geo = QApplication::primaryScreen()->availableGeometry();
    setFixedSize(STRIP_W, geo.height());
    move(geo.right() - STRIP_W + 1, geo.top());
    show();

    // Trigger initial population of dynamic fields
    updateClock();
    refreshInterfaces();
    refreshRecentModes();
    if (!m_activeMode.isEmpty()) {
        QString fallback = m_activeModeName.split(' ').first().toUpper();
        m_activeModeLabel->setText(
            m_menu ? m_menu->stripLabelFor(m_activeMode, fallback) : fallback);
        m_activeModeLabel->setVisible(true);
        m_stopBtn->setVisible(true);
    }
}

// ---------------------------------------------------------------------------
// Interfaces inline editor
// ---------------------------------------------------------------------------

static QList<QJsonObject> loadRadios()
{
    QList<QJsonObject> radios;
    QDir dir("/opt/emcomm-tools/conf/radios.d");
    for (const QString &fn : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        if (fn == "active-radio.json") continue;
        QFile f(dir.filePath(fn));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        radios.append(obj);
    }
    return radios;
}

static QString activeRadioId()
{
    QFileInfo fi("/opt/emcomm-tools/conf/radios.d/active-radio.json");
    if (fi.isSymLink())
        return QFileInfo(fi.symLinkTarget()).completeBaseName();
    return QString();
}

void MainWindow::toggleIfaceEditor()
{
    if (!m_ifaceEditor || !m_radioCombo) return;

    bool opening = !m_ifaceEditor->isVisible();
    if (opening) {
        m_radioCombo->clear();
        m_radioCombo->addItem("— No Radio —", QString());
        QString active = activeRadioId();
        int activeIdx = 0;
        for (const QJsonObject &r : loadRadios()) {
            QString id       = r.value("id").toString(r.value("file").toString());
            QString vendor   = r.value("vendor").toString(r.value("manufacturer").toString());
            QString model    = r.value("model").toString();
            QString label    = vendor.isEmpty() ? model : vendor + " " + model;
            // Build notes string
            QStringList notes;
            for (const QJsonValue &n : r.value("notes").toArray())
                notes << n.toString();
            m_radioCombo->addItem(label, id);
            m_radioCombo->setItemData(m_radioCombo->count()-1, notes.join('\n'), Qt::UserRole + 1);
            if (!active.isEmpty() && id == active)
                activeIdx = m_radioCombo->count()-1;
        }
        m_radioCombo->setCurrentIndex(activeIdx);
    }
    m_ifaceEditor->setVisible(opening);
}

void MainWindow::applyRadio()
{
    if (!m_radioCombo) return;

    QString radioId = m_radioCombo->currentData(Qt::DisplayRole).isNull()
                          ? m_radioCombo->currentData().toString()
                          : m_radioCombo->currentData().toString();

    // Set active-radio symlink
    QString linkPath = "/opt/emcomm-tools/conf/radios.d/active-radio.json";
    QFile::remove(linkPath);
    if (!radioId.isEmpty()) {
        QString target = "/opt/emcomm-tools/conf/radios.d/" + radioId + ".json";
        QFile::link(target, linkPath);
    }

    // Build config notes for dialog + document
    QString notes = m_radioCombo->currentData(Qt::UserRole + 1).toString();
    QString label = m_radioCombo->currentText();

    // Save document to ~/Documents
    if (!radioId.isEmpty() && !notes.isEmpty()) {
        QString docsPath = QDir::homePath() + "/Documents";
        QDir().mkpath(docsPath);
        QString safeName = label.replace(' ', '_').replace('/', '-');
        QString docFile  = docsPath + "/liaisonos-radio-" + safeName + ".txt";
        QFile doc(docFile);
        if (doc.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream s(&doc);
            s << "LiaisonOS — Radio Configuration\n";
            s << QString(60, '=') + "\n";
            s << "Radio: " + label + "\n\n";
            s << "Configuration Notes:\n";
            for (const QString &line : notes.split('\n'))
                s << "  • " + line + "\n";
            s << "\n" + QString(60, '=') + "\n";
            doc.close();
        }
    }

    m_ifaceEditor->setVisible(false);
    refreshInterfaces();

    // Show config notes dialog
    if (!notes.isEmpty()) {
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle("Radio Configuration — " + label);
        dlg->setMinimumWidth(420);
        QVBoxLayout *dl = new QVBoxLayout(dlg);
        dl->setSpacing(10);
        dl->setContentsMargins(16, 16, 16, 16);

        QLabel *title = new QLabel("<b>Apply these settings on your radio:</b>", dlg);
        title->setStyleSheet("color: #e0e0e0;");
        dl->addWidget(title);

        QLabel *body = new QLabel(dlg);
        body->setStyleSheet("color: #9dbfad; font-family: monospace; font-size: 11px; background: #111; padding: 8px; border-radius: 4px;");
        QStringList lines = notes.split('\n');
        QStringList html;
        for (const QString &l : lines)
            html << "• " + l.toHtmlEscaped();
        body->setText(html.join("<br>"));
        body->setWordWrap(true);
        dl->addWidget(body);

        if (!radioId.isEmpty()) {
            QLabel *saved = new QLabel("📄 Saved to ~/Documents/liaisonos-radio-" +
                                       label.replace(' ', '_').replace('/', '-') + ".txt", dlg);
            saved->setStyleSheet("color: #666; font-size: 10px;");
            saved->setWordWrap(true);
            dl->addWidget(saved);
        }

        QPushButton *ok = new QPushButton("OK — I've configured my radio", dlg);
        ok->setStyleSheet("background: #2E7D32; color: #fff; padding: 8px; border-radius: 4px;");
        connect(ok, &QPushButton::clicked, dlg, &QDialog::accept);
        dl->addWidget(ok);

        dlg->setStyleSheet("background-color: #1a1a1a; color: #e0e0e0;");
        dlg->exec();
        dlg->deleteLater();
    }
}

// ---------------------------------------------------------------------------
// Event filter
// ---------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w) {
            QString action = w->property("action").toString();
            if (action == "operator-edit") {
                toggleOperatorEditor();
                return true;
            } else if (action == "iface-edit") {
                toggleIfaceEditor();
                return true;
            }
            // Strip-mode recent-mode card click → launch the mode
            QString recentId = w->property("liaisonos_mode_id").toString();
            if (!recentId.isEmpty()) {
                onModeButtonClicked(recentId);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onTouchModeChanged(bool enabled)
{
    // Strip mode is a Desktop sub-mode; entering Touch mode clears it so
    // that toggling Touch off later returns to full Desktop, not Strip.
    m_stripMode = false;
    if (enabled) buildTouchUI();
    else         buildDesktopUI();
}

void MainWindow::onToggleTouchMode()
{
    m_userConfig->setTouchMode(!m_userConfig->touchMode());
}

void MainWindow::onModeButtonClicked(const QString &modeId)
{
    // modeId may be a composite "id:modem" stored by addParamMode
    QString baseId = modeId;
    QString modem;
    int sep = modeId.indexOf(':');
    if (sep >= 0) {
        baseId = modeId.left(sep);
        modem  = modeId.mid(sep + 1);
    }

    m_userConfig->addToRecent(modeId);
    refreshRecentModes();

    if (!modem.isEmpty()) {
        QJsonObject params;
        params["modem"] = modem;
        m_supervisor->startMode(baseId, params);
        setActiveMode(baseId, m_modeLoader->nameForId(baseId, m_language) + " (" + modem + ")");
    } else {
        m_supervisor->startMode(baseId);
        setActiveMode(baseId, m_modeLoader->nameForId(baseId, m_language));
    }
    fastPoll();
}

void MainWindow::fastPoll()
{
    // Poll a few times after a mode action to get quick feedback
    m_statusTimer->stop();
    int *count = new int(0);
    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this, t, count]() {
        m_supervisor->requestStatus();
        if (++(*count) >= 3) {
            t->stop();
            t->deleteLater();
            delete count;
            m_statusTimer->start(5000);
        }
    });
    t->start(2000);
}

void MainWindow::onStopClicked()
{
    m_supervisor->stopAll();
    setActiveMode(QString());
    fastPoll();
}

void MainWindow::onStatusReceived(const QJsonObject &status)
{
    QString st = status.value("status").toString();
    if (st != "ok") {
        QString msg = status.value("message").toString();
        if (msg.isEmpty())
            msg = (m_language == "fr") ? "et-supervisor n'est pas en cours."
                                       : "et-supervisor is not running.";
        QString title = (m_language == "fr") ? "Erreur du superviseur"
                                             : "Supervisor Error";
        setActiveMode(QString());
        QMessageBox::critical(this, title, msg);
        return;
    }

    QString mode     = status.value("mode").toString();
    QString modeName = status.value("mode_name").toString();
    if (modeName.isEmpty())
        modeName = m_modeLoader->nameForId(mode, m_language);
    setActiveMode(mode, modeName);

    // Update process list
    if (!m_processBox) return;

    // Clear old entries
    qDeleteAll(m_processBox->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly));
    QVBoxLayout *procLayout = qobject_cast<QVBoxLayout*>(m_processBox->layout());

    QJsonArray processes = status.value("processes").toArray();
    bool hasProcs = !processes.isEmpty() && !mode.isEmpty();
    m_processBox->setVisible(hasProcs);

    if (!hasProcs || !procLayout) return;

    QString lastCrash;
    for (const QJsonValue &v : processes) {
        QJsonObject p   = v.toObject();
        QString name    = p.value("name").toString();
        QString state   = p.value("state").toString();
        QString pid     = p.value("pid").toVariant().toString();

        QString color, symbol;
        if (state == "RUNNING")              { color = "#4ade80"; symbol = "●"; }
        else if (state == "CRASHED" ||
                 state == "FAILED")          { color = "#f87171"; symbol = "✗"; lastCrash = name; }
        else                                 { color = "#666666"; symbol = "○"; }

        QString text = QString("<span style='color:%1;'>%2</span>"
                               " <span style='color:#888;'>%3</span>")
                           .arg(color, symbol, name.toHtmlEscaped());
        if (!pid.isEmpty() && state == "RUNNING")
            text += QString(" <span style='color:#555;'>(%1)</span>").arg(pid);

        QLabel *lbl = new QLabel(m_processBox);
        lbl->setTextFormat(Qt::RichText);
        lbl->setText(text);
        lbl->setStyleSheet("font-size: 11px;");
        procLayout->addWidget(lbl);
    }

    // Crash notification
    if (!lastCrash.isEmpty() && lastCrash != m_lastCrash) {
        m_lastCrash = lastCrash;
        QProcess::startDetached("notify-send", {"-u", "critical", "-i", "dialog-error",
                                                "LiaisonOS", "Process crashed: " + lastCrash});
    } else if (lastCrash.isEmpty()) {
        m_lastCrash.clear();
    }
}

void MainWindow::refreshStatus()
{
    m_supervisor->requestStatus();
    refreshInterfaces();
}

void MainWindow::onScreenGeometryChanged()
{
    if (m_userConfig->touchMode())
        buildTouchUI();
    else if (m_stripMode)
        buildStripUI();
    else
        buildDesktopUI();
}

void MainWindow::onMenuChanged()
{
    // The dashboard-menu JSON (system default or user overrides) changed on
    // disk; rebuild the UI so the new menu structure is reflected. Reuses
    // the same full-rebuild path as a touch-mode toggle.
    if (m_userConfig->touchMode())
        buildTouchUI();
    else if (m_stripMode)
        buildStripUI();
    else
        buildDesktopUI();
}

void MainWindow::setStripMode(bool enabled)
{
    // No-op in touch mode (touch has its own fullscreen layout)
    if (m_userConfig->touchMode()) return;
    if (m_stripMode == enabled) return;
    m_stripMode = enabled;

    const int DESKTOP_W = 380;
    const int STRIP_W   = 80;
    const int targetW   = enabled ? STRIP_W : DESKTOP_W;
    const QRect geo     = QApplication::primaryScreen()->availableGeometry();

    QRect endRect(geo.right() - targetW + 1, geo.top(), targetW, geo.height());

    // Animate window width — right-anchored, so the LEFT edge moves while
    // the right edge stays put. 250ms InOutCubic per design.
    QPropertyAnimation *a = new QPropertyAnimation(this, "geometry", this);
    a->setDuration(250);
    a->setEasingCurve(QEasingCurve::InOutCubic);
    a->setStartValue(geometry());
    a->setEndValue(endRect);

    // Rebuild only AFTER the animation finishes, so the user sees a smooth
    // width shrink/grow of the OLD widget tree, then a clean swap to the
    // new tree once the target geometry is reached.
    connect(a, &QAbstractAnimation::finished, this, [this, enabled]() {
        if (enabled) buildStripUI();
        else         buildDesktopUI();
    });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// ---------------------------------------------------------------------------
// setActiveMode
// ---------------------------------------------------------------------------

void MainWindow::setActiveMode(const QString &modeId, const QString &modeName)
{
    if (m_activeMode == modeId && m_activeModeName == modeName)
        return;

    if (!m_activeMode.isEmpty() && m_modeButtons.contains(m_activeMode))
        m_modeButtons[m_activeMode]->setProperty("active", false);

    m_activeMode     = modeId;
    m_activeModeName = modeName.isEmpty() ? modeId : modeName;

    if (!modeId.isEmpty() && m_modeButtons.contains(modeId))
        m_modeButtons[modeId]->setProperty("active", true);

    for (QPushButton *btn : m_modeButtons) {
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }

    bool active = !modeId.isEmpty();
    if (m_activeModeLabel) {
        QString display;
        if (active) {
            if (m_stripMode) {
                // Strip mode: prefer dashboard-menu.json's label_touch; fall back
                // to first word of label (or m_activeModeName if mode isn't in the
                // menu at all). Group name is never prefixed.
                QString fallback = m_activeModeName.split(' ').first().toUpper();
                display = m_menu ? m_menu->stripLabelFor(modeId, fallback) : fallback;
            } else {
                display = "● " + m_activeModeName;
            }
        }
        m_activeModeLabel->setText(display);
        m_activeModeLabel->setVisible(active);
    }
    if (m_stopBtn)
        m_stopBtn->setVisible(active);

    // Touch mode bar (parent widget stored as property on centralWidget)
    if (centralWidget()) {
        QWidget *modeBar = centralWidget()->property("touchModeBar").value<QWidget*>();
        if (modeBar) modeBar->setVisible(active);
    }

    // Refresh recent buttons so their "active" highlight tracks the current
    // mode. Recent buttons live in their own layouts (desktop / touch / strip)
    // and aren't part of m_modeButtons, so they don't get the polish() above —
    // a fresh rebuild is the simplest way to keep their visual state in sync.
    refreshRecentModes();
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------

void MainWindow::updateClock()
{
    if (!m_clockLabel) return;
    if (m_stripMode) {
        // Strip mode: HH:MM only, no UTC prefix — saves horizontal space
        m_clockLabel->setText(QDateTime::currentDateTimeUtc().toString("HH:mm"));
    } else {
        m_clockLabel->setText("UTC " + QDateTime::currentDateTimeUtc().toString("HH:mm:ss"));
    }
}

void MainWindow::startClockTimer()
{
    int msToNext = 1000 - QDateTime::currentDateTimeUtc().time().msec();
    QTimer::singleShot(msToNext, this, [this]() {
        updateClock();
        m_clockTimer->start(1000);
    });
}

// ---------------------------------------------------------------------------
// GPS notify — receives JSON from QtGpsSync over /tmp/et-gps-notify.sock
// ---------------------------------------------------------------------------

void MainWindow::onGpsNotify()
{
    while (m_gpsNotifySocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(m_gpsNotifySocket->pendingDatagramSize());
        m_gpsNotifySocket->readDatagram(data.data(), data.size());

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("grid")) {
                // Save grid/lat/lon to user.json via UserConfig
                QString grid = obj.value("grid").toString();
                double  lat  = obj.value("lat").toDouble();
                double  lon  = obj.value("lon").toDouble();
                m_gpsActive = true;
                m_userConfig->updateGpsPosition(grid, lat, lon);
                if (m_gridLabel) {
                    m_gridLabel->setText(grid);
                    m_gridLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #4ade80;");
                }
                refreshInterfaces();   // flip GPS dot to green now, not at next poll tick
                return;
            }
        }

        // Plain text messages from GpsMonitor (continuous-mode lifecycle):
        //   gps:start    — gpsd just came up
        //   gps:running  — periodic pulse, fix is active
        //   gps:warn     — periodic pulse, fix LOST (signal dropped)
        //   gps:stop     — gpsd shutting down
        //   gps:closed   — socket / connection closed
        // The JSON-with-grid path above only fires on the one-shot Sync flow.
        QString msg = QString::fromUtf8(data).trimmed();
        if (msg == "gps:start" || msg == "gps:running") {
            if (!m_gpsActive) {
                m_gpsActive = true;
                refreshInterfaces();   // → GREEN
            }
        } else if (msg == "gps:warn") {
            // Signal lost while gpsd is still up — flip to yellow
            if (m_gpsActive) {
                m_gpsActive = false;
                refreshInterfaces();   // → YELLOW
            }
        } else if (msg == "gps:closed" || msg == "gps:stop") {
            m_gpsActive = false;
            if (m_gridLabel) {
                m_gridLabel->setStyleSheet(m_userConfig->tracking()
                    ? "font-size: 12px; font-weight: bold; color: #4ade80;"
                    : "font-size: 12px; color: #9e9e9e;");
            }
            refreshInterfaces();   // → YELLOW (or RED if device also gone)
        }
    }
}

// ---------------------------------------------------------------------------
// Operator refresh — callsign, grid (green when tracking)
// ---------------------------------------------------------------------------

void MainWindow::refreshOperator()
{
    if (m_callsignLabel)
        m_callsignLabel->setText(m_userConfig->callsign());

    if (m_gridLabel) {
        QString gs = m_userConfig->gridSquare();
        m_gridLabel->setText(gs.isEmpty() ? "—" : gs);
        m_gridLabel->setStyleSheet(m_userConfig->tracking()
            ? "font-size: 12px; font-weight: bold; color: #4ade80;"
            : "font-size: 12px; color: #9e9e9e;");
    }
}

// ---------------------------------------------------------------------------
// Recent modes refresh — called after every mode launch and on configChanged
// ---------------------------------------------------------------------------

void MainWindow::refreshRecentModes()
{
    QStringList recents = m_userConfig->recentModes();

    // Desktop
    if (m_recentLayout) {
        QLayoutItem *item;
        while ((item = m_recentLayout->takeAt(0))) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        for (const QString &rid : recents) {
            QString baseId = rid;
            QString modem;
            int sep = rid.indexOf(':');
            if (sep >= 0) { baseId = rid.left(sep); modem = rid.mid(sep + 1); }
            QString rname = m_modeLoader->nameForId(baseId, m_language);
            if (rname.isEmpty()) continue;
            if (!modem.isEmpty()) rname += " (" + modem + ")";
            QPushButton *rbtn = new QPushButton(rname);
            rbtn->setProperty("active", m_activeMode == baseId);
            connect(rbtn, &QPushButton::clicked, this, [this, rid]() {
                onModeButtonClicked(rid);
            });
            m_recentLayout->addWidget(rbtn);
        }
    }

    // Strip
    if (m_recentStripLayout) {
        QLayoutItem *item;
        while ((item = m_recentStripLayout->takeAt(0))) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        // Cap at 6 — matches the normal (desktop) Recent section count
        int shown = 0;
        for (const QString &rid : recents) {
            if (shown >= 6) break;
            QString baseId = rid;
            QString modem;
            int sep = rid.indexOf(':');
            if (sep >= 0) { baseId = rid.left(sep); modem = rid.mid(sep + 1); }

            // stripLabelFor handles the abbr-or-truncate logic; we just use its result.
            QString fallback = m_modeLoader->nameForId(baseId, m_language).toUpper();
            QString tag = m_menu ? m_menu->stripLabelFor(baseId, fallback) : fallback;
            if (tag.isEmpty()) continue;

            QString icon = m_menu ? m_menu->stripIconFor(baseId) : QString();

            // Use a QFrame with stacked QLabels — lets us size the icon big
            // and the text small on the same "button" (QPushButton text uses
            // a single font size for all lines, which doesn't suit icon+label).
            QFrame *card = new QFrame();
            card->setFixedSize(60, 60);
            card->setCursor(Qt::PointingHandCursor);
            const bool isActive = (m_activeMode == baseId);
            card->setProperty("active", isActive);
            card->setStyleSheet(
                "QFrame { background: #1a1a1a; border: 1px solid #2a2a2a; border-radius: 6px; }"
                "QFrame[active=\"true\"] { background: #1a3a1a; border: 1px solid #4ade80; }"
            );

            QVBoxLayout *cl = new QVBoxLayout(card);
            cl->setContentsMargins(2, 2, 2, 2);
            cl->setSpacing(0);

            if (!icon.isEmpty()) {
                QLabel *iconLbl = new QLabel(icon, card);
                iconLbl->setAlignment(Qt::AlignCenter);
                iconLbl->setStyleSheet("background: transparent; border: none; font-size: 22px;");
                cl->addWidget(iconLbl, 2);
            }

            // Bottom label — show short tag, plus modem suffix (truncated) for param modes
            QString text = tag;
            if (!modem.isEmpty()) {
                QString mt = modem.toUpper();
                mt.remove(' ').remove('-');
                text += " " + mt.left(4);
            }
            QLabel *txtLbl = new QLabel(text, card);
            txtLbl->setAlignment(Qt::AlignCenter);
            txtLbl->setStyleSheet(QString(
                "background: transparent; border: none; font-size: 10px; font-weight: bold;"
                "color: %1;").arg(isActive ? "#4ade80" : "#e0e0e0"));
            cl->addWidget(txtLbl, 1);

            // Click handling — install an event filter on the frame to trigger
            // the existing mode-launch path
            card->installEventFilter(this);
            card->setProperty("liaisonos_mode_id", rid);

            m_recentStripLayout->addWidget(card, 0, Qt::AlignHCenter);
            ++shown;
        }
    }

    // Touch
    if (m_recentTouchLayout) {
        QLayoutItem *item;
        while ((item = m_recentTouchLayout->takeAt(0))) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        const QString cardSS =
            "QPushButton { background: #1e3a1e; color: #e8e8e8; border: 1px solid #3a6b3a;"
            "  border-radius: 10px; font-size: 15px; font-weight: bold;"
            "  text-align: center; }"
            "QPushButton:pressed { background: #2d5a2d; }";
        for (const QString &rid : recents) {
            QString baseId = rid;
            QString modem;
            int sep = rid.indexOf(':');
            if (sep >= 0) { baseId = rid.left(sep); modem = rid.mid(sep + 1); }
            QString rname = m_modeLoader->nameForId(baseId, m_language);
            if (rname.isEmpty()) continue;
            if (!modem.isEmpty()) rname += " (" + modem + ")";
            QPushButton *rbtn = new QPushButton(rname);
            rbtn->setMinimumHeight(80);
            rbtn->setStyleSheet(cardSS);
            connect(rbtn, &QPushButton::clicked, this, [this, rid]() {
                onModeButtonClicked(rid);
            });
            m_recentTouchLayout->addWidget(rbtn, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Operator inline editor
// ---------------------------------------------------------------------------

void MainWindow::toggleOperatorEditor()
{
    if (!m_operatorEditor) return;

    bool opening = !m_operatorEditor->isVisible();
    if (opening) {
        // Populate fields from current config
        m_editCallsign->setText(m_userConfig->callsign());
        m_editGrid->setText(m_userConfig->gridSquare());

        QFile f(QDir::homePath() + "/.config/liaisonos/user.json");
        if (f.open(QIODevice::ReadOnly)) {
            QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
            f.close();
            m_editName->setText(obj.value("name").toString());
            // Don't pre-fill password — security
        }
        int langIdx = m_editLang->findData(m_userConfig->language());
        if (langIdx >= 0) m_editLang->setCurrentIndex(langIdx);
    }
    m_operatorEditor->setVisible(opening);
}

void MainWindow::saveOperator()
{
    if (!m_operatorEditor) return;

    QString callsign = m_editCallsign->text().trimmed().toUpper();
    QString grid     = m_editGrid->text().trimmed().toUpper();
    QString name     = m_editName->text().trimmed();
    QString password = m_editPassword->text().trimmed();
    QString lang     = m_editLang->currentData().toString();

    if (callsign.isEmpty()) {
        m_editCallsign->setStyleSheet("background: #3a1a1a; color: #f87171; border: 1px solid #f87171; border-radius: 3px; padding: 4px;");
        return;
    }

    // Read, update, write user.json
    QString configPath = QDir::homePath() + "/.config/liaisonos/user.json";
    QFile f(configPath);
    QJsonObject obj;
    if (f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    obj["callsign"] = callsign;
    obj["grid"]     = grid;
    obj["name"]     = name;
    obj["language"] = lang;
    if (!password.isEmpty())
        obj["winlinkPasswd"] = password;

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    }

    m_editPassword->clear();
    m_operatorEditor->setVisible(false);
    // UserConfig watcher will pick up the change and emit configChanged
}

// ---------------------------------------------------------------------------
// Interface refresh
// ---------------------------------------------------------------------------

void MainWindow::refreshInterfaces()
{
    if (m_gpsLabel) {
        bool gpsOk = QFile::exists("/dev/et-gps");
        // Three-state GPS — based on fix activity, not the Track toggle:
        //   red    — no GPS source (/dev/et-gps missing)
        //   yellow — GPS source exists but no fix yet (waiting / stale)
        //   green  — GPS source exists AND receiving fixes (m_gpsActive)
        QString color, mark;
        if (!gpsOk) {
            color = "#cc0000"; mark = "✗";
        } else if (m_gpsActive) {
            color = "#4ade80"; mark = "✓";   // fix active
        } else {
            color = "#fcc419"; mark = "●";   // gps available, no fix yet
        }
        m_gpsLabel->setText("GPS: " + mark);
        m_gpsLabel->setStyleSheet("color: " + color + ";");
        if (m_trackBtn)
            m_trackBtn->setEnabled(gpsOk);
    }

    // Radio config drives CAT logic (rigId == 1 means no rig control).
    // Load it even in Strip mode, where m_radioLabel doesn't exist, so we
    // can still compute CAT N/A vs ✓/✗ correctly for the CAT tile.
    const QString ACTIVE_RADIO = "/opt/emcomm-tools/conf/radios.d/active-radio.json";
    QString radioName = "Not configured";
    int rigId = -1;
    bool radioFileExists = false;
    QFile rf(ACTIVE_RADIO);
    if (rf.exists() && rf.open(QIODevice::ReadOnly)) {
        radioFileExists = true;
        QJsonDocument doc = QJsonDocument::fromJson(rf.readAll());
        rf.close();
        if (doc.isObject()) {
            radioName = doc.object().value("model").toString(radioName);
            rigId = doc.object().value("rigctrl").toObject().value("id").toInt(-1);
        }
    }
    if (m_radioLabel) {
        if (m_stripMode) {
            // Strip mode: drop the parenthetical (e.g. "FT-897D (Digirig)" → "FT-897D")
            // so the model fits in the 80px column.
            QString shortName = radioName;
            int paren = shortName.indexOf('(');
            if (paren > 0) shortName = shortName.left(paren).trimmed();
            m_radioLabel->setText(shortName);
        } else {
            m_radioLabel->setText("Radio: " + radioName);
        }
    }

    if (!m_catLabel || !m_audioLabel)
        return;

    // Strip mode uses shorter text + preserves the compact font we set in buildStripUI
    const QString stripFont = m_stripMode ? " font-size: 10px; font-weight: bold;" : "";

    if (!radioFileExists || rigId == 1) {
        m_catLabel->setText(m_stripMode ? "CAT —" : "CAT: N/A");
        m_catLabel->setStyleSheet(QString("color: #666666;%1").arg(stripFont));
    } else if (QFile::exists("/dev/et-cat")) {
        m_catLabel->setText(m_stripMode ? "CAT ✓" : "CAT: ✓");
        m_catLabel->setStyleSheet(QString("color: #4ade80;%1").arg(stripFont));
    } else {
        m_catLabel->setText(m_stripMode ? "CAT ✗" : "CAT: ✗");
        m_catLabel->setStyleSheet(QString("color: #cc0000;%1").arg(stripFont));
    }

    if (QFile::exists("/dev/et-audio")) {
        m_audioLabel->setText(m_stripMode ? "AUD ✓" : "Audio: ✓");
        m_audioLabel->setStyleSheet(QString("color: #4ade80;%1").arg(stripFont));
    } else {
        m_audioLabel->setText(m_stripMode ? "AUD ✗" : "Audio: ✗");
        m_audioLabel->setStyleSheet(QString("color: #cc0000;%1").arg(stripFont));
    }

    // TNC / VARA / Mercury IP:PORT label — shown only for relevant active modes
    if (m_tncLabel) {
        bool isTnc     = (m_activeMode == "direwolf-tnc");
        bool isVara    = (m_activeMode == "vara-hf" || m_activeMode == "vara-fm");
        bool isMercury = (m_activeMode == "mercury" || m_activeMode == "winlink-mercury");
        if (isTnc || isVara || isMercury) {
            QString ip = "?";
            QProcess p;
            p.start("hostname", {"-I"});
            p.waitForFinished(2000);
            QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            if (!out.isEmpty())
                ip = out.split(' ').first();

            if (isTnc) {
                QString port = "8001";
                QProcess chk;
                chk.start("ss", {"-tln", QString("sport = :%1").arg(port)});
                chk.waitForFinished(2000);
                bool up = chk.readAllStandardOutput().contains(port.toUtf8());
                m_tncLabel->setText(QString("TNC: %1 %2:%3").arg(up ? "✓" : "✗").arg(ip).arg(port));
                m_tncLabel->setStyleSheet(up ? "color: #4ade80;" : "color: #cc0000;");
            } else if (isMercury) {
                m_tncLabel->setText(QString("Mercury: %1:8300").arg(ip));
                m_tncLabel->setStyleSheet("color: #4ade80;");
            } else {
                m_tncLabel->setText(QString("VARA: %1:8300").arg(ip));
                m_tncLabel->setStyleSheet("color: #4ade80;");
            }
            m_tncLabel->setVisible(true);
        } else {
            m_tncLabel->setVisible(false);
        }
    }
}

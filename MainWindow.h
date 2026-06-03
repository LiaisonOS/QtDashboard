//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : QtDashboard main window.
//           Desktop Mode: fixed 320px sidebar with collapsible mode sections.
//           Touch Mode: fullscreen large-card layout.
//           Four top sections mirror et-dashboard: Time/GPS, Operator,
//           Interfaces, Active Mode bar.
//

#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QComboBox>
#include <QEvent>
#include <QUdpSocket>
#include <QFileSystemWatcher>
#include "UserConfig.h"
#include "SupervisorClient.h"
#include "ModeLoader.h"
#include "MenuLoader.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onTouchModeChanged(bool enabled);
    void onGpsNotify();
    void onStatusReceived(const QJsonObject &status);
    void onModeButtonClicked(const QString &modeId);
    void onStopClicked();
    void onToggleTouchMode();
    void refreshStatus();
    void updateClock();
    void refreshInterfaces();
    void refreshOperator();
    void refreshRecentModes();
    void fastPoll();
    void toggleOperatorEditor();
    void saveOperator();
    void toggleIfaceEditor();
    void applyRadio();
    void onScreenGeometryChanged();
    void onMenuChanged();

private:
    void buildDesktopUI();
    void buildTouchUI();
    void buildStripUI();
    void setStripMode(bool enabled);
    void clearUI();
    void applyStyleSheet();
    void startClockTimer();
    // The optional `modem` parameter is the param-mode option key (e.g.
    // "vara-hf"). It's stored separately from modeId so highlighting can
    // distinguish between e.g. QtTermTCP via VARA-HF vs via Mercury — without
    // it, both Recent buttons would glow when either is launched.
    void setActiveMode(const QString &modeId,
                       const QString &modeName = QString(),
                       const QString &modem = QString());
    bool eventFilter(QObject *obj, QEvent *event) override;

    Ui::MainWindow    *ui;
    UserConfig        *m_userConfig;
    SupervisorClient  *m_supervisor;
    ModeLoader        *m_modeLoader;
    MenuLoader        *m_menu;
    QTimer            *m_statusTimer;
    QTimer            *m_clockTimer;
    QUdpSocket        *m_gpsNotifySocket = nullptr;

    // Top section labels — reset in clearUI()
    QLabel       *m_clockLabel      = nullptr;
    QLabel       *m_gpsLabel        = nullptr;
    QPushButton  *m_trackBtn        = nullptr;
    QLabel       *m_callsignLabel   = nullptr;
    QLabel       *m_gridLabel       = nullptr;
    QLabel       *m_radioLabel      = nullptr;
    QLabel       *m_catLabel        = nullptr;
    QLabel       *m_audioLabel      = nullptr;
    QLabel       *m_tncLabel        = nullptr;
    QLabel       *m_activeModeLabel = nullptr;
    QPushButton  *m_stopBtn         = nullptr;
    QWidget      *m_processBox      = nullptr;

    // Recent modes section — refreshed on every mode launch
    QVBoxLayout  *m_recentLayout      = nullptr;  // desktop
    QHBoxLayout  *m_recentTouchLayout = nullptr;  // touch
    QVBoxLayout  *m_recentStripLayout = nullptr;  // strip

    // Operator inline editor
    QWidget      *m_operatorEditor  = nullptr;

    // Interfaces inline editor
    QWidget      *m_ifaceEditor     = nullptr;
    QComboBox    *m_radioCombo      = nullptr;
    QLabel       *m_radioNotesLabel = nullptr;
    QLineEdit    *m_editCallsign    = nullptr;
    QLineEdit    *m_editGrid        = nullptr;
    QLineEdit    *m_editName        = nullptr;
    QLineEdit    *m_editPassword    = nullptr;
    QComboBox    *m_editLang        = nullptr;

    QString       m_activeMode;
    QString       m_activeModeName;
    QString       m_activeModem;     // "" for non-param modes

    // Tiny in-line warning shown just above the Recent section.
    // Replaces the modal error dialog for soft errors (e.g. "mode already
    // running"). Two labels because Desktop and Touch dashboards live in
    // separate widget trees; only one is visible at a time.
    QLabel       *m_recentToast      = nullptr;
    QLabel       *m_recentToastTouch = nullptr;
    void          showRecentToast(const QString &msg, int durationMs = 3000);

    // Returns true if it's OK to start a new mode (nothing currently active).
    // Returns false AND shows the inline Recent toast if a mode is already
    // running — caller MUST abort. Operator is expected to press Stop or
    // close the mode's app before launching a different one. Replaces the
    // old "launch anyway, supervisor rejects, dialog appears" flow.
    bool          canStartNewMode();
    QString       m_language;
    QString       m_lastCrash;
    bool          m_gpsActive = false;
    bool          m_stripMode = false;

    QMap<QString, QPushButton*> m_modeButtons;

    // Toggle items (TOOLS → AUTOMATION). Map id → list-of-buttons (we may
    // have one per UI rebuild; Touch and Desktop layouts both render it,
    // and clearUI() doesn't always wipe the map before rebuild). State is
    // derived from the file existence test in onToggleStateChanged().
    QMap<QString, QList<QPushButton*>> m_toggleButtons;
    QMap<QString, QString>             m_toggleStateFiles;   // id → path
    QMap<QString, bool>                m_toggleOffWhenExists;// id → polarity
    QFileSystemWatcher                *m_toggleWatcher = nullptr;
    QPushButton  *makeToggleBtn(const MenuLoader::Item &it, QWidget *parent,
                                bool touchMode);
    void          updateToggleVisual(const QString &id);
    void          onToggleClicked(const QString &id);
    void          onToggleStateChanged(const QString &path);
    static bool   isFileToggleOn(const QString &path, bool offWhenExists);
    static QString expandHome(const QString &path);
};

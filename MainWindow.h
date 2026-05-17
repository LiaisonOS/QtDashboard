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
#include "UserConfig.h"
#include "SupervisorClient.h"
#include "ModeLoader.h"

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

private:
    void buildDesktopUI();
    void buildTouchUI();
    void clearUI();
    void applyStyleSheet();
    void startClockTimer();
    void setActiveMode(const QString &modeId, const QString &modeName = QString());
    bool eventFilter(QObject *obj, QEvent *event) override;

    Ui::MainWindow    *ui;
    UserConfig        *m_userConfig;
    SupervisorClient  *m_supervisor;
    ModeLoader        *m_modeLoader;
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
    QString       m_language;
    QString       m_lastCrash;
    bool          m_gpsActive = false;

    QMap<QString, QPushButton*> m_modeButtons;
};

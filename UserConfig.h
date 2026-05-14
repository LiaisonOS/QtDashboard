//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Reads and watches ~/.config/emcomm-tools/user.json
//           Provides touch_mode flag and user info to the rest of the app.
//           Uses QFileSystemWatcher to detect live changes — no restart needed.
//

#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QJsonObject>

class UserConfig : public QObject
{
    Q_OBJECT

public:
    explicit UserConfig(QObject *parent = nullptr);

    bool        touchMode()   const { return m_touchMode; }
    bool        tracking()    const { return m_tracking; }
    QString     callsign()    const { return m_callsign; }
    QString     gridSquare()  const { return m_gridSquare; }
    QString     language()    const { return m_language; }
    QStringList recentModes() const { return m_recentModes; }

    void setTouchMode(bool enabled);
    void setTracking(bool enabled);
    void addToRecent(const QString &modeId);
    void updateGpsPosition(const QString &grid, double lat, double lon);

signals:
    void touchModeChanged(bool enabled);
    void configChanged();

private slots:
    void onFileChanged(const QString &path);
    void onPollTimer();

private:
    void load();
    void save();
    void writeJson();

    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer  m_pollTimer;
    QJsonObject m_json;   // full in-memory copy of user.json
    QString m_lastGrid;

    bool        m_touchMode   = false;
    bool        m_tracking    = false;
    QString     m_callsign;
    QString     m_gridSquare;
    QString     m_language    = "en";
    QStringList m_recentModes;
};

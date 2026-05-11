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
#include <QString>

class UserConfig : public QObject
{
    Q_OBJECT

public:
    explicit UserConfig(QObject *parent = nullptr);

    bool    touchMode()  const { return m_touchMode; }
    bool    tracking()   const { return m_tracking; }
    QString callsign()   const { return m_callsign; }
    QString gridSquare() const { return m_gridSquare; }
    QString language()   const { return m_language; }

    void setTouchMode(bool enabled);
    void setTracking(bool enabled);

signals:
    void touchModeChanged(bool enabled);
    void configChanged();

private slots:
    void onFileChanged(const QString &path);

private:
    void load();
    void save();

    QString m_path;
    QFileSystemWatcher m_watcher;

    bool    m_touchMode  = false;
    bool    m_tracking   = false;
    QString m_callsign;
    QString m_gridSquare;
    QString m_language   = "en";
};

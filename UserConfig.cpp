//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Reads and watches ~/.config/emcomm-tools/user.json
//

#include "UserConfig.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>

UserConfig::UserConfig(QObject *parent)
    : QObject(parent)
{
    m_path = QDir::homePath() + "/.config/liaisonos/user.json";

    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &UserConfig::onFileChanged);

    load();
    m_lastGrid = m_gridSquare;
    m_watcher.addPath(m_path);
}

void UserConfig::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (doc.isNull() || !doc.isObject())
        return;

    m_json = doc.object();

    m_touchMode  = m_json.value("touch_mode").toBool(false);
    m_tracking   = m_json.value("tracking").toBool(false);
    m_callsign   = m_json.value("callsign").toString();
    m_gridSquare = m_json.value("grid").toString();
    m_language   = m_json.value("language").toString("en");

    m_recentModes.clear();
    for (const QJsonValue &v : m_json.value("recent_modes").toArray())
        m_recentModes.append(v.toString());

    // Sync the legacy /tmp/et-gps-tracking flag file used by et-repeater
    // and any other Flask app polling /api/tracking-status. This ensures
    // the flag reflects the persisted state even before any Track toggle.
    const QString flag = "/tmp/et-gps-tracking";
    if (m_tracking) {
        QFile f(flag);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.close();
    } else {
        QFile::remove(flag);
    }
}

void UserConfig::writeJson()
{
    m_watcher.removePath(m_path);
    QFile fw(m_path);
    if (!fw.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_watcher.addPath(m_path);
        return;
    }
    fw.write(QJsonDocument(m_json).toJson(QJsonDocument::Indented));
    fw.close();
    m_watcher.addPath(m_path);
}

void UserConfig::save()
{
    m_json["touch_mode"] = m_touchMode;
    m_json["tracking"]   = m_tracking;
    writeJson();
}

void UserConfig::setTouchMode(bool enabled)
{
    if (m_touchMode == enabled)
        return;
    m_touchMode = enabled;
    save();
    emit touchModeChanged(m_touchMode);
}

void UserConfig::setTracking(bool enabled)
{
    if (m_tracking == enabled)
        return;
    m_tracking = enabled;
    save();

    // Legacy flag file used by et-repeater (and any other Flask app polling
    // /api/tracking-status). The old Python/GTK et-dashboard used to create
    // this; QtDashboard must keep doing it so downstream apps see the same
    // tracking signal.
    const QString flag = "/tmp/et-gps-tracking";
    if (enabled) {
        QFile f(flag);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.close();
    } else {
        QFile::remove(flag);
    }
}

void UserConfig::addToRecent(const QString &modeId)
{
    m_recentModes.removeAll(modeId);
    m_recentModes.prepend(modeId);
    while (m_recentModes.size() > 6)
        m_recentModes.removeLast();

    QJsonArray arr;
    for (const QString &id : m_recentModes) arr.append(id);
    m_json["recent_modes"] = arr;
    writeJson();
}

void UserConfig::updateGpsPosition(const QString &grid, double lat, double lon)
{
    m_gridSquare     = grid;
    m_json["grid"]   = grid;
    m_json["lat"]    = lat;
    m_json["lon"]    = lon;
    m_lastGrid       = grid;
    writeJson();
}

void UserConfig::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    bool prevTouch = m_touchMode;
    load();
    m_lastGrid = m_gridSquare;
    // Re-add path — some editors replace the file (inotify loses track)
    m_watcher.addPath(m_path);
    emit configChanged();
    if (m_touchMode != prevTouch)
        emit touchModeChanged(m_touchMode);
}

void UserConfig::onPollTimer()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull() || !doc.isObject()) return;

    QString newGrid = doc.object().value("grid").toString();
    if (newGrid != m_lastGrid) {
        m_lastGrid = newGrid;
        bool prevTouch = m_touchMode;
        load();
        m_watcher.addPath(m_path);
        emit configChanged();
        if (m_touchMode != prevTouch)
            emit touchModeChanged(m_touchMode);
    }
}

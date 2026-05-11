//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Reads and watches ~/.config/emcomm-tools/user.json
//

#include "UserConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

UserConfig::UserConfig(QObject *parent)
    : QObject(parent)
{
    m_path = QDir::homePath() + "/.config/emcomm-tools/user.json";

    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &UserConfig::onFileChanged);

    load();
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

    QJsonObject obj = doc.object();
    m_touchMode  = obj.value("touch_mode").toBool(false);
    m_tracking   = obj.value("tracking").toBool(false);
    m_callsign   = obj.value("callsign").toString();
    m_gridSquare = obj.value("grid").toString();
    m_language   = obj.value("language").toString("en");
}

void UserConfig::save()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
    obj["touch_mode"] = m_touchMode;
    obj["tracking"]   = m_tracking;

    QFile fw(m_path);
    if (!fw.open(QIODevice::WriteOnly))
        return;

    fw.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    fw.close();
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
}

void UserConfig::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    load();
    // Re-add path — some editors replace the file (inotify loses track)
    m_watcher.addPath(m_path);
    emit configChanged();
    emit touchModeChanged(m_touchMode);
}

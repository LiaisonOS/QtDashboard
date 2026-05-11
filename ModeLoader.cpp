//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Loads mode JSON files from /opt/emcomm-tools/conf/modes.d/
//

#include "ModeLoader.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ModeLoader::ModeLoader(QObject *parent)
    : QObject(parent)
    , m_modesDir("/opt/emcomm-tools/conf/modes.d")
{
}

void ModeLoader::load()
{
    m_modes.clear();

    QDir dir(m_modesDir);
    if (!dir.exists())
        return;

    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

    for (const QString &file : files) {
        QFile f(dir.filePath(file));
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();

        if (doc.isNull() || !doc.isObject())
            continue;

        QJsonObject obj = doc.object();

        ModeInfo info;
        info.id       = obj.value("id").toString();
        info.category = obj.value("category").toString("other");
        info.icon     = obj.value("icon").toString();

        QJsonObject nameObj = obj.value("name").toObject();
        info.name["en"] = nameObj.value("en").toString(info.id);
        info.name["fr"] = nameObj.value("fr").toString(info.name["en"]);

        if (!info.id.isEmpty())
            m_modes.append(info);
    }
}

QString ModeLoader::nameForId(const QString &id, const QString &lang) const
{
    for (const ModeInfo &m : m_modes)
        if (m.id == id)
            return m.name.value(lang, m.name.value("en", id));
    return id;
}

QStringList ModeLoader::categories() const
{
    QStringList cats;
    for (const ModeInfo &m : m_modes) {
        if (!cats.contains(m.category))
            cats.append(m.category);
    }
    return cats;
}

//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Loads mode JSON files from /opt/emcomm-tools/conf/modes.d/
//           Parses id, name (EN/FR), category, icon for display in QtDashboard.
//

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QMap>

struct ModeInfo {
    QString id;
    QMap<QString, QString> name;  // "en" -> "VARA HF", "fr" -> "VARA HF"
    QString category;
    QString icon;
};

class ModeLoader : public QObject
{
    Q_OBJECT

public:
    explicit ModeLoader(QObject *parent = nullptr);

    void load();
    QList<ModeInfo> modes() const { return m_modes; }
    QStringList categories() const;
    QString nameForId(const QString &id, const QString &lang = "en") const;

private:
    QString m_modesDir;
    QList<ModeInfo> m_modes;
};

//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Loads dashboard-menu.json (system) + dashboard-menu.overrides.json (user)
//           and produces a merged, in-memory menu tree consumed by MainWindow.
//

#include "MenuLoader.h"
#include "ModeLoader.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

MenuLoader::MenuLoader(ModeLoader *modeLoader, QObject *parent)
    : QObject(parent)
    , m_modeLoader(modeLoader)
{
    // Create the override file with documentation if it doesn't exist —
    // gives the operator a discoverable place to start customizing.
    ensureOverridesTemplate();

    // Debounce for live reload — editors often write multiple times in
    // quick succession (truncate + write, atomic rename, etc.). Coalesce
    // those into one reload ~200ms after the last change.
    m_reloadDebounce = new QTimer(this);
    m_reloadDebounce->setSingleShot(true);
    m_reloadDebounce->setInterval(200);
    connect(m_reloadDebounce, &QTimer::timeout, this, &MenuLoader::doReload);

    // File watcher — watch the file itself AND its parent directory, so we
    // pick up both edits-in-place and create-from-nothing events.
    m_watcher = new QFileSystemWatcher(this);
    const QString dir = QFileInfo(overridesPath()).absolutePath();
    QDir().mkpath(dir);
    m_watcher->addPath(dir);
    rewatchOverrides();
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &MenuLoader::onOverrideFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &MenuLoader::onOverrideDirChanged);

    doReload();
}

QString MenuLoader::systemPath() const
{
    // Production path. If you need to dev-test outside of an installed system,
    // place a copy of dashboard-menu.json next to the QtDashboard binary —
    // the loader will fall back to it when the system file is missing.
    const QString prod = "/opt/emcomm-tools/conf/dashboard-menu.json";
    if (QFile::exists(prod)) return prod;

    const QString devLocal = QCoreApplication::applicationDirPath()
                             + "/dashboard-menu.json";
    if (QFile::exists(devLocal)) return devLocal;

    return prod; // returns the canonical path even if missing, for log clarity
}

QString MenuLoader::overridesPath() const
{
    // Canonical path under the new LiaisonOS branding (emcomm-tools is banned
    // naming due to the KT7RUN project conflict). This file is brand-new in
    // 2.3.4 so it lives natively at the new path with no legacy fallback —
    // older Qt apps' config files (user.json etc.) will migrate later via the
    // emcomm-tools→liaisonos symlink plan; see the migration project memory.
    return QDir::homePath() + "/.config/liaisonos/dashboard-menu.overrides.json";
}

void MenuLoader::reload()
{
    // Public API: schedule a (debounced) reload. Use doReload() for synchronous.
    if (m_reloadDebounce)
        m_reloadDebounce->start();
    else
        doReload();
}

void MenuLoader::doReload()
{
    QList<Group> sys = loadFromFile(systemPath());
    QJsonObject  ov  = loadJsonObject(overridesPath());
    m_merged = applyOverrides(sys, ov);
    rewatchOverrides();
    emit menuChanged();
}

void MenuLoader::onOverrideFileChanged(const QString &)
{
    // Atomic-rename writers (vim, etc.) remove the original inode; the
    // watcher drops the path. Re-add on next reload via rewatchOverrides().
    if (m_reloadDebounce) m_reloadDebounce->start();
}

void MenuLoader::onOverrideDirChanged(const QString &)
{
    // Catches the case where the override file is created from nothing.
    const QString p = overridesPath();
    if (QFile::exists(p) && m_watcher && !m_watcher->files().contains(p))
        m_watcher->addPath(p);
    if (m_reloadDebounce) m_reloadDebounce->start();
}

void MenuLoader::rewatchOverrides()
{
    if (!m_watcher) return;
    const QString p = overridesPath();
    if (QFile::exists(p) && !m_watcher->files().contains(p))
        m_watcher->addPath(p);
}

void MenuLoader::ensureOverridesTemplate()
{
    const QString p = overridesPath();
    if (QFile::exists(p)) return;

    QDir().mkpath(QFileInfo(p).absolutePath());

    QFile f(p);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    // Keys starting with `_` are inert — applyOverrides() only reads the
    // documented top-level keys (hide_modes, hide_groups, relabel, etc.).
    // The operator can copy any of the examples up to the top level to
    // activate it. Schema: https://liaisonos.com/docs/menu-overrides
    const QByteArray content =
R"({
  "_doc": "LiaisonOS Dashboard menu overrides — edit this file to customize your menu.",
  "_doc_keys": [
    "hide_modes",
    "hide_groups",
    "relabel",
    "add_to_group",
    "custom_groups",
    "group_order"
  ],
  "_examples": {
    "hide_a_mode":         { "hide_modes":  ["fldigi"] },
    "hide_an_entire_group":{ "hide_groups": ["aprs"] },
    "rename_a_mode":       { "relabel":     { "varac": "VarAC HF" } },
    "add_to_existing_group": {
      "add_to_group": {
        "messaging": [
          { "type": "mode", "id": "my-custom-mode", "label": "My Custom" }
        ]
      }
    },
    "add_a_custom_group": {
      "custom_groups": [
        {
          "id": "my-favs",
          "title_en": "My Favorites",
          "title_fr": "Mes Favoris",
          "icon": "⭐",
          "items": [
            { "type": "mode", "id": "varac",   "label": "VarAC quick-start" },
            { "type": "mode", "id": "js8call", "label": "JS8 quick-start" }
          ]
        }
      ]
    },
    "reorder_groups": { "group_order": ["my-favs", "messaging", "winlink"] }
  }
}
)";
    f.write(content);
    f.close();
    qInfo() << "MenuLoader: created override template at" << p;
}

QString MenuLoader::labelFor(const QString &label, const QString &labelTouch, bool touchMode)
{
    if (touchMode && !labelTouch.isEmpty())
        return labelTouch;
    return label;
}

QString MenuLoader::stripLabelFor(const QString &modeId, const QString &fallback) const
{
    // Priority chain for strip-mode tag:
    //  1) label_abbr  — explicit, trust user verbatim (no truncation)
    //  2) label_touch — uppercase, drop spaces/dashes, cap at 5 chars
    //  3) label       — first word, uppercase, drop separators, cap at 5
    //  4) fallback    — supplied by caller
    auto pick = [](const QString &label, const QString &labelTouch, const QString &labelAbbr) -> QString {
        if (!labelAbbr.isEmpty()) return labelAbbr.toUpper();
        auto compact = [](QString t) {
            t.remove(' ').remove('-');
            return t.left(5);
        };
        if (!labelTouch.isEmpty()) return compact(labelTouch.toUpper());
        if (!label.isEmpty())      return compact(label.split(' ').first().toUpper());
        return QString();
    };
    for (const Group &g : m_merged) {
        for (const Item &it : g.items) {
            if ((it.type == ItemType::Mode || it.type == ItemType::Param) && it.id == modeId) {
                QString r = pick(it.label, it.labelTouch, it.labelAbbr);
                return r.isEmpty() ? fallback : r;
            }
            if (it.type == ItemType::Multi) {
                for (const ModeRef &m : it.modes) {
                    if (m.id == modeId) {
                        QString r = pick(m.label, m.labelTouch, m.labelAbbr);
                        return r.isEmpty() ? fallback : r;
                    }
                }
            }
        }
    }
    return fallback;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

QJsonObject MenuLoader::loadJsonObject(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "MenuLoader:" << path << "parse error:" << err.errorString();
        return {};
    }
    return doc.object();
}

QList<MenuLoader::Group> MenuLoader::loadFromFile(const QString &path)
{
    QJsonObject root = loadJsonObject(path);
    if (root.isEmpty()) {
        qWarning() << "MenuLoader: no menu loaded from" << path;
        return {};
    }

    QList<Group> out;
    for (const QJsonValue &v : root.value("groups").toArray()) {
        if (!v.isObject()) continue;
        out.append(parseGroup(v.toObject()));
    }
    return out;
}

MenuLoader::Group MenuLoader::parseGroup(const QJsonObject &o)
{
    Group g;
    g.id      = o.value("id").toString();
    g.titleEn = o.value("title_en").toString();
    g.titleFr = o.value("title_fr").toString();
    g.icon    = o.value("icon").toString();

    for (const QJsonValue &v : o.value("items").toArray()) {
        if (!v.isObject()) continue;
        g.items.append(parseItem(v.toObject()));
    }
    return g;
}

MenuLoader::Item MenuLoader::parseItem(const QJsonObject &o)
{
    Item it;
    const QString t = o.value("type").toString();
    if      (t == "mode")  it.type = ItemType::Mode;
    else if (t == "multi") it.type = ItemType::Multi;
    else if (t == "param") it.type = ItemType::Param;
    else {
        qWarning() << "MenuLoader: unknown item type" << t;
        it.type = ItemType::Mode;
    }

    it.id         = o.value("id").toString();
    it.label      = o.value("label").toString();
    it.labelTouch = o.value("label_touch").toString();
    it.labelAbbr  = o.value("label_abbr").toString();
    it.stripIcon  = o.value("strip_icon").toString();
    it.paramKey   = o.value("param_key").toString();

    for (const QJsonValue &v : o.value("modes").toArray()) {
        if (!v.isObject()) continue;
        it.modes.append(parseModeRef(v.toObject()));
    }
    for (const QJsonValue &v : o.value("options").toArray()) {
        if (!v.isObject()) continue;
        it.options.append(parseParamOption(v.toObject()));
    }
    return it;
}

MenuLoader::ModeRef MenuLoader::parseModeRef(const QJsonObject &o)
{
    ModeRef m;
    m.id         = o.value("id").toString();
    m.label      = o.value("label").toString();
    m.labelTouch = o.value("label_touch").toString();
    m.labelAbbr  = o.value("label_abbr").toString();
    m.stripIcon  = o.value("strip_icon").toString();
    return m;
}

MenuLoader::ParamOption MenuLoader::parseParamOption(const QJsonObject &o)
{
    ParamOption p;
    p.value      = o.value("value").toString();
    p.label      = o.value("label").toString();
    p.labelTouch = o.value("label_touch").toString();
    p.labelAbbr  = o.value("label_abbr").toString();
    p.stripIcon  = o.value("strip_icon").toString();
    return p;
}

QString MenuLoader::stripIconFor(const QString &modeId) const
{
    for (const Group &g : m_merged) {
        for (const Item &it : g.items) {
            if ((it.type == ItemType::Mode || it.type == ItemType::Param) && it.id == modeId)
                return it.stripIcon;
            if (it.type == ItemType::Multi) {
                for (const ModeRef &m : it.modes) {
                    if (m.id == modeId) return m.stripIcon;
                }
            }
        }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Merge (defaults <- overrides)
// ---------------------------------------------------------------------------

QList<MenuLoader::Group> MenuLoader::applyOverrides(QList<Group> sys,
                                                    const QJsonObject &ov)
{
    if (ov.isEmpty()) return sys;

    // hide_modes / hide_groups
    QSet<QString> hideModes;
    for (const QJsonValue &v : ov.value("hide_modes").toArray())
        hideModes.insert(v.toString());

    QSet<QString> hideGroups;
    for (const QJsonValue &v : ov.value("hide_groups").toArray())
        hideGroups.insert(v.toString());

    // relabel — overrides "label" on Mode / Param / multi-child by id.
    QHash<QString, QString> relabel;
    const QJsonObject rl = ov.value("relabel").toObject();
    for (auto it = rl.constBegin(); it != rl.constEnd(); ++it)
        relabel.insert(it.key(), it.value().toString());

    // relabel_touch — overrides "labelTouch" on Mode / Param / multi-child by id.
    QHash<QString, QString> relabelTouch;
    const QJsonObject rlt = ov.value("relabel_touch").toObject();
    for (auto it = rlt.constBegin(); it != rlt.constEnd(); ++it)
        relabelTouch.insert(it.key(), it.value().toString());

    // relabel_groups — overrides group title (replaces BOTH titleEn AND titleFr
    // with the same string, regardless of active UI language).
    QHash<QString, QString> relabelGroups;
    const QJsonObject rlg = ov.value("relabel_groups").toObject();
    for (auto it = rlg.constBegin(); it != rlg.constEnd(); ++it)
        relabelGroups.insert(it.key(), it.value().toString());

    // Walk system groups, hide / relabel
    QList<Group> out;
    for (Group g : sys) {
        if (hideGroups.contains(g.id)) continue;

        // Group title override
        if (relabelGroups.contains(g.id)) {
            const QString nt = relabelGroups.value(g.id);
            g.titleEn = nt;
            g.titleFr = nt;
        }

        QList<Item> kept;
        for (Item it : g.items) {
            // Drop "mode" or "param" items hidden by id. From the operator's
            // perspective they're all mode-apps the dashboard launches, so we
            // treat them uniformly against the hide_modes set.
            if ((it.type == ItemType::Mode || it.type == ItemType::Param)
                    && hideModes.contains(it.id))
                continue;

            // Relabel "mode" / "param" items — desktop label + touch label.
            if (it.type == ItemType::Mode || it.type == ItemType::Param) {
                if (relabel.contains(it.id))      it.label      = relabel.value(it.id);
                if (relabelTouch.contains(it.id)) it.labelTouch = relabelTouch.value(it.id);
            }

            // Drop hidden mode refs inside "multi"; also relabel children.
            if (it.type == ItemType::Multi) {
                QList<ModeRef> kr;
                for (ModeRef m : it.modes) {
                    if (hideModes.contains(m.id)) continue;
                    if (relabel.contains(m.id))      m.label      = relabel.value(m.id);
                    if (relabelTouch.contains(m.id)) m.labelTouch = relabelTouch.value(m.id);
                    kr.append(m);
                }
                it.modes = kr;
                if (it.modes.isEmpty()) continue; // empty multi disappears
            }

            kept.append(it);
        }
        g.items = kept;
        out.append(g);
    }

    // add_to_group: append extra items to existing groups
    const QJsonObject add = ov.value("add_to_group").toObject();
    for (auto it = add.constBegin(); it != add.constEnd(); ++it) {
        const QString gid = it.key();
        const QJsonArray arr = it.value().toArray();
        for (int gi = 0; gi < out.size(); ++gi) {
            if (out[gi].id != gid) continue;
            for (const QJsonValue &v : arr) {
                if (!v.isObject()) continue;
                out[gi].items.append(parseItem(v.toObject()));
            }
        }
    }

    // custom_groups
    for (const QJsonValue &v : ov.value("custom_groups").toArray()) {
        if (!v.isObject()) continue;
        out.append(parseGroup(v.toObject()));
    }

    // group_order
    const QJsonArray order = ov.value("group_order").toArray();
    if (!order.isEmpty()) {
        QList<Group> reordered;
        QSet<QString> placed;
        for (const QJsonValue &v : order) {
            const QString gid = v.toString();
            for (const Group &g : out) {
                if (g.id == gid && !placed.contains(gid)) {
                    reordered.append(g);
                    placed.insert(gid);
                }
            }
        }
        // Append any groups not listed in group_order, preserving original order
        for (const Group &g : out)
            if (!placed.contains(g.id)) reordered.append(g);
        out = reordered;
    }

    return out;
}

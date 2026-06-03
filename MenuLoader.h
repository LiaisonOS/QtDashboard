//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : Loads dashboard-menu.json (system) + dashboard-menu.overrides.json (user)
//           and produces a merged, in-memory menu tree consumed by MainWindow.
//

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QTimer>

class ModeLoader;

class MenuLoader : public QObject
{
    Q_OBJECT

public:
    enum class ItemType { Mode, Multi, Param, Toggle };

    struct ModeRef {
        QString id;
        QString label;
        QString labelTouch;
        QString labelAbbr;     // 3-5 char strip-mode tag (optional)
        QString stripIcon;     // single unicode glyph/emoji (optional)
    };

    struct ParamOption {
        QString value;
        QString label;
        QString labelTouch;
        QString labelAbbr;
        QString stripIcon;
    };

    struct Item {
        ItemType         type;
        QString          id;          // Mode + Param + Toggle
        QString          label;
        QString          labelTouch;
        QString          labelAbbr;
        QString          stripIcon;
        QList<ModeRef>   modes;       // Multi
        QString          paramKey;    // Param
        QList<ParamOption> options;   // Param
        // Toggle: state is derived from a sentinel file on disk. Touch the
        // file to toggle OFF (when offWhenFileExists=true) or ON (otherwise).
        QString          stateFile;        // Toggle
        bool             offWhenFileExists = true;  // Toggle
    };

    struct Group {
        QString       id;
        QString       titleEn;
        QString       titleFr;
        QString       icon;
        QList<Item>   items;
    };

    explicit MenuLoader(ModeLoader *modeLoader, QObject *parent = nullptr);

    // Returns the merged menu. Cached; call reload() to refresh.
    const QList<Group> &menu() const { return m_merged; }

    // Re-read system file + overrides + re-merge. Emits menuChanged() if anything changed.
    void reload();

    // Helper to pick label for the current UI mode (touch falls back to label).
    static QString labelFor(const QString &label, const QString &labelTouch, bool touchMode);

    // Look up a mode's strip-display label in the merged menu.
    // Returns: label_touch (uppercased) if set;
    //          else first word of label (uppercased) if set;
    //          else the supplied fallback.
    // Searches both top-level "mode"/"param" items and nested "multi" entries.
    QString stripLabelFor(const QString &modeId, const QString &fallback) const;

    // Look up a mode's regular menu label (the one shown on the group button).
    // Used by Recent buttons so they show the SAME short label as the group
    // (e.g. "QtTermTCP") rather than the verbose modes.d name (e.g.
    // "BBS Client (QtTermTCP)"). Returns labelTouch if touchMode and set,
    // else label, else fallback.
    QString menuLabelFor(const QString &modeId, const QString &fallback,
                         bool touchMode) const;

    // Look up the LABEL of a specific modem/param option (e.g. "VARA HF")
    // given the param-mode id and the modem value (e.g. "vara-hf"). Returns
    // fallback if the menu doesn't define a matching option. Used by Recent
    // buttons so they show the operator-defined label, not the raw key.
    QString modemLabelFor(const QString &modeId, const QString &modemValue,
                          const QString &fallback) const;

    // If `modeId` is a child of a Multi item, return the Multi's wrapper
    // label (e.g. "Winlink" for the winlink-mercury / winlink-vara-hf set).
    // Returns empty string if the mode isn't a Multi child. Used to build
    // full Recent labels like "Winlink - Mercury".
    QString multiParentLabelFor(const QString &modeId, bool touchMode) const;

    // Look up a mode's strip icon (single unicode glyph / emoji). Returns
    // empty string if no icon is defined.
    QString stripIconFor(const QString &modeId) const;

signals:
    void menuChanged();

private slots:
    void onOverrideFileChanged(const QString &path);
    void onOverrideDirChanged(const QString &path);
    void doReload();

private:
    QList<Group> loadFromFile(const QString &path);
    QJsonObject  loadJsonObject(const QString &path);
    Group        parseGroup(const QJsonObject &o);
    Item         parseItem(const QJsonObject &o);
    ModeRef      parseModeRef(const QJsonObject &o);
    ParamOption  parseParamOption(const QJsonObject &o);

    QList<Group> applyOverrides(QList<Group> sys, const QJsonObject &ov);

    QString systemPath() const;
    QString overridesPath() const;

    void ensureOverridesTemplate();   // create empty + documented file if missing
    void rewatchOverrides();          // (re-)add override path to the watcher

    ModeLoader          *m_modeLoader;
    QList<Group>         m_merged;
    QFileSystemWatcher  *m_watcher       = nullptr;
    QTimer              *m_reloadDebounce = nullptr;
};

#include "cellculture.h"
#include <QStringList>
#include <QSet>

// Root base name (first-parent chain up to root)
static QString rootBaseName(const CellCultureStore* store, const QUuid& id)
{
    const CellCulture* cur = store->byId(id);
    while (cur && !cur->parents.isEmpty()) {
        const CellCulture* next = store->byId(cur->parents.first());
        if (!next) break;
        cur = next;
    }
    return cur ? cur->name : QStringLiteral("Unknown");
}

CellCultureStore::CellCultureStore(QObject *parent)
    : QObject(parent)
{}

QUuid CellCultureStore::addRoot(const QString &name,
                                const QString &textDesc,
                                const QString &numKey,
                                double numVal)
{
    CellCulture c;
    c.id = QUuid::createUuid();
    c.passage = 0;
    c.name = name.isEmpty() ? QStringLiteral("Root") : name;

    Operation op{ textDesc, numKey, numVal };
    c.ops.push_back(op);

    m_items.push_back(c);
    emit changed();
    return c.id;
}

QUuid CellCultureStore::addDerived(const QUuid &parentId,
                                   const QString &name,
                                   const QString &textDesc,
                                   const QString &numKey,
                                   double numVal)
{
    const CellCulture *p = byId(parentId);

    CellCulture c;
    c.id = QUuid::createUuid();
    c.parents = { parentId };

    const int parentPassage = p ? p->passage : 0;
    c.passage = parentPassage + 1;

    if (name.isEmpty()) {
        const QString base = p ? rootBaseName(this, parentId) : QStringLiteral("Unknown");
        c.name = QString("%1 %2").arg(base).arg(c.passage);   // “Root A 1”, “Root A 2”, ...
    } else {
        c.name = name;
    }

    Operation op{ textDesc, numKey, numVal };
    c.ops.push_back(op);

    m_items.push_back(c);
    emit changed();
    return c.id;
}

int CellCultureStore::indexOf(const QUuid &id) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id) return i;
    return -1;
}

const CellCulture *CellCultureStore::byId(const QUuid &id) const
{
    int i = indexOf(id);
    return (i >= 0) ? &m_items[i] : nullptr;
}

// DFS from target back to roots (assumes at most one parent, but still works)
static void collectPathsToRoots(const CellCultureStore *store,
                                const QUuid &currentId,
                                QVector<QUuid> &stack,
                                QVector<QVector<QUuid>> &allPaths)
{
    const CellCulture *cur = store->byId(currentId);
    stack.push_back(currentId);

    if (!cur || cur->parents.isEmpty()) {
        QVector<QUuid> path;
        for (int i = stack.size() - 1; i >= 0; --i)
            path.push_back(stack[i]);
        allPaths.push_back(path);
    } else {
        // only one parent expected now
        collectPathsToRoots(store, cur->parents.first(), stack, allPaths);
    }

    stack.pop_back();
}

QVector<QVector<QUuid>> CellCultureStore::lineagePaths(const QUuid &targetId) const
{
    QVector<QVector<QUuid>> paths;
    QVector<QUuid> stack;
    collectPathsToRoots(this, targetId, stack, paths);
    return paths;
}

CellCultureStore::Summary
CellCultureStore::summarizePath(const QVector<QUuid> &path, const QString &key) const
{
    Summary s;
    bool hasValue = false;

    for (const QUuid &id : path) {
        const CellCulture *c = byId(id);
        if (!c) continue;

        for (const auto &op : c->ops) {
            if (op.key.compare(key, Qt::CaseInsensitive) == 0) {
                s.sum += op.value;
                if (!hasValue) { s.min = s.max = op.value; hasValue = true; }
                else { if (op.value < s.min) s.min = op.value; if (op.value > s.max) s.max = op.value; }
                s.count += 1;
                break;
            }
        }
    }

    if (!hasValue) { s.min = 0.0; s.max = 0.0; }
    return s;
}

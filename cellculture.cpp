#include "cellculture.h"
#include <QStringList>
#include <QSet>

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

// Root: passage = 0; if no name, use "Root"
QUuid CellCultureStore::addRoot(const QString &name,
                                const QString &textDesc,
                                const QString &numKey,
                                double numVal)
{
    CellCulture c;
    c.id = QUuid::createUuid();
    c.passage = 0;
    c.name = name.isEmpty() ? QStringLiteral("Root") : name;

    Operation op;
    op.text = textDesc;
    op.key = numKey;
    op.value = numVal;
    c.ops.push_back(op);

    m_items.push_back(c);
    emit changed();
    return c.id;
}

// Derived: passage = parent + 1; if no name, "<rootBase> <passage>"
QUuid CellCultureStore::addDerived(const QUuid &parentId,
                                   const QString &name,
                                   const QString &textDesc,
                                   const QString &numKey,
                                   double numVal)
{
    const CellCulture *p = byId(parentId);

    CellCulture c;
    c.id = QUuid::createUuid();
    c.parents.push_back(parentId);

    int parentPassage = p ? p->passage : 0;
    c.passage = parentPassage + 1;

    if (name.isEmpty()) {
        const QString base = p ? rootBaseName(this, parentId) : QStringLiteral("Unknown");
        c.name = QString("%1 %2").arg(base).arg(c.passage);   // e.g. "Root A 1"
    } else {
        c.name = name;
    }

    Operation op;
    op.text = textDesc;
    op.key = numKey;
    op.value = numVal;
    c.ops.push_back(op);

    m_items.push_back(c);
    emit changed();
    return c.id;
}

// Mix: passage = max(parents) + 1; if no name, "<base1 + base2 + ...> <passage>"
QUuid CellCultureStore::addMix(const QVector<QUuid> &parentIds,
                               const QString &name,
                               const QString &textDesc,
                               const QString &numKey,
                               double numVal)
{
    CellCulture c;
    c.id = QUuid::createUuid();
    c.parents = parentIds;

    int maxPassage = -1;
    QSet<QString> baseSet;
    for (const QUuid &pid : parentIds) {
        const CellCulture *p = byId(pid);
        if (p) {
            if (p->passage > maxPassage) maxPassage = p->passage;
            baseSet.insert(rootBaseName(this, pid));
        }
    }
    c.passage = maxPassage + 1;

    if (name.isEmpty()) {
        QStringList bases = QStringList(baseSet.begin(), baseSet.end());
        bases.sort(Qt::CaseInsensitive);
        QString label = bases.join(QStringLiteral(" + "));
        if (label.isEmpty()) label = QStringLiteral("Unknown");
        c.name = QString("%1 %2").arg(label).arg(c.passage);  // e.g. "Root A + Root B 3"
    } else {
        c.name = name;
    }

    Operation op;
    op.text = textDesc;
    op.key = numKey;
    op.value = numVal;
    c.ops.push_back(op);

    m_items.push_back(c);
    emit changed();
    return c.id;
}

int CellCultureStore::indexOf(const QUuid &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id)
            return i;
    }
    return -1;
}

const CellCulture *CellCultureStore::byId(const QUuid &id) const
{
    int i = indexOf(id);
    return (i >= 0) ? &m_items[i] : nullptr;
}

// lineage (root -> ... -> target), simple DFS (assumes DAG)
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
        for (int i = 0; i < cur->parents.size(); ++i)
            collectPathsToRoots(store, cur->parents[i], stack, allPaths);
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

    for (int i = 0; i < path.size(); ++i) {
        const CellCulture *c = byId(path[i]);
        if (!c) continue;

        for (int j = 0; j < c->ops.size(); ++j) {
            const Operation &op = c->ops[j];
            if (op.key.compare(key, Qt::CaseInsensitive) == 0) {
                s.sum += op.value;
                if (!hasValue) { s.min = s.max = op.value; hasValue = true; }
                else { if (op.value < s.min) s.min = op.value; if (op.value > s.max) s.max = op.value; }
                s.count += 1;
            }
        }
    }

    if (!hasValue) { s.min = 0.0; s.max = 0.0; }
    return s;
}

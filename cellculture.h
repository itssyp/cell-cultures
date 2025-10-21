#ifndef CELLCULTURE_H
#define CELLCULTURE_H

#include <QObject>
#include <QUuid>
#include <QVector>
#include <QString>

struct Operation {
    QString text;
    QString key;
    double  value;
};

struct CellCulture {
    QUuid id;
    QString name;
    int passage{0};
    QVector<QUuid> parents;   // empty for root; size==1 for derived
    QVector<Operation> ops;
};

class CellCultureStore : public QObject {
    Q_OBJECT

private:
    QVector<CellCulture> m_items;

public:
    explicit CellCultureStore(QObject *parent = nullptr);

    QUuid addRoot(const QString &name,
                  const QString &textDesc,
                  const QString &numKey,
                  double numVal);

    QUuid addDerived(const QUuid &parentId,
                     const QString &name,
                     const QString &textDesc,
                     const QString &numKey,
                     double numVal);

    const QVector<CellCulture> &all() const { return m_items; }
    int indexOf(const QUuid &id) const;
    const CellCulture *byId(const QUuid &id) const;

    QVector<QVector<QUuid>> lineagePaths(const QUuid &targetId) const;

    struct Summary {
        double sum{0.0};
        double min{0.0};
        double max{0.0};
        int    count{0};
    };
    Summary summarizePath(const QVector<QUuid> &path, const QString &key) const;

signals:
    void changed();
};

#endif // CELLCULTURE_H

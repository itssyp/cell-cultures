#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QAbstractItemView>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

static QString cultureLabel(const CellCulture &c) {
    return c.name; // passage included by auto-naming when name empty
}

static QVector<double> valuesForKeyAlongPath(const CellCultureStore& store,
                                             const QVector<QUuid>& path,
                                             const QString& key)
{
    QVector<double> vals;
    for (const QUuid& id : path) {
        const CellCulture* c = store.byId(id);
        if (!c) continue;
        for (const auto& op : c->ops) {
            if (op.key.compare(key, Qt::CaseInsensitive) == 0) {
                vals.push_back(op.value);
                break; // take first match per culture
            }
        }
    }
    return vals;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // list model + multi-select (for MIX)
    ui->listView->setModel(&m_list);
    ui->listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // seed a root
    m_store.addRoot("Root A", "initial stock", "temperature", 37.0);

    // wire buttons
    connect(ui->pushButtonAdd, &QPushButton::clicked, this, &MainWindow::addCulture);
    connect(ui->pushButtonShowLineage, &QPushButton::clicked, this, &MainWindow::showLineage);
    connect(ui->pushButtonShowPlot, &QPushButton::clicked, this, &MainWindow::showNumericPlot);

    // refresh when data changes
    connect(&m_store, &CellCultureStore::changed, this, [this]{ refreshList(); });

    refreshList();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::refreshList() {
    QStringList rows;
    rows.reserve(m_store.all().size());
    for (const auto &c : m_store.all())
        rows << cultureLabel(c);
    m_list.setStringList(rows);
}

void MainWindow::addCulture() {
    // Name optional; text+key required
    if (ui->lineEditText->text().isEmpty() || ui->lineEditKey->text().isEmpty()) {
        QMessageBox::warning(this, "Missing fields", "Fill Text and Numeric Key.");
        return;
    }

    const auto indexes = ui->listView->selectionModel()->selectedIndexes();
    const int n = indexes.size();

    if (n == 0) {
        // ROOT
        m_store.addRoot(ui->lineEditName->text(),
                        ui->lineEditText->text(),
                        ui->lineEditKey->text(),
                        ui->doubleSpinBoxValue->value());
        return;
    }

    if (n == 1) {
        // DERIVED
        const int row = indexes.first().row();
        if (row < 0 || row >= m_store.all().size()) {
            QMessageBox::warning(this, "Invalid selection", "Please select a valid parent.");
            return;
        }
        const QUuid parentId = m_store.all().at(row).id;

        m_store.addDerived(parentId,
                           ui->lineEditName->text(),
                           ui->lineEditText->text(),
                           ui->lineEditKey->text(),
                           ui->doubleSpinBoxValue->value());
        return;
    }

    // MIX (2+)
    QVector<QUuid> parents; parents.reserve(n);
    for (const auto &idx : indexes) {
        const int row = idx.row();
        if (row >= 0 && row < m_store.all().size())
            parents.push_back(m_store.all().at(row).id);
    }
    if (parents.size() < 2) {
        QMessageBox::warning(this, "Invalid selection", "Select at least two valid parents.");
        return;
    }

    m_store.addMix(parents,
                   ui->lineEditName->text(),
                   ui->lineEditText->text(),
                   ui->lineEditKey->text(),
                   ui->doubleSpinBoxValue->value());
}

void MainWindow::showLineage() {
    const QModelIndex idx = ui->listView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "No selection", "Select a culture in the list.");
        return;
    }
    const int row = idx.row();
    if (row < 0 || row >= m_store.all().size()) return;

    const QUuid targetId = m_store.all().at(row).id;
    const QString key = ui->lineEditKey->text().isEmpty()
                            ? QStringLiteral("temperature")
                            : ui->lineEditKey->text();

    const auto paths = m_store.lineagePaths(targetId);

    QString out;
    out += QString("Found %1 path(s).\n\n").arg(paths.size());
    int pNo = 1;

    for (const auto &path : paths) {
        out += QString("Path %1:\n").arg(pNo++);
        for (int i = 0; i < path.size(); ++i) {
            const CellCulture *c = m_store.byId(path[i]);
            if (!c) continue;
            out += QString("  %1. %2\n").arg(i + 1).arg(c->name);
            for (int j = 0; j < c->ops.size(); ++j) {
                const Operation &op = c->ops[j];
                out += QString("      - %1 | %2 = %3\n")
                           .arg(op.text, op.key)
                           .arg(op.value);
            }
        }
        const auto s = m_store.summarizePath(path, key);
        out += QString("  Summary '%1': count=%2, sum=%3, min=%4, max=%5\n\n")
                   .arg(key).arg(s.count).arg(s.sum).arg(s.min).arg(s.max);
    }

    ui->textEditInfo->setPlainText(out);
}

void MainWindow::showNumericPlot()
{
    // selected culture
    const QModelIndex idx = ui->listView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "No selection", "Select a culture to plot.");
        return;
    }
    const int row = idx.row();
    if (row < 0 || row >= m_store.all().size()) return;
    const QUuid targetId = m_store.all().at(row).id;

    // numeric key (default temperature)
    const QString key = ui->lineEditKey->text().isEmpty()
                            ? QStringLiteral("temperature")
                            : ui->lineEditKey->text();

    // choose the longest root->target path (simple heuristic)
    const auto paths = m_store.lineagePaths(targetId);
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Nothing to plot", "No lineage found.");
        return;
    }
    QVector<QUuid> best = paths.first();
    for (const auto& p : paths) if (p.size() > best.size()) best = p;

    // collect values
    const auto vals = valuesForKeyAlongPath(m_store, best, key);
    if (vals.isEmpty()) {
        QMessageBox::information(this, "No data",
                                 QString("No '%1' values found along the path.").arg(key));
        return;
    }

    // build line series
    auto *series = new QLineSeries();
    for (int i = 0; i < vals.size(); ++i)
        series->append(i, vals[i]);

    // chart
    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QString("'%1' along production steps").arg(key));
    chart->legend()->hide();

    // axes
    auto *axisX = new QValueAxis();
    axisX->setTitleText("Step");
    axisX->setLabelFormat("%d");
    axisX->setTickCount(qMin(vals.size() + 1, 11));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    axisY->setTitleText(key);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // show in a simple dialog
    auto *view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);

    QDialog dlg(this);
    dlg.setWindowTitle("Numeric Plot");
    QVBoxLayout lay(&dlg);
    lay.addWidget(view);
    dlg.resize(640, 400);
    dlg.exec();
}

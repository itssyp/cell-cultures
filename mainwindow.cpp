#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QAbstractItemView>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
QT_CHARTS_USE_NAMESPACE
#endif

    static QString cultureLabel(const CellCulture &c) {
    return c.name;
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
                break;
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

    ui->listView->setModel(&m_list);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection); // <— single only

    m_store.addRoot("Root A", "initial stock", "temperature", 37.0);

    connect(ui->pushButtonAdd, &QPushButton::clicked, this, &MainWindow::addCulture);
    connect(ui->pushButtonShowLineage, &QPushButton::clicked, this, &MainWindow::showLineage);
    connect(ui->pushButtonShowPlot, &QPushButton::clicked, this, &MainWindow::showNumericPlot);

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
    if (ui->lineEditText->text().isEmpty() || ui->lineEditKey->text().isEmpty()) {
        QMessageBox::warning(this, "Missing fields", "Fill Text and Numeric Key.");
        return;
    }

    const QModelIndex idx = ui->listView->currentIndex();
    if (!idx.isValid()) {
        // ROOT
        m_store.addRoot(ui->lineEditName->text(),
                        ui->lineEditText->text(),
                        ui->lineEditKey->text(),
                        ui->doubleSpinBoxValue->value());
        return;
    }

    // DERIVED
    const int row = idx.row();
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
            for (const auto &op : c->ops)
                out += QString("      - %1 | %2 = %3\n")
                           .arg(op.text, op.key)
                           .arg(op.value);
        }
        const auto s = m_store.summarizePath(path, key);
        out += QString("  Summary '%1': count=%2, sum=%3, min=%4, max=%5\n\n")
                   .arg(key).arg(s.count).arg(s.sum).arg(s.min).arg(s.max);
    }

    ui->textEditInfo->setPlainText(out);
}

void MainWindow::showNumericPlot()
{
    const QModelIndex idx = ui->listView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "No selection", "Select a culture to plot.");
        return;
    }
    const int row = idx.row();
    if (row < 0 || row >= m_store.all().size()) return;
    const QUuid targetId = m_store.all().at(row).id;

    const QString key = ui->lineEditKey->text().isEmpty()
                            ? QStringLiteral("temperature")
                            : ui->lineEditKey->text();

    const auto paths = m_store.lineagePaths(targetId);
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Nothing to plot", "No lineage found.");
        return;
    }
    const QVector<QUuid>& path = paths.first(); // there will be only one in non-mix

    const auto vals = valuesForKeyAlongPath(m_store, path, key);
    if (vals.isEmpty()) {
        QMessageBox::information(this, "No data",
                                 QString("No '%1' values found along the path.").arg(key));
        return;
    }

    auto *series = new QLineSeries();
    for (int i = 0; i < vals.size(); ++i)
        series->append(i, vals[i]);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QString("'%1' along production steps").arg(key));
    chart->legend()->hide();

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

    auto *view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);

    QDialog dlg(this);
    dlg.setWindowTitle("Numeric Plot");
    QVBoxLayout lay(&dlg);
    lay.addWidget(view);
    dlg.resize(640, 400);
    dlg.exec();
}

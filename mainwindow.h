#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringListModel>
#include "cellculture.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    Ui::MainWindow *ui;
    CellCultureStore m_store;
    QStringListModel m_list;

    void refreshList();

private slots:
    void addCulture();     // Root / Derived / Mix by selection
    void showLineage();    // Text summary
    void showNumericPlot(); // Simple plot using Qt Charts

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
};

#endif // MAINWINDOW_H

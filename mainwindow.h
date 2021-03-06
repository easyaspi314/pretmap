#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QString>
#include <QModelIndex>
#include <QMainWindow>
#include <QGraphicsPixmapItem>
#include <QGraphicsItemGroup>
#include <QGraphicsSceneMouseEvent>
#include <QAbstractItemModel>
#include "project.h"
#include "map.h"
#include "editor.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_action_Open_Project_triggered();
    void on_mapList_activated(const QModelIndex &index);
    void on_action_Save_Project_triggered();

    void undo();
    void redo();

    void onMapChanged(Map *map);

    void on_action_Save_triggered();
    void on_tabWidget_2_currentChanged(int index);
    void on_action_Exit_triggered();
    void on_comboBox_Song_activated(const QString &arg1);
    void on_comboBox_Location_activated(const QString &arg1);
    void on_comboBox_Visibility_activated(const QString &arg1);
    void on_comboBox_Weather_activated(const QString &arg1);
    void on_comboBox_Type_activated(const QString &arg1);
    void on_comboBox_BattleScene_activated(const QString &arg1);
    void on_checkBox_ShowLocation_clicked(bool checked);

    void on_tabWidget_currentChanged(int index);

    void on_actionUndo_triggered();

    void on_actionRedo_triggered();

    void on_toolButton_newObject_clicked();

    void on_toolButton_deleteObject_clicked();

    void updateSelectedObjects();

    void on_toolButton_Paint_clicked();

    void on_toolButton_Select_clicked();

    void on_toolButton_Fill_clicked();

    void on_toolButton_Dropper_clicked();

private:
    Ui::MainWindow *ui;
    Editor *editor = NULL;
    void setMap(QString);
    void populateMapList();
    QString getExistingDirectory(QString);
    void openProject(QString dir);
    QString getDefaultMap();
    void setRecentMap(QString map_name);

    void markAllEdited(QAbstractItemModel *model);
    void markEdited(QModelIndex index);
    void updateMapList();

    void displayMapProperties();
    void checkToolButtons();
};

#endif // MAINWINDOW_H

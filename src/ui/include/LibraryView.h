#pragma once

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include <QString>
#include <QVector>
#include "RomInfo.h"

class GameGridDelegate;

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr);
    ~LibraryView() override;

    void setRoms(const QVector<RomInfo> &roms);

signals:
    void romSelected(const QString &romId, const QString &romPath);
    void launchRequested(const QString &romId, const QString &romPath, const QString &emulatorId);

private slots:
    void onItemClicked(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);

private:
    void setupUI();

    QListView          *m_view      = nullptr;
    QStandardItemModel *m_model     = nullptr;
    GameGridDelegate   *m_delegate  = nullptr; // Qt-parented; destroyed with m_view
};

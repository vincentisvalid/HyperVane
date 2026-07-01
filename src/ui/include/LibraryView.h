#pragma once

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include <QString>
#include <QVector>
#include "RomInfo.h"

class GameGridDelegate;
class UISoundManager;

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr, UISoundManager *soundMgr = nullptr);
    ~LibraryView() override;

    void setRoms(const QVector<RomInfo> &roms);
    QListView *view() const { return m_view; }

signals:
    void romSelected(const QString &romId, const QString &romPath);
    void playRequested(const QString &romId);
    void launchRequested(const QString &romId, const QString &romPath, const QString &emulatorId);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onItemClicked(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);

private:
    void setupUI();

    QListView          *m_view      = nullptr;
    QStandardItemModel *m_model     = nullptr;
    GameGridDelegate   *m_delegate  = nullptr;
    UISoundManager     *m_soundMgr  = nullptr;
    QString             m_lastHoverId;
};

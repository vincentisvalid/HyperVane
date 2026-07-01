#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QString>

class InGameMenu : public QWidget
{
    Q_OBJECT

public:
    explicit InGameMenu(const QString &gameTitle, QWidget *parent = nullptr);

    void setGameTitle(const QString &title);

signals:
    void resumeGame();
    void saveAndClose();
    void closeGame();
    void goHome();
    void exitHyperVane();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QPushButton *createButton(const QString &text);

    QLabel *m_gameLabel   = nullptr;
    QLabel *m_hintLabel   = nullptr;
};

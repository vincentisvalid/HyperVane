#pragma once

#include <QVector>
#include <QWidget>

class VirtualKeyboard : public QWidget
{
    Q_OBJECT
public:
    explicit VirtualKeyboard(QWidget *parent = nullptr);
    void attach(QWidget *target);

signals:
    void keyOutput(const QString &text);
    void backspace();
    void confirmed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPoint keyAt(int row, int col) const;
    void   emitCurrent();

    static const QVector<QVector<QString>> kRows;
    int m_col = 0;
    int m_row = 0;

    static constexpr int kKeyW   = 44;
    static constexpr int kKeyH   = 44;
    static constexpr int kGap    = 4;
    static constexpr int kRadius = 8;
    static constexpr int kCols   = 15;
};

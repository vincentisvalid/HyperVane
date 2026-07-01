#include "VirtualKeyboard.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTextEdit>
#include <QVBoxLayout>

const QVector<QVector<QString>> VirtualKeyboard::kRows = {
    {"1","2","3","4","5","6","7","8","9","0","-","="},
    {"q","w","e","r","t","y","u","i","o","p","[","]","\\"},
    {"a","s","d","f","g","h","j","k","l",";","'"},
    {"z","x","c","v","b","n","m",",",".","/"},
    {" ", "<-", "OK"},
};

VirtualKeyboard::VirtualKeyboard(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(kCols * (kKeyW + kGap) + kGap,
                 kRows.size() * (kKeyH + kGap) + kGap);
    setFocusPolicy(Qt::StrongFocus);
}

void VirtualKeyboard::attach(QWidget *target)
{
    if (target) {
        connect(this, &VirtualKeyboard::keyOutput, target, [target](const QString &text) {
            if (auto *edit = qobject_cast<QLineEdit*>(target))
                edit->insert(text);
            else if (auto *browser = qobject_cast<QTextEdit*>(target))
                browser->insertPlainText(text);
        });
        connect(this, &VirtualKeyboard::backspace, target, [target]() {
            if (auto *edit = qobject_cast<QLineEdit*>(target))
                edit->backspace();
            else if (auto *browser = qobject_cast<QTextEdit*>(target))
                browser->textCursor().deletePreviousChar();
        });
        connect(this, &VirtualKeyboard::confirmed, target, [target]() {
            if (auto *edit = qobject_cast<QLineEdit*>(target)) {
                emit edit->returnPressed();
                edit->clearFocus();
            }
        });
    }
}

QPoint VirtualKeyboard::keyAt(int row, int col) const
{
    return QPoint(kGap + col * (kKeyW + kGap),
                  kGap + row * (kKeyH + kGap));
}

void VirtualKeyboard::emitCurrent()
{
    if (m_row < 0 || m_row >= kRows.size()) return;
    const auto &row = kRows[m_row];
    if (m_col < 0 || m_col >= row.size()) return;
    const QString &key = row[m_col];

    if (key == "<-")
        emit backspace();
    else if (key == "OK")
        emit confirmed();
    else if (key == " ")
        emit keyOutput(" ");
    else
        emit keyOutput(key);
}

void VirtualKeyboard::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        m_col = qMax(0, m_col - 1);
        break;
    case Qt::Key_Right:
        m_col = qMin(kRows[m_row].size() - 1, m_col + 1);
        break;
    case Qt::Key_Up:
        m_row = qMax(0, m_row - 1);
        m_col = qMin(m_col, kRows[m_row].size() - 1);
        break;
    case Qt::Key_Down:
        m_row = qMin(kRows.size() - 1, m_row + 1);
        m_col = qMin(m_col, kRows[m_row].size() - 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        emitCurrent();
        break;
    case Qt::Key_Backspace:
        emit backspace();
        break;
    case Qt::Key_Escape:
        emit confirmed();
        break;
    default:
        if (!event->text().isEmpty() && event->text().at(0).isPrint())
            emit keyOutput(event->text());
        break;
    }
    update();
}

void VirtualKeyboard::mousePressEvent(QMouseEvent *event)
{
    const int x = static_cast<int>(event->position().x());
    const int y = static_cast<int>(event->position().y());

    for (int r = 0; r < kRows.size(); ++r) {
        const auto &row = kRows[r];
        for (int c = 0; c < row.size(); ++c) {
            QPoint origin = keyAt(r, c);
            QRect keyRect(origin.x(), origin.y(), kKeyW, kKeyH);
            if (keyRect.contains(x, y)) {
                m_row = r;
                m_col = c;
                emitCurrent();
                update();
                return;
            }
        }
    }
}

void VirtualKeyboard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0x1a, 0x1a, 0x1a));

    for (int r = 0; r < kRows.size(); ++r) {
        const auto &row = kRows[r];
        for (int c = 0; c < row.size(); ++c) {
            const QPoint origin = keyAt(r, c);
            QRectF keyRect(origin.x(), origin.y(), kKeyW, kKeyH);

            bool hovered = (r == m_row && c == m_col);
            QColor bg = hovered ? QColor(0x00, 0x66, 0xcc)
                                : QColor(0x33, 0x33, 0x33);

            QPainterPath path;
            path.addRoundedRect(keyRect, kRadius, kRadius);
            p.fillPath(path, bg);

            p.setPen(Qt::white);
            QFont f = p.font();
            f.setPointSize(11);
            f.setBold(hovered);
            p.setFont(f);
            p.drawText(keyRect, Qt::AlignCenter, row[c]);
        }
    }
}

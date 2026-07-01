#include "SocoliteCanvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>

SocoliteCanvas::SocoliteCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(false);
    m_canvas = QImage(640, 480, QImage::Format_ARGB32);
    m_canvas.fill(QColor("#f0f0f0"));
}

void SocoliteCanvas::setPenColor(const QColor &c) { m_penColor = c; }
void SocoliteCanvas::setPenWidth(int w) { m_penWidth = qBound(1, w, 50); }
void SocoliteCanvas::setEraser(bool erasing) { m_erasing = erasing; }

void SocoliteCanvas::clearCanvas()
{
    m_canvas.fill(QColor("#f0f0f0"));
    update();
}

QImage SocoliteCanvas::canvasImage() const { return m_canvas; }

void SocoliteCanvas::loadImage(const QImage &img)
{
    m_canvas = img.scaled(m_canvas.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    update();
}

static QPoint clampToRect(const QPoint &pt, const QRect &rect)
{
    return QPoint(std::clamp(pt.x(), rect.left(), rect.right()),
                  std::clamp(pt.y(), rect.top(), rect.bottom()));
}

void SocoliteCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();
    QImage scaled = m_canvas.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (r.width() - scaled.width()) / 2;
    int y = (r.height() - scaled.height()) / 2;
    p.drawImage(x, y, scaled);
}

void SocoliteCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        QRect r = rect();
        QSize imgSize = m_canvas.size();
        double sx = static_cast<double>(imgSize.width()) / r.width();
        double sy = static_cast<double>(imgSize.height()) / r.height();
        m_lastPoint = clampToRect(
            QPoint(static_cast<int>(event->pos().x() * sx),
                   static_cast<int>(event->pos().y() * sy)),
            m_canvas.rect());
    }
}

void SocoliteCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing) return;
    QRect r = rect();
    QSize imgSize = m_canvas.size();
    double sx = static_cast<double>(imgSize.width()) / r.width();
    double sy = static_cast<double>(imgSize.height()) / r.height();
    QPoint pt = clampToRect(
        QPoint(static_cast<int>(event->pos().x() * sx),
               static_cast<int>(event->pos().y() * sy)),
        m_canvas.rect());
    drawLineTo(pt);
}

void SocoliteCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        emit imageCreated(m_canvas);
    }
}

void SocoliteCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void SocoliteCanvas::drawLineTo(const QPoint &end)
{
    QPainter painter(&m_canvas);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_erasing) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.setPen(QPen(Qt::transparent, m_penWidth * 3,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    } else {
        painter.setPen(QPen(m_penColor, m_penWidth,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }

    painter.drawLine(m_lastPoint, end);
    m_lastPoint = end;
    update();
}

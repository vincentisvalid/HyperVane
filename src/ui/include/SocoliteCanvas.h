#pragma once

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QVector>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

class SocoliteCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit SocoliteCanvas(QWidget *parent = nullptr);

    void setPenColor(const QColor &c);
    void setPenWidth(int w);
    void setEraser(bool erasing);
    void clearCanvas();
    QImage canvasImage() const;
    void loadImage(const QImage &img);

signals:
    void imageCreated(const QImage &img);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawLineTo(const QPoint &end);

    QImage m_canvas;
    QColor m_penColor = QColor(0x00, 0x66, 0xcc);
    int m_penWidth = 3;
    bool m_erasing = false;
    bool m_drawing = false;
    QPoint m_lastPoint;

    // Color presets
    QVector<QColor> m_presets = {
        QColor("#000000"), QColor("#ffffff"), QColor("#e50914"),
        QColor("#0066cc"), QColor("#00cc66"), QColor("#e5a00d"),
        QColor("#ff6b35"), QColor("#9b59b6"), QColor("#1abc9c"),
        QColor("#e84393"), QColor("#2d3436"), QColor("#dfe6e9"),
    };
};

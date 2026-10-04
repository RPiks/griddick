#ifndef WATERFALL_H
#define WATERFALL_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * Waterfall.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

class QPainter;

class Waterfall : public QWidget {
    Q_OBJECT

public:
    explicit Waterfall(QWidget *parent = nullptr);
    void addLine(const float *db, int n);
    void clear();
    void setBinsPerPixel(double bpp);
    void setStartHz(int hz);
    void setTimeMarks(bool on);
    double binsPerPixel() const { return m_bpp; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Mark {
        int row;
        QString text;
    };

    QRgb colorOf(float db) const;
    void ensureSize();
    int startBin() const;
    int visBins() const;
    int plotWidth() const;
    void drawScale(QPainter &p, int plotW) const;
    void noteTime();

    QImage m_img;
    int m_bins;
    double m_bpp;
    int m_startHz;
    int m_cursorX;
    bool m_timeOn;
    int m_lastSlot;
    QVector<Mark> m_marks;
};

#endif

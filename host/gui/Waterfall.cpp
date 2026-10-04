/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * Waterfall.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "Waterfall.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QTime>
#include <algorithm>
#include <cstring>

static const QColor kPaper(0, 0, 80);
static const int kScaleH = 28;
static const float kFs = 48000.0f;
static const float kNfft = 16384.0f;
static const float kHzPerBin = kFs / kNfft;

Waterfall::Waterfall(QWidget *parent)
    : QWidget(parent)
    , m_bins(1366)
    , m_bpp(0.5)
    , m_startHz(200)
    , m_cursorX(-1)
    , m_timeOn(true)
    , m_lastSlot(-1)
{
    setMinimumHeight(180);
    setMinimumWidth(320);
    setMouseTracking(true);
    ensureSize();
}

int Waterfall::startBin() const
{
    int b = static_cast<int>(static_cast<float>(m_startHz) / kHzPerBin + 0.5f);
    if (b < 0) {
        b = 0;
    }
    if (b >= m_bins) {
        b = m_bins - 1;
    }
    return b;
}

int Waterfall::visBins() const
{
    return std::max(1, m_bins - startBin());
}

int Waterfall::plotWidth() const
{
    const double bpp = (m_bpp < 0.25) ? 0.25 : m_bpp;
    return std::max(1, static_cast<int>(static_cast<double>(visBins()) / bpp + 0.5));
}

void Waterfall::setBinsPerPixel(double bpp)
{
    if (bpp < 0.5) {
        bpp = 0.5;
    }
    if (bpp > 6.0) {
        bpp = 6.0;
    }
    if (bpp == m_bpp) {
        return;
    }
    m_bpp = bpp;
    update();
}

void Waterfall::setStartHz(int hz)
{
    if (hz < 0) {
        hz = 0;
    }
    if (hz > 3500) {
        hz = 3500;
    }
    if (hz == m_startHz) {
        return;
    }
    m_startHz = hz;
    update();
}

void Waterfall::setTimeMarks(bool on)
{
    m_timeOn = on;
    if (!on) {
        m_marks.clear();
    }
    update();
}

void Waterfall::ensureSize()
{
    const int h = std::max(80, height() > kScaleH ? height() - kScaleH : 180);
    const int w = std::max(1, m_bins);

    if (m_img.width() == w && m_img.height() == h) {
        return;
    }
    QImage neu(w, h, QImage::Format_RGB32);
    neu.fill(kPaper);
    if (!m_img.isNull()) {
        const int rows = std::min(h, m_img.height());
        const int bytes = std::min(w, m_img.width()) * static_cast<int>(sizeof(QRgb));
        for (int y = 0; y < rows; y++) {
            std::memcpy(neu.scanLine(y), m_img.constScanLine(y),
                        static_cast<size_t>(bytes));
        }
    }
    m_img = neu;
}

void Waterfall::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    ensureSize();
    update();
}

void Waterfall::clear()
{
    if (!m_img.isNull()) {
        m_img.fill(kPaper);
    }
    m_marks.clear();
    m_lastSlot = -1;
    update();
}

QRgb Waterfall::colorOf(float db) const
{
    static const float pos[] = {0.00f, 0.18f, 0.36f, 0.52f, 0.72f, 0.88f, 1.00f};
    static const int rr[] = {0, 0, 20, 14, 6, 249, 250};
    static const int gg[] = {0, 0, 80, 150, 229, 253, 0};
    static const int bb[] = {5, 116, 220, 247, 163, 15, 7};
    float t = (db + 72.0f) / 72.0f;
    int i;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    for (i = 0; i < 6; i++) {
        if (t <= pos[i + 1]) {
            const float u = (t - pos[i]) / (pos[i + 1] - pos[i]);
            return qRgb(rr[i] + static_cast<int>((rr[i + 1] - rr[i]) * u),
                        gg[i] + static_cast<int>((gg[i + 1] - gg[i]) * u),
                        bb[i] + static_cast<int>((bb[i + 1] - bb[i]) * u));
        }
    }
    return qRgb(250, 0, 7);
}

void Waterfall::noteTime()
{
    const QTime t = QTime::currentTime();
    const int slot = (t.hour() * 3600 + t.minute() * 60 + t.second()) / 15;

    if (m_lastSlot < 0) {
        m_lastSlot = slot;
        return;
    }
    if (slot == m_lastSlot) {
        return;
    }
    m_lastSlot = slot;
    const int s = (slot * 15) % 60;
    const int mtot = (slot * 15) / 60;
    const int hh = (mtot / 60) % 24;
    const int mm = mtot % 60;
    Mark mk;
    mk.row = 0;
    mk.text = QStringLiteral("%1:%2:%3")
                  .arg(hh, 2, 10, QLatin1Char('0'))
                  .arg(mm, 2, 10, QLatin1Char('0'))
                  .arg(s, 2, 10, QLatin1Char('0'));
    m_marks.prepend(mk);
}

void Waterfall::addLine(const float *db, int n)
{
    if (n <= 0) {
        return;
    }
    if (n != m_bins) {
        m_bins = n;
    }
    ensureSize();
    if (m_img.height() < 2) {
        return;
    }
    const int bpl = m_img.bytesPerLine();
    const int rows = m_img.height() - 1;
    std::memmove(m_img.scanLine(1), m_img.constScanLine(0),
                 static_cast<size_t>(bpl * rows));
    QRgb *line = reinterpret_cast<QRgb *>(m_img.scanLine(0));
    const int cols = std::min(m_bins, m_img.width());
    for (int x = 0; x < cols; x++) {
        line[x] = colorOf(db[x]);
    }
    for (int i = 0; i < m_marks.size(); i++) {
        m_marks[i].row++;
    }
    while (!m_marks.isEmpty() && m_marks.last().row >= m_img.height()) {
        m_marks.removeLast();
    }
    if (m_timeOn) {
        noteTime();
    }
    update();
}

void Waterfall::drawScale(QPainter &p, int plotW) const
{
    const int vis = std::min(width(), plotW);
    const float hzPx = kHzPerBin * static_cast<float>(m_bpp);
    const bool fine = (m_bpp <= 1.0);
    const int minorHz = fine ? 50 : 100;
    const int majorHz = fine ? 200 : 500;
    const int base = kScaleH - 1;
    const int start = m_startHz;

    p.fillRect(0, 0, width(), kScaleH, QColor(32, 32, 40));
    p.setPen(QColor(200, 200, 210));
    p.drawLine(0, base, vis, base);

    const int fMax = start + static_cast<int>(static_cast<float>(vis) * hzPx + 0.5f);
    int f0 = ((start + minorHz - 1) / minorHz) * minorHz;
    int f;
    for (f = f0; f <= fMax; f += minorHz) {
        const int x = static_cast<int>(static_cast<float>(f - start) / hzPx + 0.5f);
        if (x < 0 || x >= vis) {
            continue;
        }
        const bool major = (f % majorHz) == 0;
        p.drawLine(x, major ? base - 10 : base - 5, x, base);
        if (major) {
            p.drawText(x + 2, 12, QString::number(f));
        }
    }
}

void Waterfall::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    if (m_img.isNull()) {
        return;
    }
    const int plotW = plotWidth();
    const int vis = std::min(width(), plotW);
    const int bodyH = std::max(1, height() - kScaleH);
    const int sbin = startBin();

    drawScale(p, plotW);

    if (vis > 0 && m_img.height() > 0) {
        QImage view(vis, m_img.height(), QImage::Format_RGB32);
        const double bpp = (m_bpp < 0.25) ? 0.25 : m_bpp;
        for (int y = 0; y < m_img.height(); y++) {
            const QRgb *src = reinterpret_cast<const QRgb *>(m_img.constScanLine(y));
            QRgb *dst = reinterpret_cast<QRgb *>(view.scanLine(y));
            for (int x = 0; x < vis; x++) {
                const double b0d = static_cast<double>(x) * bpp;
                const int b0 = sbin + static_cast<int>(b0d);
                const int span = std::max(1, static_cast<int>(bpp + 0.999));
                QRgb best = src[std::min(b0, m_img.width() - 1)];
                int bestL = qGray(best);
                if (bpp >= 1.0) {
                    for (int k = 1; k < span; k++) {
                        const int b = b0 + k;
                        if (b >= m_img.width()) {
                            break;
                        }
                        const int L = qGray(src[b]);
                        if (L > bestL) {
                            bestL = L;
                            best = src[b];
                        }
                    }
                }
                dst[x] = best;
            }
        }
        p.drawImage(QRect(0, kScaleH, vis, bodyH), view);

        if (m_timeOn && !m_marks.isEmpty()) {
            QPen dash(QColor(255, 255, 255));
            dash.setStyle(Qt::DashLine);
            p.setPen(dash);
            const float yScale = static_cast<float>(bodyH) /
                                 static_cast<float>(m_img.height());
            for (const Mark &mk : m_marks) {
                const int y = kScaleH + static_cast<int>(mk.row * yScale);
                if (y < kScaleH || y >= height()) {
                    continue;
                }
                p.drawLine(0, y, vis, y);
                p.setPen(QColor(255, 255, 255));
                p.drawText(4, y - 2, mk.text);
                p.setPen(dash);
            }
        }
    }

    if (m_cursorX >= 0 && m_cursorX < vis) {
        p.setPen(QColor(240, 240, 255));
        p.drawLine(m_cursorX, 0, m_cursorX, height());
    }
}

void Waterfall::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pt = event->position().toPoint();
    m_cursorX = pt.x();
    if (pt.y() >= kScaleH) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    update();
}

void Waterfall::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_cursorX = -1;
    unsetCursor();
    update();
}

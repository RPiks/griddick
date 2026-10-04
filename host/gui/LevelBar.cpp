/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * LevelBar.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "LevelBar.h"

#include <QPainter>

static const float kRecLo = -20.0f;
static const float kRecHi = -8.0f;
static const float kMinDb = -60.0f;
static const float kMaxDb = 0.0f;

LevelBar::LevelBar(QWidget *parent)
    : QWidget(parent)
    , m_rms(kMinDb)
    , m_peak(kMinDb)
    , m_hold(kMinDb)
    , m_clips(0)
{
    setFixedWidth(32);
    setMinimumHeight(160);
}

void LevelBar::setLevels(float rmsDb, float peakDb, int clips)
{
    m_rms = rmsDb;
    m_peak = peakDb;
    if (peakDb > m_hold) {
        m_hold = peakDb;
    } else {
        m_hold -= 0.8f;
        if (m_hold < peakDb) {
            m_hold = peakDb;
        }
    }
    m_clips = clips;
    update();
}

void LevelBar::resetHold()
{
    m_hold = m_peak;
    m_clips = 0;
    update();
}

static int yOf(const QRect &r, float db)
{
    if (db < kMinDb) {
        db = kMinDb;
    }
    if (db > kMaxDb) {
        db = kMaxDb;
    }
    const float t = (db - kMinDb) / (kMaxDb - kMinDb);
    return r.bottom() - static_cast<int>(t * static_cast<float>(r.height()));
}

void LevelBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRect r = rect();

    p.fillRect(r, QColor(30, 30, 30));

    const int yRms = yOf(r, m_rms);
    QColor bar(200, 180, 40);
    if (m_hold >= -1.0f || m_clips > 0) {
        bar = QColor(200, 40, 40);
    } else if (m_rms >= kRecLo && m_rms <= kRecHi) {
        bar = QColor(50, 180, 70);
    } else if (m_rms < -30.0f) {
        bar = QColor(180, 50, 50);
    }
    p.fillRect(QRect(r.left(), yRms, r.width(), r.bottom() - yRms + 1), bar);

    p.setPen(QPen(QColor(40, 200, 60), 2));
    const int yLo = yOf(r, kRecLo);
    p.drawLine(r.left(), yLo, r.right(), yLo);

    p.setPen(QPen(QColor(220, 40, 40), 2));
    const int yHi = yOf(r, kRecHi);
    p.drawLine(r.left(), yHi, r.right(), yHi);

    p.setPen(QPen(QColor(240, 240, 240), 2));
    const int yHold = yOf(r, m_hold);
    p.drawLine(r.left(), yHold, r.right(), yHold);
}

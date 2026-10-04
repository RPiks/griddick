#ifndef LEVELBAR_H
#define LEVELBAR_H
/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * LevelBar.h - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include <QWidget>

class LevelBar : public QWidget {
    Q_OBJECT

public:
    explicit LevelBar(QWidget *parent = nullptr);
    void setLevels(float rmsDb, float peakDb, int clips);
    void resetHold();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float m_rms;
    float m_peak;
    float m_hold;
    int m_clips;
};

#endif

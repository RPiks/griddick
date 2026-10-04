/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * main.cpp - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "MainWindow.h"

#include "gt.h"

#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("gtstation"));
    app.setApplicationDisplayName(QStringLiteral("Griddick Station"));
    app.setApplicationVersion(QString::fromLatin1(GT_VERSION));
    app.setOrganizationName(QStringLiteral("Griddick"));

    MainWindow w;
    w.show();
    return app.exec();
}

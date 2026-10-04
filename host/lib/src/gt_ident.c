/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gt_ident.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gt_ident.h"

void gt_print_notice(FILE *fp, const char *prog)
{
    fprintf(fp,
            "Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.\n"
            "https://www.qrz.com/db/r2bdy\n"
            "Type '%s -L' for software license.\n"
            "\n",
            prog);
}

void gt_print_license(FILE *fp, const char *prog)
{
    fprintf(fp,
            "%s — Griddick TNC host tool\n"
            "Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.\n"
            "https://www.qrz.com/db/r2bdy\n"
            "\n"
            "This program is free software: you can redistribute it and/or modify\n"
            "it under the terms of the GNU Lesser General Public License as published\n"
            "by the Free Software Foundation, either version 2.1 of the License, or\n"
            "(at your option) any later version.\n"
            "\n"
            "This program is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
            "GNU Lesser General Public License for more details.\n"
            "\n"
            "You should have received a copy of the GNU Lesser General Public License\n"
            "along with this program. If not, see <https://www.gnu.org/licenses/>.\n",
            prog);
}

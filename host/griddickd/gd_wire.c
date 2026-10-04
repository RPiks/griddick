/*
 * Copyright (c) 2025-2026 Roman Piksaykin [piksaykin@gmail.com], callsign R2BDY.
 * https://www.qrz.com/db/r2bdy
 *
 * gd_wire.c - This file is part of Griddick TNC.
 *
 * LICENCE
 *     GNU Lesser General Public License v2.1 or later.
 *     https://www.gnu.org/licenses/lgpl-2.1.html
 */

#include "gd_wire.h"

#include <openssl/evp.h>
#include <string.h>

static int has_password(const char *password)
{
    return password != NULL && password[0] != '\0';
}

void gd_wire_auth(const char *password, uint16_t sport, uint16_t dport,
                  const uint8_t *payload, int n, uint8_t out[2])
{
    EVP_MD_CTX *ctx;
    unsigned char dig[32];
    unsigned int dlen = 0;
    uint8_t ports[4];

    out[0] = 0;
    out[1] = 0;
    ports[0] = (uint8_t)(sport >> 8);
    ports[1] = (uint8_t)(sport & 0xffu);
    ports[2] = (uint8_t)(dport >> 8);
    ports[3] = (uint8_t)(dport & 0xffu);

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return;
    }
    if (has_password(password)) {
        EVP_DigestUpdate(ctx, password, strlen(password));
    }
    EVP_DigestUpdate(ctx, ports, 4);
    if (n > 0 && payload != NULL) {
        EVP_DigestUpdate(ctx, payload, (size_t)n);
    }
    EVP_DigestFinal_ex(ctx, dig, &dlen);
    EVP_MD_CTX_free(ctx);
    if (dlen >= 2) {
        out[0] = dig[0];
        out[1] = dig[1];
    }
}

int gd_wire_pack(uint8_t *out, int max,
                 uint16_t sport, uint16_t dport,
                 const uint8_t *payload, int n,
                 const char *password)
{
    int need;
    int auth;

    if (out == NULL || max < GD_WIRE_HDR_LEN || n < 0) {
        return -1;
    }
    if (n > 0 && payload == NULL) {
        return -1;
    }
    auth = has_password(password) ? GD_WIRE_AUTH_LEN : 0;
    need = GD_WIRE_HDR_LEN + n + auth;
    if (need > max) {
        return -1;
    }
    out[0] = GD_WIRE_VER;
    out[1] = (uint8_t)(sport >> 8);
    out[2] = (uint8_t)(sport & 0xffu);
    out[3] = (uint8_t)(dport >> 8);
    out[4] = (uint8_t)(dport & 0xffu);
    if (n > 0) {
        memcpy(out + GD_WIRE_HDR_LEN, payload, (size_t)n);
    }
    if (auth) {
        gd_wire_auth(password, sport, dport, payload, n,
                     out + GD_WIRE_HDR_LEN + n);
    }
    return need;
}

int gd_wire_unpack(const uint8_t *info, int info_len,
                   const char *password,
                   uint16_t *sport, uint16_t *dport,
                   const uint8_t **payload, int *n)
{
    int auth;
    int body;
    uint16_t sp;
    uint16_t dp;
    const uint8_t *pl;
    uint8_t expect[2];

    if (info == NULL || info_len < GD_WIRE_HDR_LEN) {
        return -1;
    }
    if (info[0] != GD_WIRE_VER) {
        return -1;
    }
    auth = has_password(password) ? GD_WIRE_AUTH_LEN : 0;
    if (info_len < GD_WIRE_HDR_LEN + auth) {
        return -1;
    }
    body = info_len - GD_WIRE_HDR_LEN - auth;
    sp = (uint16_t)(((uint16_t)info[1] << 8) | info[2]);
    dp = (uint16_t)(((uint16_t)info[3] << 8) | info[4]);
    pl = info + GD_WIRE_HDR_LEN;
    if (auth) {
        gd_wire_auth(password, sp, dp, pl, body, expect);
        if (info[info_len - 2] != expect[0] ||
            info[info_len - 1] != expect[1]) {
            return -2;
        }
    }
    if (sport != NULL) {
        *sport = sp;
    }
    if (dport != NULL) {
        *dport = dp;
    }
    if (payload != NULL) {
        *payload = pl;
    }
    if (n != NULL) {
        *n = body;
    }
    return 0;
}

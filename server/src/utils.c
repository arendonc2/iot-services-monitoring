/*
 * utils.c — Funciones auxiliares reutilizables.
 */

#include "utils.h"
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

/* ── safe_strncpy ───────────────────────────────────────────────── */

void safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (dest == NULL || dest_size == 0)
        return;

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* ── trim ───────────────────────────────────────────────────────── */

char *trim(char *str)
{
    if (str == NULL)
        return NULL;

    /* Avanzar sobre espacios iniciales */
    while (isspace((unsigned char)*str))
        str++;

    if (*str == '\0')
        return str;

    /* Retroceder sobre espacios finales */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';
    return str;
}

/* ── get_timestamp ──────────────────────────────────────────────── */

void get_timestamp(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

/* ── recv_line ──────────────────────────────────────────────────── */

int recv_line(int sockfd, char *buf, size_t buf_size)
{
    size_t i = 0;
    char   c;
    ssize_t n;

    while (i < buf_size - 1) {
        n = recv(sockfd, &c, 1, 0);

        if (n < 0) {
            /* Error de red */
            return -1;
        }

        if (n == 0) {
            /* Conexión cerrada por el peer */
            if (i == 0)
                return 0;
            break;
        }

        if (c == '\n')
            break;

        /* Ignorar '\r' para compatibilidad Windows */
        if (c != '\r')
            buf[i++] = c;
    }

    buf[i] = '\0';
    return (int)i;
}

/* ── send_line ──────────────────────────────────────────────────── */

int send_line(int sockfd, const char *message)
{
    char buf[BUFFER_SIZE];
    int  len;

    len = snprintf(buf, sizeof(buf), "%s\n", message);
    if (len < 0 || (size_t)len >= sizeof(buf))
        return -1;

    ssize_t total_sent = 0;
    ssize_t to_send    = len;

    while (total_sent < to_send) {
        ssize_t sent = send(sockfd, buf + total_sent,
                            to_send - total_sent, 0);
        if (sent < 0)
            return -1;
        total_sent += sent;
    }

    return 0;
}

/* ── is_valid_timestamp ─────────────────────────────────────────── */

int is_valid_timestamp(const char *ts)
{
    /*
     * Formato esperado: YYYY-MM-DDTHH:MM:SSZ
     * Longitud exacta:  20 caracteres
     * Ejemplo:          2026-03-30T14:05:00Z
     */
    if (ts == NULL)
        return 0;

    size_t len = strlen(ts);
    if (len != 20)
        return 0;

    /* Verificar posiciones fijas */
    if (ts[4]  != '-' || ts[7]  != '-' || ts[10] != 'T' ||
        ts[13] != ':' || ts[16] != ':' || ts[19] != 'Z')
        return 0;

    /* Verificar que el resto sean dígitos */
    for (int i = 0; i < 20; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19)
            continue;
        if (!isdigit((unsigned char)ts[i]))
            return 0;
    }

    return 1;
}

/* ── parse_double ───────────────────────────────────────────────── */

int parse_double(const char *str, double *out)
{
    if (str == NULL || out == NULL)
        return 0;

    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);

    if (errno != 0 || endptr == str || *endptr != '\0')
        return 0;

    *out = val;
    return 1;
}

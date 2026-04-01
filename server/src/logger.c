/*
 * logger.c — Logging dual (consola + archivo), thread-safe.
 */

#include "logger.h"
#include "config.h"
#include "utils.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ── Estado interno ─────────────────────────────────────────────── */

static FILE           *log_file  = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Funciones internas ─────────────────────────────────────────── */

static void write_log(const char *line)
{
    pthread_mutex_lock(&log_mutex);

    /* Consola */
    printf("%s\n", line);
    fflush(stdout);

    /* Archivo */
    if (log_file != NULL) {
        fprintf(log_file, "%s\n", line);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

/* ── API pública ────────────────────────────────────────────────── */

int logger_init(const char *log_dir)
{
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", log_dir, LOG_FILENAME);

    pthread_mutex_lock(&log_mutex);

    log_file = fopen(path, "a");
    if (log_file == NULL) {
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "[FATAL] No se pudo abrir archivo de log: %s\n", path);
        return -1;
    }

    pthread_mutex_unlock(&log_mutex);

    char msg[256];
    snprintf(msg, sizeof(msg), "Logger inicializado. Archivo: %s", path);
    log_info(msg);

    return 0;
}

void logger_close(void)
{
    pthread_mutex_lock(&log_mutex);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_sensor(const char *ip, int port,
                const char *recv_msg, const char *resp_msg)
{
    char ts[32];
    char line[BUFFER_SIZE];

    get_timestamp(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "[%s] [SENSOR] IP=%s PORT=%d RECV=\"%s\" RESP=\"%s\"",
             ts, ip, port, recv_msg, resp_msg);
    write_log(line);
}

void log_http(const char *ip, int port,
              const char *request, const char *response)
{
    char ts[32];
    char line[BUFFER_SIZE];

    get_timestamp(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "[%s] [HTTP]   IP=%s PORT=%d REQ=\"%s\" RESP=\"%s\"",
             ts, ip, port, request, response);
    write_log(line);
}

void log_error(const char *ip, int port, const char *error_msg)
{
    char ts[32];
    char line[BUFFER_SIZE];

    get_timestamp(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "[%s] [ERROR]  IP=%s PORT=%d MSG=\"%s\"",
             ts, ip, port, error_msg);
    write_log(line);
}

void log_info(const char *message)
{
    char ts[32];
    char line[BUFFER_SIZE];

    get_timestamp(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "[%s] [INFO]   MSG=\"%s\"",
             ts, message);
    write_log(line);
}

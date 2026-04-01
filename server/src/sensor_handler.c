/*
 * sensor_handler.c — Procesamiento del protocolo SMP 1.0.
 *
 * Cada thread worker atiende una conexión de sensor.
 * Lee líneas, parsea comandos, ejecuta lógica y responde.
 */

#include "sensor_handler.h"
#include "config.h"
#include "state.h"
#include "alerts.h"
#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Prototipos internos ────────────────────────────────────────── */

static void handle_register(int fd, const char *ip, int port,
                            char *tokens[], int token_count,
                            const char *raw);
static void handle_metric(int fd, const char *ip, int port,
                          char *tokens[], int token_count,
                          const char *raw);
static void handle_status(int fd, const char *ip, int port,
                          char *tokens[], int token_count,
                          const char *raw);
static void handle_ping(int fd, const char *ip, int port,
                        char *tokens[], int token_count,
                        const char *raw);
static int  handle_quit(int fd, const char *ip, int port,
                        char *tokens[], int token_count,
                        const char *raw);

static int  tokenize(char *line, char *tokens[], int max_tokens);
static void send_response(int fd, const char *ip, int port,
                          const char *recv_msg, const char *resp);

/* ── Thread principal del handler ───────────────────────────────── */

void *handle_sensor_connection(void *arg)
{
    SensorClientInfo *info = (SensorClientInfo *)arg;
    int   fd   = info->client_fd;
    char *ip   = info->client_ip;
    int   port = info->client_port;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Sensor client connected: %s:%d", ip, port);
    log_info(msg);

    char line[BUFFER_SIZE];

    while (1) {
        int n = recv_line(fd, line, sizeof(line));

        if (n <= 0) {
            /* Conexión cerrada o error */
            if (n == 0) {
                snprintf(msg, sizeof(msg),
                         "Sensor client disconnected: %s:%d", ip, port);
                log_info(msg);
            } else {
                log_error(ip, port, "Error reading from sensor socket");
            }
            break;
        }

        /* Guardar copia del mensaje original para logging */
        char raw[BUFFER_SIZE];
        safe_strncpy(raw, line, sizeof(raw));

        /* Tokenizar */
        char *tokens[8];
        int   token_count = tokenize(line, tokens, 8);

        if (token_count == 0) {
            send_response(fd, ip, port, raw, "400 BAD_REQUEST");
            continue;
        }

        const char *command = tokens[0];

        if (strcmp(command, "REGISTER") == 0) {
            handle_register(fd, ip, port, tokens, token_count, raw);

        } else if (strcmp(command, "METRIC") == 0) {
            handle_metric(fd, ip, port, tokens, token_count, raw);

        } else if (strcmp(command, "STATUS") == 0) {
            handle_status(fd, ip, port, tokens, token_count, raw);

        } else if (strcmp(command, "PING") == 0) {
            handle_ping(fd, ip, port, tokens, token_count, raw);

        } else if (strcmp(command, "QUIT") == 0) {
            int should_exit = handle_quit(fd, ip, port,
                                          tokens, token_count, raw);
            if (should_exit)
                break;

        } else {
            send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        }
    }

    close(fd);

    snprintf(msg, sizeof(msg),
             "Sensor handler thread finished for %s:%d", ip, port);
    log_info(msg);

    free(info);
    return NULL;
}

/* ── REGISTER ───────────────────────────────────────────────────── */

static void handle_register(int fd, const char *ip, int port,
                            char *tokens[], int token_count,
                            const char *raw)
{
    if (token_count != 3) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return;
    }

    const char *sensor_id   = tokens[1];
    const char *type_str    = tokens[2];
    SensorType  sensor_type = sensor_type_from_str(type_str);

    if (sensor_type == SENSOR_TYPE_INVALID) {
        send_response(fd, ip, port, raw, "422 INVALID_SENSOR_TYPE");
        return;
    }

    int result = state_register_sensor(sensor_id, sensor_type);

    char resp[BUFFER_SIZE];

    switch (result) {
        case 0:
            snprintf(resp, sizeof(resp), "200 REGISTERED %s", sensor_id);
            break;
        case -1:
            snprintf(resp, sizeof(resp), "500 INTERNAL_ERROR");
            break;
        case -2:
            snprintf(resp, sizeof(resp), "409 SENSOR_ALREADY_EXISTS");
            break;
        case -3:
            snprintf(resp, sizeof(resp), "422 INVALID_SENSOR_TYPE");
            break;
        default:
            snprintf(resp, sizeof(resp), "500 INTERNAL_ERROR");
            break;
    }

    send_response(fd, ip, port, raw, resp);
}

/* ── METRIC ─────────────────────────────────────────────────────── */

static void handle_metric(int fd, const char *ip, int port,
                          char *tokens[], int token_count,
                          const char *raw)
{
    if (token_count != 4) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return;
    }

    const char *sensor_id    = tokens[1];
    const char *value_str    = tokens[2];
    const char *timestamp    = tokens[3];

    /* Verificar que el sensor exista y esté registrado */
    Sensor sensor;
    int idx = state_find_sensor(sensor_id, &sensor);

    if (idx < 0) {
        send_response(fd, ip, port, raw, "404 SENSOR_NOT_FOUND");
        return;
    }

    if (sensor.status != STATUS_ACTIVE) {
        send_response(fd, ip, port, raw, "403 SENSOR_NOT_REGISTERED");
        return;
    }

    /* Validar valor numérico */
    double value;
    if (!parse_double(value_str, &value)) {
        send_response(fd, ip, port, raw, "422 INVALID_VALUE");
        return;
    }

    /* Validar timestamp */
    if (!is_valid_timestamp(timestamp)) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return;
    }

    /* Registrar métrica */
    const char *type_str = sensor_type_to_str(sensor.type);
    int result = state_add_metric(sensor_id, value, timestamp, type_str);

    if (result < 0) {
        send_response(fd, ip, port, raw, "500 INTERNAL_ERROR");
        return;
    }

    /* Evaluar anomalía */
    alerts_evaluate(sensor_id, value, sensor.type, timestamp);

    send_response(fd, ip, port, raw, "200 OK");
}

/* ── STATUS ─────────────────────────────────────────────────────── */

static void handle_status(int fd, const char *ip, int port,
                          char *tokens[], int token_count,
                          const char *raw)
{
    if (token_count != 2) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return;
    }

    const char *sensor_id = tokens[1];
    Sensor sensor;
    int idx = state_find_sensor(sensor_id, &sensor);

    if (idx < 0) {
        send_response(fd, ip, port, raw, "404 SENSOR_NOT_FOUND");
        return;
    }

    char resp[BUFFER_SIZE];
    snprintf(resp, sizeof(resp),
             "200 STATUS %s %s %s %.2f %s %s",
             sensor.id,
             sensor_status_to_str(sensor.status),
             sensor_type_to_str(sensor.type),
             sensor.last_value,
             sensor.last_timestamp,
             alert_level_to_str(sensor.alert_level));

    send_response(fd, ip, port, raw, resp);
}

/* ── PING ───────────────────────────────────────────────────────── */

static void handle_ping(int fd, const char *ip, int port,
                        char *tokens[], int token_count,
                        const char *raw)
{
    if (token_count != 2) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return;
    }

    send_response(fd, ip, port, raw, "200 PONG");
}

/* ── QUIT ───────────────────────────────────────────────────────── */

static int handle_quit(int fd, const char *ip, int port,
                       char *tokens[], int token_count,
                       const char *raw)
{
    if (token_count != 2) {
        send_response(fd, ip, port, raw, "400 BAD_REQUEST");
        return 0;  /* No salir, comando mal formado */
    }

    const char *sensor_id = tokens[1];
    state_deactivate_sensor(sensor_id);

    send_response(fd, ip, port, raw, "200 BYE");
    return 1;  /* Señal para salir del loop */
}

/* ── Funciones auxiliares ───────────────────────────────────────── */

static int tokenize(char *line, char *tokens[], int max_tokens)
{
    int count = 0;
    char *saveptr;
    char *token = strtok_r(line, " \t", &saveptr);

    while (token != NULL && count < max_tokens) {
        tokens[count++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }

    return count;
}

static void send_response(int fd, const char *ip, int port,
                          const char *recv_msg, const char *resp)
{
    send_line(fd, resp);
    log_sensor(ip, port, recv_msg, resp);
}

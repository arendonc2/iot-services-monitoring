/*
 * main.c — Entry point del servidor IoT Services Monitoring.
 *
 * Parsea argumentos CLI, inicializa módulos y lanza los threads
 * del servidor de sensores (TCP:9090) y HTTP (HTTP:8080).
 */

#include "config.h"
#include "state.h"
#include "logger.h"
#include "sensor_server.h"
#include "http_server.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Variable global de configuración ───────────────────────────── */

ServerConfig g_config;

/* ── Ayuda ──────────────────────────────────────────────────────── */

static void print_usage(const char *progname)
{
    fprintf(stderr,
        "Uso: %s [opciones]\n"
        "\n"
        "Opciones:\n"
        "  --sensor-port <puerto>   Puerto TCP para sensores   (default: %d)\n"
        "  --http-port   <puerto>   Puerto HTTP para operadores (default: %d)\n"
        "  --auth-host   <host>     Hostname del auth service   (default: %s)\n"
        "  --auth-port   <puerto>   Puerto del auth service     (default: %d)\n"
        "  --log-dir     <ruta>     Directorio de logs          (default: %s)\n"
        "  --help                   Muestra esta ayuda\n"
        "\n"
        "Ejemplo:\n"
        "  %s --auth-host auth.iot-monitor.example.com --log-dir /app/logs\n",
        progname,
        SENSOR_PORT_DEFAULT,
        HTTP_PORT_DEFAULT,
        AUTH_HOST_DEFAULT,
        AUTH_PORT_DEFAULT,
        LOG_DIR_DEFAULT,
        progname);
}

/* ── Parseo de argumentos ───────────────────────────────────────── */

static int parse_args(int argc, char *argv[])
{
    /* Valores por defecto */
    g_config.sensor_port = SENSOR_PORT_DEFAULT;
    g_config.http_port   = HTTP_PORT_DEFAULT;
    g_config.auth_port   = AUTH_PORT_DEFAULT;
    strncpy(g_config.auth_host, AUTH_HOST_DEFAULT, MAX_ID_LEN - 1);
    strncpy(g_config.log_dir,   LOG_DIR_DEFAULT,   MAX_PATH_LEN - 1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }

        /* Todos los demás flags requieren un valor */
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: falta valor para %s\n", argv[i]);
            return -1;
        }

        if (strcmp(argv[i], "--sensor-port") == 0) {
            g_config.sensor_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--http-port") == 0) {
            g_config.http_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--auth-host") == 0) {
            strncpy(g_config.auth_host, argv[++i], MAX_ID_LEN - 1);
            g_config.auth_host[MAX_ID_LEN - 1] = '\0';
        } else if (strcmp(argv[i], "--auth-port") == 0) {
            g_config.auth_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-dir") == 0) {
            strncpy(g_config.log_dir, argv[++i], MAX_PATH_LEN - 1);
            g_config.log_dir[MAX_PATH_LEN - 1] = '\0';
        } else {
            fprintf(stderr, "Error: argumento desconocido: %s\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* Ignorar SIGPIPE para evitar crash al escribir en socket cerrado */
    signal(SIGPIPE, SIG_IGN);

    /* Parsear argumentos */
    if (parse_args(argc, argv) < 0) {
        print_usage(argv[0]);
        return 1;
    }

    /* Inicializar logger */
    if (logger_init(g_config.log_dir) < 0) {
        fprintf(stderr, "FATAL: No se pudo inicializar el logger.\n");
        return 1;
    }

    /* Inicializar estado global */
    state_init();

    /* Mostrar configuración */
    {
        char msg[256];
        log_info("========================================");
        log_info("IoT Services Monitoring Server starting");
        log_info("========================================");

        snprintf(msg, sizeof(msg), "Sensor port : %d", g_config.sensor_port);
        log_info(msg);
        snprintf(msg, sizeof(msg), "HTTP port   : %d", g_config.http_port);
        log_info(msg);
        snprintf(msg, sizeof(msg), "Auth service: %s:%d",
                 g_config.auth_host, g_config.auth_port);
        log_info(msg);
        snprintf(msg, sizeof(msg), "Log dir     : %s", g_config.log_dir);
        log_info(msg);
    }

    /* Lanzar thread del servidor de sensores */
    pthread_t sensor_thread;
    if (pthread_create(&sensor_thread, NULL, sensor_server_start, NULL) != 0) {
        log_info("FATAL: Cannot create sensor server thread");
        state_destroy();
        logger_close();
        return 1;
    }

    /* Lanzar thread del servidor HTTP */
    pthread_t http_thread;
    if (pthread_create(&http_thread, NULL, http_server_start, NULL) != 0) {
        log_info("FATAL: Cannot create HTTP server thread");
        state_destroy();
        logger_close();
        return 1;
    }

    log_info("All servers started. Waiting for connections...");

    /* Esperar a que ambos threads terminen (normalmente no terminan) */
    pthread_join(sensor_thread, NULL);
    pthread_join(http_thread, NULL);

    /* Limpieza (se alcanza solo si los servers terminan) */
    state_destroy();
    logger_close();

    return 0;
}

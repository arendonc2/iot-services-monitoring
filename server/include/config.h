/*
 * config.h — Constantes globales de configuración del servidor IoT.
 *
 * Todos los valores por defecto pueden sobreescribirse mediante
 * argumentos de línea de comandos en main.c.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ── Tamaños de buffers ─────────────────────────────────────────── */
#define BUFFER_SIZE         4096
#define MAX_ID_LEN            64
#define MAX_TIMESTAMP_LEN     32
#define MAX_TYPE_LEN           8
#define MAX_MESSAGE_LEN      256
#define MAX_PATH_LEN         256
#define MAX_RESPONSE_LEN    8192
#define HTTP_RESPONSE_MAX  65536

/* ── Capacidades máximas ────────────────────────────────────────── */
#define MAX_SENSORS           64
#define MAX_ALERTS           256
#define MAX_METRICS          512

/* ── Puertos por defecto ────────────────────────────────────────── */
#define SENSOR_PORT_DEFAULT 9090
#define HTTP_PORT_DEFAULT   8080

/* ── Microservicio de autenticación ─────────────────────────────── */
#define AUTH_HOST_DEFAULT   "localhost"
#define AUTH_PORT_DEFAULT   5000

/* ── Logging ────────────────────────────────────────────────────── */
#define LOG_DIR_DEFAULT     "logs"
#define LOG_FILENAME        "server.log"

/* ── Backlog de conexiones ──────────────────────────────────────── */
#define LISTEN_BACKLOG        16

/* ── Configuración en tiempo de ejecución ───────────────────────── */
typedef struct {
    int  sensor_port;
    int  http_port;
    char auth_host[MAX_ID_LEN];
    int  auth_port;
    char log_dir[MAX_PATH_LEN];
} ServerConfig;

/*
 * Configuración global — inicializada en main.c,
 * leída (solo lectura) desde cualquier módulo.
 */
extern ServerConfig g_config;

#endif /* CONFIG_H */

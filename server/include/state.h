/*
 * state.h — Estado global compartido del servidor IoT.
 *
 * Contiene las tablas de sensores, métricas y alertas,
 * protegidas por un mutex para acceso concurrente.
 */

#ifndef STATE_H
#define STATE_H

#include <pthread.h>
#include "config.h"

/* ── Enumeraciones ──────────────────────────────────────────────── */

typedef enum {
    SENSOR_TEMP,
    SENSOR_VIB,
    SENSOR_POWER,
    SENSOR_TYPE_INVALID
} SensorType;

typedef enum {
    STATUS_ACTIVE,
    STATUS_INACTIVE
} SensorStatus;

typedef enum {
    ALERT_NORMAL,
    ALERT_ALERT
} AlertLevel;

/* ── Estructuras ────────────────────────────────────────────────── */

typedef struct {
    char          id[MAX_ID_LEN];
    SensorType    type;
    SensorStatus  status;
    AlertLevel    alert_level;
    double        last_value;
    char          last_timestamp[MAX_TIMESTAMP_LEN];
    int           active;   /* 1 = slot ocupado en la tabla */
} Sensor;

typedef struct {
    char   sensor_id[MAX_ID_LEN];
    double value;
    char   timestamp[MAX_TIMESTAMP_LEN];
    char   type_str[MAX_TYPE_LEN];
} Metric;

typedef struct {
    char   sensor_id[MAX_ID_LEN];
    double value;
    double threshold;
    char   timestamp[MAX_TIMESTAMP_LEN];
    char   type_str[MAX_TYPE_LEN];
    char   message[MAX_MESSAGE_LEN];
} Alert;

typedef struct {
    Sensor  sensors[MAX_SENSORS];
    int     sensor_count;

    Metric  metrics[MAX_METRICS];
    int     metric_count;
    int     metric_next;   /* índice circular de escritura */

    Alert   alerts[MAX_ALERTS];
    int     alert_count;
    int     alert_next;    /* índice circular de escritura */

    pthread_mutex_t lock;
} GlobalState;

/* ── Variable global ────────────────────────────────────────────── */

extern GlobalState g_state;

/* ── API pública ────────────────────────────────────────────────── */

/* Inicializa mutex y pone todo a cero. */
void state_init(void);

/* Destruye mutex. */
void state_destroy(void);

/*
 * Registra un sensor nuevo.
 * Retorna:  0 = OK,  -1 = tabla llena,  -2 = ya existe,  -3 = tipo inválido.
 */
int state_register_sensor(const char *id, SensorType type);

/*
 * Busca un sensor por ID.
 * Si found != NULL, copia el sensor encontrado.
 * Retorna índice en la tabla si existe, -1 si no.
 */
int state_find_sensor(const char *id, Sensor *found);

/*
 * Agrega una métrica a la tabla circular.
 * Actualiza el last_value y last_timestamp del sensor correspondiente.
 * Retorna 0 = OK, -1 = sensor no encontrado.
 */
int state_add_metric(const char *sensor_id, double value,
                     const char *timestamp, const char *type_str);

/*
 * Registra una alerta en la tabla circular.
 */
void state_add_alert(const char *sensor_id, double value,
                     double threshold, const char *timestamp,
                     const char *type_str, const char *message);

/*
 * Marca un sensor como INACTIVE.
 * Retorna 0 = OK, -1 = no encontrado.
 */
int state_deactivate_sensor(const char *id);

/*
 * Actualiza el alert_level de un sensor.
 * Retorna 0 = OK, -1 = no encontrado.
 */
int state_set_alert_level(const char *id, AlertLevel level);

/* ── Funciones de consulta (para HTTP) ──────────────────────────── */

/* Devuelve el número de sensores registrados. */
int state_get_sensor_count(void);

/* Devuelve el número de sensores activos. */
int state_get_active_sensor_count(void);

/* Devuelve el número de alertas registradas. */
int state_get_alert_count(void);

/* Devuelve el número de métricas registradas. */
int state_get_metric_count(void);

/* ── Conversión de tipos ────────────────────────────────────────── */

SensorType  sensor_type_from_str(const char *str);
const char *sensor_type_to_str(SensorType type);
const char *sensor_status_to_str(SensorStatus status);
const char *alert_level_to_str(AlertLevel level);

#endif /* STATE_H */

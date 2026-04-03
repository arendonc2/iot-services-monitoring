/*
 * state.c — Estado global compartido del servidor IoT.
 *
 * Todas las operaciones adquieren g_state.lock antes de leer/escribir.
 */

#include "state.h"
#include "utils.h"

#include <string.h>
#include <stdio.h>

/* ── Variable global ────────────────────────────────────────────── */

GlobalState g_state;

/* ── Inicialización / destrucción ───────────────────────────────── */

void state_init(void)
{
    memset(&g_state, 0, sizeof(GlobalState));
    pthread_mutex_init(&g_state.lock, NULL);
}

void state_destroy(void)
{
    pthread_mutex_destroy(&g_state.lock);
}

/* ── Conversión de tipos ────────────────────────────────────────── */

SensorType sensor_type_from_str(const char *str)
{
    if (str == NULL)            return SENSOR_TYPE_INVALID;
    if (strcmp(str, "TEMP")  == 0) return SENSOR_TEMP;
    if (strcmp(str, "VIB")   == 0) return SENSOR_VIB;
    if (strcmp(str, "POWER") == 0) return SENSOR_POWER;
    return SENSOR_TYPE_INVALID;
}

const char *sensor_type_to_str(SensorType type)
{
    switch (type) {
        case SENSOR_TEMP:  return "TEMP";
        case SENSOR_VIB:   return "VIB";
        case SENSOR_POWER: return "POWER";
        default:           return "UNKNOWN";
    }
}

const char *sensor_status_to_str(SensorStatus status)
{
    switch (status) {
        case STATUS_ACTIVE:   return "ACTIVE";
        case STATUS_INACTIVE: return "INACTIVE";
        default:              return "UNKNOWN";
    }
}

const char *alert_level_to_str(AlertLevel level)
{
    switch (level) {
        case ALERT_NORMAL: return "NORMAL";
        case ALERT_ALERT:  return "ALERT";
        default:           return "UNKNOWN";
    }
}

/* ── Registro de sensores ───────────────────────────────────────── */

int state_register_sensor(const char *id, SensorType type)
{
    if (type == SENSOR_TYPE_INVALID)
        return -3;

    pthread_mutex_lock(&g_state.lock);

    /* Verificar si ya existe */
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            strcmp(g_state.sensors[i].id, id) == 0) {
            if (g_state.sensors[i].status == STATUS_INACTIVE) {
                /* Reactivar sensor que había enviado QUIT */
                g_state.sensors[i].status = STATUS_ACTIVE;
                pthread_mutex_unlock(&g_state.lock);
                return 0;
            }
            pthread_mutex_unlock(&g_state.lock);
            return -2;  /* Ya existe y está activo */
        }
    }

    /* Buscar slot libre */
    int slot = -1;
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!g_state.sensors[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&g_state.lock);
        return -1;  /* Tabla llena */
    }

    /* Inicializar el sensor */
    Sensor *s = &g_state.sensors[slot];
    memset(s, 0, sizeof(Sensor));
    safe_strncpy(s->id, id, MAX_ID_LEN);
    s->type        = type;
    s->status      = STATUS_ACTIVE;
    s->alert_level = ALERT_NORMAL;
    s->last_value  = 0.0;
    s->active      = 1;
    safe_strncpy(s->last_timestamp, "N/A", MAX_TIMESTAMP_LEN);

    g_state.sensor_count++;

    pthread_mutex_unlock(&g_state.lock);
    return 0;
}

/* ── Búsqueda ───────────────────────────────────────────────────── */

int state_find_sensor(const char *id, Sensor *found)
{
    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            strcmp(g_state.sensors[i].id, id) == 0) {

            if (found != NULL)
                *found = g_state.sensors[i];

            pthread_mutex_unlock(&g_state.lock);
            return i;
        }
    }

    pthread_mutex_unlock(&g_state.lock);
    return -1;
}

/* ── Métricas ───────────────────────────────────────────────────── */

int state_add_metric(const char *sensor_id, double value,
                     const char *timestamp, const char *type_str)
{
    pthread_mutex_lock(&g_state.lock);

    /* Buscar sensor y actualizar su último valor */
    int found = -1;
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            strcmp(g_state.sensors[i].id, sensor_id) == 0) {
            g_state.sensors[i].last_value = value;
            safe_strncpy(g_state.sensors[i].last_timestamp,
                         timestamp, MAX_TIMESTAMP_LEN);
            found = i;
            break;
        }
    }

    if (found < 0) {
        pthread_mutex_unlock(&g_state.lock);
        return -1;
    }

    /* Agregar métrica al buffer circular */
    Metric *m = &g_state.metrics[g_state.metric_next];
    safe_strncpy(m->sensor_id, sensor_id, MAX_ID_LEN);
    m->value = value;
    safe_strncpy(m->timestamp, timestamp, MAX_TIMESTAMP_LEN);
    safe_strncpy(m->type_str, type_str, MAX_TYPE_LEN);

    g_state.metric_next = (g_state.metric_next + 1) % MAX_METRICS;
    if (g_state.metric_count < MAX_METRICS)
        g_state.metric_count++;

    pthread_mutex_unlock(&g_state.lock);
    return 0;
}

/* ── Alertas ────────────────────────────────────────────────────── */

void state_add_alert(const char *sensor_id, double value,
                     double threshold, const char *timestamp,
                     const char *type_str, const char *message)
{
    pthread_mutex_lock(&g_state.lock);

    Alert *a = &g_state.alerts[g_state.alert_next];
    safe_strncpy(a->sensor_id, sensor_id, MAX_ID_LEN);
    a->value     = value;
    a->threshold = threshold;
    safe_strncpy(a->timestamp, timestamp, MAX_TIMESTAMP_LEN);
    safe_strncpy(a->type_str, type_str, MAX_TYPE_LEN);
    safe_strncpy(a->message, message, MAX_MESSAGE_LEN);

    g_state.alert_next = (g_state.alert_next + 1) % MAX_ALERTS;
    if (g_state.alert_count < MAX_ALERTS)
        g_state.alert_count++;

    pthread_mutex_unlock(&g_state.lock);
}

/* ── Desactivar sensor ──────────────────────────────────────────── */

int state_deactivate_sensor(const char *id)
{
    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            strcmp(g_state.sensors[i].id, id) == 0) {
            g_state.sensors[i].status = STATUS_INACTIVE;
            pthread_mutex_unlock(&g_state.lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_state.lock);
    return -1;
}

/* ── Actualizar alert_level ─────────────────────────────────────── */

int state_set_alert_level(const char *id, AlertLevel level)
{
    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            strcmp(g_state.sensors[i].id, id) == 0) {
            g_state.sensors[i].alert_level = level;
            pthread_mutex_unlock(&g_state.lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_state.lock);
    return -1;
}

/* ── Funciones de consulta ──────────────────────────────────────── */

int state_get_sensor_count(void)
{
    pthread_mutex_lock(&g_state.lock);
    int count = g_state.sensor_count;
    pthread_mutex_unlock(&g_state.lock);
    return count;
}

int state_get_active_sensor_count(void)
{
    int count = 0;
    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (g_state.sensors[i].active &&
            g_state.sensors[i].status == STATUS_ACTIVE)
            count++;
    }

    pthread_mutex_unlock(&g_state.lock);
    return count;
}

int state_get_alert_count(void)
{
    pthread_mutex_lock(&g_state.lock);
    int count = g_state.alert_count;
    pthread_mutex_unlock(&g_state.lock);
    return count;
}

int state_get_metric_count(void)
{
    pthread_mutex_lock(&g_state.lock);
    int count = g_state.metric_count;
    pthread_mutex_unlock(&g_state.lock);
    return count;
}

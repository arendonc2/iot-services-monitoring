/*
 * alerts.h — Evaluación de umbrales y detección de anomalías.
 */

#ifndef ALERTS_H
#define ALERTS_H

#include "state.h"

/* Umbrales de anomalía por tipo de sensor */
#define TEMP_THRESHOLD_HIGH   70.0
#define VIB_THRESHOLD_HIGH    50.0
#define POWER_THRESHOLD_LOW   10.0
#define POWER_THRESHOLD_HIGH 100.0

/*
 * Evalúa si un valor es anómalo para el tipo de sensor dado.
 * Si es anómalo, registra la alerta en g_state y actualiza
 * el alert_level del sensor.
 *
 * Retorna 1 si se generó alerta, 0 si el valor es normal.
 */
int alerts_evaluate(const char *sensor_id, double value,
                    SensorType type, const char *timestamp);

#endif /* ALERTS_H */

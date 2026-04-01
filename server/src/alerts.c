/*
 * alerts.c — Evaluación de umbrales y detección de anomalías.
 */

#include "alerts.h"
#include "state.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

int alerts_evaluate(const char *sensor_id, double value,
                    SensorType type, const char *timestamp)
{
    int         is_alert = 0;
    double      threshold = 0.0;
    char        message[MAX_MESSAGE_LEN];
    const char *type_str = sensor_type_to_str(type);

    switch (type) {
        case SENSOR_TEMP:
            if (value > TEMP_THRESHOLD_HIGH) {
                is_alert  = 1;
                threshold = TEMP_THRESHOLD_HIGH;
                snprintf(message, sizeof(message),
                         "TEMP anomaly: %.2f > %.2f", value, threshold);
            }
            break;

        case SENSOR_VIB:
            if (value > VIB_THRESHOLD_HIGH) {
                is_alert  = 1;
                threshold = VIB_THRESHOLD_HIGH;
                snprintf(message, sizeof(message),
                         "VIB anomaly: %.2f > %.2f", value, threshold);
            }
            break;

        case SENSOR_POWER:
            if (value < POWER_THRESHOLD_LOW) {
                is_alert  = 1;
                threshold = POWER_THRESHOLD_LOW;
                snprintf(message, sizeof(message),
                         "POWER anomaly: %.2f < %.2f", value, threshold);
            } else if (value > POWER_THRESHOLD_HIGH) {
                is_alert  = 1;
                threshold = POWER_THRESHOLD_HIGH;
                snprintf(message, sizeof(message),
                         "POWER anomaly: %.2f > %.2f", value, threshold);
            }
            break;

        default:
            break;
    }

    if (is_alert) {
        state_add_alert(sensor_id, value, threshold, timestamp,
                        type_str, message);
        state_set_alert_level(sensor_id, ALERT_ALERT);
    } else {
        state_set_alert_level(sensor_id, ALERT_NORMAL);
    }

    return is_alert;
}

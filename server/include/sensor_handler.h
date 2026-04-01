/*
 * sensor_handler.h — Procesamiento del protocolo SMP 1.0 por conexión.
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

/*
 * Información de contexto de un cliente sensor conectado.
 */
typedef struct {
    int  client_fd;
    char client_ip[64];
    int  client_port;
} SensorClientInfo;

/*
 * Función principal del thread worker para un sensor.
 * Recibe un puntero a SensorClientInfo (heap-allocated, se libera aquí).
 * Lee líneas del socket, parsea comandos SMP, ejecuta lógica,
 * envía respuesta, loggea. Termina al recibir QUIT o al cerrar conexión.
 */
void *handle_sensor_connection(void *arg);

#endif /* SENSOR_HANDLER_H */

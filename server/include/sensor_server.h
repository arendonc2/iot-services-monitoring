/*
 * sensor_server.h — Servidor TCP para sensores (puerto 9090).
 */

#ifndef SENSOR_SERVER_H
#define SENSOR_SERVER_H

/*
 * Inicia el servidor TCP de sensores.
 * Crea socket, bind, listen y entra en accept loop infinito.
 * Por cada conexión aceptada, lanza un thread con handle_sensor_connection().
 *
 * Esta función es el entry point del thread del sensor server
 * (se pasa a pthread_create desde main).
 *
 * El argumento es NULL (usa g_config.sensor_port).
 */
void *sensor_server_start(void *arg);

#endif /* SENSOR_SERVER_H */

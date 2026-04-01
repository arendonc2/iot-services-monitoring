/*
 * sensor_server.c — Servidor TCP para sensores (puerto 9090).
 *
 * Accept loop que crea un thread detached por cada conexión entrante.
 */

#include "sensor_server.h"
#include "sensor_handler.h"
#include "config.h"
#include "logger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void *sensor_server_start(void *arg)
{
    (void)arg;  /* No usado */

    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    /* Crear socket TCP */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_info("FATAL: Cannot create sensor server socket");
        return NULL;
    }

    /* Reusar dirección para evitar TIME_WAIT */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(g_config.sensor_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "FATAL: Cannot bind sensor server to port %d",
                 g_config.sensor_port);
        log_info(msg);
        close(server_fd);
        return NULL;
    }

    /* Listen */
    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        log_info("FATAL: Cannot listen on sensor server socket");
        close(server_fd);
        return NULL;
    }

    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Sensor server listening on port %d",
                 g_config.sensor_port);
        log_info(msg);
    }

    /* Accept loop */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            log_info("WARNING: accept() failed on sensor server");
            continue;
        }

        /* Preparar info del cliente para el thread */
        SensorClientInfo *info = malloc(sizeof(SensorClientInfo));
        if (info == NULL) {
            log_info("ERROR: malloc failed for SensorClientInfo");
            close(client_fd);
            continue;
        }

        info->client_fd   = client_fd;
        info->client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  info->client_ip, sizeof(info->client_ip));

        /* Lanzar thread detached */
        pthread_t tid;
        if (pthread_create(&tid, NULL,
                           handle_sensor_connection, info) != 0) {
            log_info("ERROR: pthread_create failed for sensor handler");
            close(client_fd);
            free(info);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    return NULL;
}

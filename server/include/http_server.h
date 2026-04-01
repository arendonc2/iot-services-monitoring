/*
 * http_server.h — Servidor HTTP básico para operadores (puerto 8080).
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/*
 * Inicia el servidor HTTP para operadores.
 * Crea socket, bind, listen y entra en accept loop infinito.
 * Por cada conexión, lanza un thread que atiende la request HTTP,
 * genera la respuesta HTML y cierra la conexión.
 *
 * Endpoints:
 *   GET /             → página de inicio/login
 *   GET /login?user=X → validación contra auth service
 *   GET /dashboard    → resumen del sistema
 *   GET /sensors      → tabla de sensores
 *   GET /alerts       → tabla de alertas
 *   GET /metrics      → últimas métricas
 *
 * Esta función es el entry point del thread del HTTP server
 * (se pasa a pthread_create desde main).
 *
 * El argumento es NULL (usa g_config.http_port).
 */
void *http_server_start(void *arg);

#endif /* HTTP_SERVER_H */

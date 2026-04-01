/*
 * auth_client.c — Cliente HTTP mínimo para consultar el microservicio
 *                 de autenticación externo.
 *
 * Resuelve hostname con getaddrinfo() (compatible con Docker DNS y Route 53).
 * Envía GET /auth?user=X y parsea la respuesta JSON.
 */

#include "auth_client.h"
#include "config.h"
#include "utils.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

AuthResult auth_check_user(const char *username)
{
    AuthResult result;
    memset(&result, 0, sizeof(AuthResult));

    if (username == NULL || strlen(username) == 0) {
        result.error = 1;
        safe_strncpy(result.error_msg, "Empty username",
                     sizeof(result.error_msg));
        return result;
    }

    /* ── Resolver hostname con getaddrinfo ──────────────────────── */

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", g_config.auth_port);

    int gai_err = getaddrinfo(g_config.auth_host, port_str, &hints, &res);
    if (gai_err != 0) {
        result.error = 1;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "DNS resolution failed for %s: %s",
                 g_config.auth_host, gai_strerror(gai_err));
        return result;
    }

    /* ── Crear socket y conectar ────────────────────────────────── */

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        result.error = 1;
        safe_strncpy(result.error_msg, "Cannot create socket for auth",
                     sizeof(result.error_msg));
        freeaddrinfo(res);
        return result;
    }

    /* Timeout de 5 segundos */
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        result.error = 1;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "Cannot connect to auth service at %s:%d",
                 g_config.auth_host, g_config.auth_port);
        close(sockfd);
        freeaddrinfo(res);
        return result;
    }

    freeaddrinfo(res);

    /* ── Enviar request HTTP GET ────────────────────────────────── */

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "GET /auth?user=%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             username, g_config.auth_host);

    ssize_t sent = send(sockfd, request, strlen(request), 0);
    if (sent < 0) {
        result.error = 1;
        safe_strncpy(result.error_msg, "Failed to send auth request",
                     sizeof(result.error_msg));
        close(sockfd);
        return result;
    }

    /* ── Leer respuesta completa ────────────────────────────────── */

    char response[BUFFER_SIZE];
    size_t total_read = 0;

    while (total_read < sizeof(response) - 1) {
        ssize_t n = recv(sockfd, response + total_read,
                         sizeof(response) - 1 - total_read, 0);
        if (n <= 0)
            break;
        total_read += n;
    }

    response[total_read] = '\0';
    close(sockfd);

    if (total_read == 0) {
        result.error = 1;
        safe_strncpy(result.error_msg, "Empty response from auth service",
                     sizeof(result.error_msg));
        return result;
    }

    /* ── Buscar el body (después de \r\n\r\n) ───────────────────── */

    char *body = strstr(response, "\r\n\r\n");
    if (body == NULL) {
        result.error = 1;
        safe_strncpy(result.error_msg,
                     "Invalid HTTP response from auth service",
                     sizeof(result.error_msg));
        return result;
    }
    body += 4;  /* Saltar los \r\n\r\n */

    /* ── Parsear JSON trivial ───────────────────────────────────── */
    /*
     * Esperamos: {"exists": true, "role": "operator"}
     *         o: {"exists": false, "role": null}
     */

    if (strstr(body, "\"exists\": true") != NULL ||
        strstr(body, "\"exists\":true") != NULL) {
        result.exists = 1;

        /* Extraer rol */
        char *role_start = strstr(body, "\"role\":");
        if (role_start != NULL) {
            role_start = strchr(role_start, ':');
            if (role_start != NULL) {
                role_start++;
                /* Buscar comilla de apertura */
                char *quote1 = strchr(role_start, '"');
                if (quote1 != NULL) {
                    quote1++;
                    char *quote2 = strchr(quote1, '"');
                    if (quote2 != NULL) {
                        size_t role_len = quote2 - quote1;
                        if (role_len >= sizeof(result.role))
                            role_len = sizeof(result.role) - 1;
                        memcpy(result.role, quote1, role_len);
                        result.role[role_len] = '\0';
                    }
                }
            }
        }
    } else {
        result.exists = 0;
    }

    result.error = 0;
    return result;
}

/*
 * auth_client.h — Cliente HTTP mínimo para consultar el microservicio
 *                 de autenticación externo.
 */

#ifndef AUTH_CLIENT_H
#define AUTH_CLIENT_H

/*
 * Resultado de una consulta de autenticación.
 */
typedef struct {
    int  exists;     /* 1 = usuario válido, 0 = no existe */
    char role[32];   /* Rol del usuario (ej: "operator") */
    int  error;      /* 1 = hubo error de red/parsing, 0 = OK */
    char error_msg[128];
} AuthResult;

/*
 * Consulta al microservicio de autenticación si el usuario existe.
 *
 * Realiza:  GET /auth?user=<username> HTTP/1.1
 * al host auth_host:auth_port configurados en g_config.
 *
 * Resuelve hostname con getaddrinfo() (compatible DNS/Docker).
 * Retorna AuthResult con el resultado de la consulta.
 */
AuthResult auth_check_user(const char *username);

#endif /* AUTH_CLIENT_H */

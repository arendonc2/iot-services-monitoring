/*
 * utils.h — Funciones auxiliares reutilizables.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/*
 * Copia segura de cadenas (nunca desborda dest).
 * Siempre termina con '\0'.
 */
void safe_strncpy(char *dest, const char *src, size_t dest_size);

/*
 * Elimina espacios en blanco al inicio y al final de str (in-place).
 * Retorna puntero al primer carácter no-blanco.
 */
char *trim(char *str);

/*
 * Genera timestamp actual en formato "YYYY-MM-DD HH:MM:SS".
 * Escribe en buf (debe tener al menos 20 bytes).
 */
void get_timestamp(char *buf, size_t buf_size);

/*
 * Lee una línea completa (hasta '\n') de un socket.
 * Retorna número de bytes leídos, 0 si conexión cerrada, -1 en error.
 * La línea queda sin el '\n' y terminada en '\0'.
 */
int recv_line(int sockfd, char *buf, size_t buf_size);

/*
 * Envía una línea terminada en '\n' por un socket.
 * Retorna 0 en éxito, -1 en error.
 */
int send_line(int sockfd, const char *message);

/*
 * Valida formato ISO 8601 básico: YYYY-MM-DDTHH:MM:SSZ
 * Retorna 1 si válido, 0 si no.
 */
int is_valid_timestamp(const char *ts);

/*
 * Convierte string a double.
 * Retorna 1 si la conversión es exitosa, 0 si no.
 * El resultado se escribe en *out.
 */
int parse_double(const char *str, double *out);

#endif /* UTILS_H */

/*
 * logger.h — Sistema de logging dual (consola + archivo), thread-safe.
 *
 * Formato de salida:
 *   [YYYY-MM-DD HH:MM:SS] [SENSOR] IP=x PORT=y RECV="..." RESP="..."
 *   [YYYY-MM-DD HH:MM:SS] [HTTP]   IP=x PORT=y REQ="..."  RESP="..."
 *   [YYYY-MM-DD HH:MM:SS] [ERROR]  IP=x PORT=y MSG="..."
 *   [YYYY-MM-DD HH:MM:SS] [INFO]   MSG="..."
 */

#ifndef LOGGER_H
#define LOGGER_H

/*
 * Inicializa el logger.
 * Abre el archivo de log en log_dir/server.log.
 * Retorna 0 en éxito, -1 en error.
 */
int logger_init(const char *log_dir);

/*
 * Cierra el archivo de log y libera recursos.
 */
void logger_close(void);

/*
 * Registra una petición/respuesta de sensor (protocolo SMP).
 */
void log_sensor(const char *ip, int port,
                const char *recv_msg, const char *resp_msg);

/*
 * Registra una petición/respuesta HTTP.
 */
void log_http(const char *ip, int port,
              const char *request, const char *response);

/*
 * Registra un error con contexto de red.
 */
void log_error(const char *ip, int port, const char *error_msg);

/*
 * Registra un mensaje informativo general (sin IP/PORT).
 */
void log_info(const char *message);

#endif /* LOGGER_H */

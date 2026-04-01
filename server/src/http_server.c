/*
 * http_server.c — Servidor HTTP básico para operadores (puerto 8080).
 *
 * Atiende requests GET con HTML inline.
 * Endpoints: / , /login?user=X , /dashboard , /sensors , /alerts , /metrics
 */

#include "http_server.h"
#include "config.h"
#include "state.h"
#include "auth_client.h"
#include "logger.h"
#include "utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ── Estructuras internas ───────────────────────────────────────── */

typedef struct {
    int  client_fd;
    char client_ip[64];
    int  client_port;
} HttpClientInfo;

/* ── CSS compartido para las páginas HTML ───────────────────────── */

static const char *CSS_STYLE =
    "<style>"
    "* { margin:0; padding:0; box-sizing:border-box; }"
    "body { font-family:'Segoe UI',Roboto,sans-serif; background:#0f0f23; "
    "       color:#e0e0e0; min-height:100vh; }"
    "nav { background:linear-gradient(135deg,#1a1a40,#2d2d6b); padding:16px 32px; "
    "      display:flex; align-items:center; gap:24px; "
    "      box-shadow:0 2px 12px rgba(0,0,0,0.4); }"
    "nav a { color:#7eb8ff; text-decoration:none; font-weight:500; "
    "        padding:8px 16px; border-radius:8px; transition:all .2s; }"
    "nav a:hover { background:rgba(126,184,255,0.15); color:#a8d4ff; }"
    "nav .brand { font-size:1.3em; font-weight:700; color:#fff; margin-right:auto; }"
    ".container { max-width:1100px; margin:32px auto; padding:0 24px; }"
    "h1 { color:#7eb8ff; margin-bottom:24px; font-size:1.8em; }"
    ".card { background:linear-gradient(145deg,#1a1a3e,#22224a); "
    "        border-radius:12px; padding:24px; margin-bottom:20px; "
    "        border:1px solid #2a2a5a; box-shadow:0 4px 16px rgba(0,0,0,0.3); }"
    ".stats { display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:16px; }"
    ".stat-card { background:linear-gradient(145deg,#1e1e4a,#2a2a5e); "
    "             border-radius:12px; padding:24px; text-align:center; "
    "             border:1px solid #3a3a6a; }"
    ".stat-card .number { font-size:2.5em; font-weight:700; color:#7eb8ff; }"
    ".stat-card .label  { font-size:0.9em; color:#888; margin-top:8px; }"
    "table { width:100%; border-collapse:collapse; margin-top:12px; }"
    "th { background:#1a1a40; color:#7eb8ff; padding:12px 16px; "
    "     text-align:left; font-weight:600; }"
    "td { padding:10px 16px; border-bottom:1px solid #2a2a5a; }"
    "tr:hover { background:rgba(126,184,255,0.05); }"
    ".badge { padding:4px 12px; border-radius:20px; font-size:0.85em; font-weight:600; }"
    ".badge-active  { background:#0a4a2a; color:#4ade80; }"
    ".badge-inactive{ background:#4a1a1a; color:#f87171; }"
    ".badge-normal  { background:#0a4a2a; color:#4ade80; }"
    ".badge-alert   { background:#4a1a1a; color:#f87171; }"
    ".login-box { max-width:400px; margin:80px auto; text-align:center; }"
    ".login-box form { margin-top:24px; }"
    ".login-box input[type=text] { padding:12px 16px; border-radius:8px; "
    "  border:1px solid #3a3a6a; background:#1a1a3e; color:#e0e0e0; "
    "  font-size:1em; width:100%; margin-bottom:16px; }"
    ".login-box button { padding:12px 32px; border-radius:8px; "
    "  border:none; background:linear-gradient(135deg,#3b82f6,#6366f1); "
    "  color:#fff; font-size:1em; cursor:pointer; font-weight:600; "
    "  transition:all .2s; }"
    ".login-box button:hover { transform:translateY(-2px); "
    "  box-shadow:0 4px 16px rgba(99,102,241,0.4); }"
    ".msg-ok    { color:#4ade80; margin:20px 0; }"
    ".msg-error { color:#f87171; margin:20px 0; }"
    "</style>";

/* ── Prototipos internos ────────────────────────────────────────── */

static void *handle_http_request(void *arg);
static void  build_nav(char *buf, size_t size);

static void  route_home(int fd, const char *ip, int port, const char *req_line);
static void  route_login(int fd, const char *ip, int port,
                         const char *req_line, const char *query);
static void  route_dashboard(int fd, const char *ip, int port, const char *req_line);
static void  route_sensors(int fd, const char *ip, int port, const char *req_line);
static void  route_alerts(int fd, const char *ip, int port, const char *req_line);
static void  route_metrics(int fd, const char *ip, int port, const char *req_line);
static void  route_not_found(int fd, const char *ip, int port, const char *req_line);

static void  send_http_response(int fd, int status_code,
                                const char *status_text,
                                const char *body, size_t body_len);
static void  http_log_and_send(int fd, const char *ip, int port,
                               const char *req_line, int status_code,
                               const char *status_text, const char *body);

/* ── Accept loop ────────────────────────────────────────────────── */

void *http_server_start(void *arg)
{
    (void)arg;

    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_info("FATAL: Cannot create HTTP server socket");
        return NULL;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(g_config.http_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "FATAL: Cannot bind HTTP server to port %d",
                 g_config.http_port);
        log_info(msg);
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        log_info("FATAL: Cannot listen on HTTP server socket");
        close(server_fd);
        return NULL;
    }

    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "HTTP server listening on port %d",
                 g_config.http_port);
        log_info(msg);
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            log_info("WARNING: accept() failed on HTTP server");
            continue;
        }

        HttpClientInfo *info = malloc(sizeof(HttpClientInfo));
        if (info == NULL) {
            log_info("ERROR: malloc failed for HttpClientInfo");
            close(client_fd);
            continue;
        }

        info->client_fd   = client_fd;
        info->client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  info->client_ip, sizeof(info->client_ip));

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_http_request, info) != 0) {
            log_info("ERROR: pthread_create failed for HTTP handler");
            close(client_fd);
            free(info);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    return NULL;
}

/* ── Request handler ────────────────────────────────────────────── */

static void *handle_http_request(void *arg)
{
    HttpClientInfo *info = (HttpClientInfo *)arg;
    int   fd   = info->client_fd;
    char *ip   = info->client_ip;
    int   port = info->client_port;

    /* Leer la primera línea de la request HTTP */
    char buf[BUFFER_SIZE];
    int  n = recv_line(fd, buf, sizeof(buf));

    if (n <= 0) {
        close(fd);
        free(info);
        return NULL;
    }

    /* Consumir el resto de los headers (no los usamos) */
    char header_line[BUFFER_SIZE];
    while (1) {
        int hn = recv_line(fd, header_line, sizeof(header_line));
        if (hn <= 0)
            break;
        if (strlen(header_line) == 0)
            break;  /* Línea vacía = fin de headers */
    }

    /* Guardar request line para logging */
    char req_line[BUFFER_SIZE];
    safe_strncpy(req_line, buf, sizeof(req_line));

    /* Parsear: METHOD PATH HTTP/1.x (strtok_r para thread-safety) */
    char *saveptr;
    char *method = strtok_r(buf, " ", &saveptr);
    char *path   = strtok_r(NULL, " ", &saveptr);
    /* char *version = strtok_r(NULL, " ", &saveptr); */

    if (method == NULL || path == NULL) {
        http_log_and_send(fd, ip, port, req_line, 400, "Bad Request",
                          "<h1>400 Bad Request</h1>");
        close(fd);
        free(info);
        return NULL;
    }

    /* Solo soportamos GET */
    if (strcmp(method, "GET") != 0) {
        http_log_and_send(fd, ip, port, req_line, 400, "Bad Request",
                          "<h1>400 Bad Request — Only GET supported</h1>");
        close(fd);
        free(info);
        return NULL;
    }

    /* Separar path y query string */
    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';
        query++;
    }

    /* Routing */
    if (strcmp(path, "/") == 0) {
        route_home(fd, ip, port, req_line);

    } else if (strcmp(path, "/login") == 0) {
        route_login(fd, ip, port, req_line, query);

    } else if (strcmp(path, "/dashboard") == 0) {
        route_dashboard(fd, ip, port, req_line);

    } else if (strcmp(path, "/sensors") == 0) {
        route_sensors(fd, ip, port, req_line);

    } else if (strcmp(path, "/alerts") == 0) {
        route_alerts(fd, ip, port, req_line);

    } else if (strcmp(path, "/metrics") == 0) {
        route_metrics(fd, ip, port, req_line);

    } else {
        route_not_found(fd, ip, port, req_line);
    }

    close(fd);
    free(info);
    return NULL;
}

/* ── Navigation bar ─────────────────────────────────────────────── */

static void build_nav(char *buf, size_t size)
{
    snprintf(buf, size,
        "<nav>"
        "<span class='brand'>&#x1F4E1; IoT Monitor</span>"
        "<a href='/'>Inicio</a>"
        "<a href='/dashboard'>Dashboard</a>"
        "<a href='/sensors'>Sensores</a>"
        "<a href='/alerts'>Alertas</a>"
        "<a href='/metrics'>M&eacute;tricas</a>"
        "</nav>");
}

/* ── GET / ──────────────────────────────────────────────────────── */

static void route_home(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<title>IoT Services Monitoring</title>"
        "%s"
        "</head><body>"
        "%s"
        "<div class='container'>"
        "<div class='login-box card'>"
        "<h1>&#x1F512; Acceso Operador</h1>"
        "<p>Ingresa tu usuario para acceder al panel de monitoreo.</p>"
        "<form action='/login' method='get'>"
        "<input type='text' name='user' placeholder='Usuario' required>"
        "<br><button type='submit'>Iniciar Sesi&oacute;n</button>"
        "</form>"
        "</div>"
        "</div>"
        "</body></html>",
        CSS_STYLE, nav);

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /login?user=X ──────────────────────────────────────────── */

static void route_login(int fd, const char *ip, int port,
                         const char *req_line, const char *query)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    /* Extraer user del query string */
    char username[MAX_ID_LEN] = "";

    if (query != NULL) {
        const char *user_param = strstr(query, "user=");
        if (user_param != NULL) {
            user_param += 5;  /* Saltar "user=" */
            safe_strncpy(username, user_param, sizeof(username));
            /* Cortar en & si hay más parámetros */
            char *amp = strchr(username, '&');
            if (amp != NULL)
                *amp = '\0';
        }
    }

    if (strlen(username) == 0) {
        char body[HTTP_RESPONSE_MAX];
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head>"
            "<meta charset='UTF-8'><title>Error</title>%s"
            "</head><body>%s"
            "<div class='container'>"
            "<div class='card'>"
            "<p class='msg-error'>&#x26A0; Debes proporcionar un usuario: "
            "/login?user=nombre</p>"
            "<a href='/'>&#x2190; Volver al inicio</a>"
            "</div></div></body></html>",
            CSS_STYLE, nav);

        http_log_and_send(fd, ip, port, req_line, 400, "Bad Request", body);
        return;
    }

    /* Consultar microservicio de autenticación */
    AuthResult auth = auth_check_user(username);

    char body[HTTP_RESPONSE_MAX];

    if (auth.error) {
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head>"
            "<meta charset='UTF-8'><title>Error de Autenticaci&oacute;n</title>%s"
            "</head><body>%s"
            "<div class='container'>"
            "<div class='card'>"
            "<h1>&#x26A0; Error del servicio de autenticaci&oacute;n</h1>"
            "<p class='msg-error'>%s</p>"
            "<p>El servidor de autenticaci&oacute;n no est&aacute; disponible. "
            "Intenta m&aacute;s tarde.</p>"
            "<a href='/'>&#x2190; Volver al inicio</a>"
            "</div></div></body></html>",
            CSS_STYLE, nav, auth.error_msg);

        http_log_and_send(fd, ip, port, req_line, 500, "Internal Server Error", body);

    } else if (!auth.exists) {
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head>"
            "<meta charset='UTF-8'><title>Acceso Denegado</title>%s"
            "</head><body>%s"
            "<div class='container'>"
            "<div class='card'>"
            "<h1>&#x1F6AB; Acceso Denegado</h1>"
            "<p class='msg-error'>El usuario <strong>%s</strong> "
            "no est&aacute; registrado en el sistema.</p>"
            "<a href='/'>&#x2190; Volver al inicio</a>"
            "</div></div></body></html>",
            CSS_STYLE, nav, username);

        http_log_and_send(fd, ip, port, req_line, 401, "Unauthorized", body);

    } else {
        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head>"
            "<meta charset='UTF-8'><title>Bienvenido</title>%s"
            "</head><body>%s"
            "<div class='container'>"
            "<div class='card'>"
            "<h1>&#x2705; Bienvenido, %s</h1>"
            "<p>Rol: <strong>%s</strong></p>"
            "<p>Accede al panel de monitoreo desde la barra de navegaci&oacute;n.</p>"
            "</div></div></body></html>",
            CSS_STYLE, nav, username, auth.role);

        http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
    }
}

/* ── GET /dashboard ─────────────────────────────────────────────── */

static void route_dashboard(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    int total     = state_get_sensor_count();
    int active    = state_get_active_sensor_count();
    int alerts    = state_get_alert_count();
    int metrics   = state_get_metric_count();

    char body[HTTP_RESPONSE_MAX];
    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>Dashboard</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<h1>&#x1F4CA; Dashboard</h1>"
        "<div class='stats'>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Sensores Registrados</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Sensores Activos</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Alertas Registradas</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>M&eacute;tricas Recibidas</div>"
        "</div>"
        "</div>"
        "</div></body></html>",
        CSS_STYLE, nav, total, active, alerts, metrics);

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /sensors ───────────────────────────────────────────────── */

static void route_sensors(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    int  offset = 0;

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>Sensores</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<h1>&#x1F4E1; Sensores Registrados</h1>"
        "<div class='card'>"
        "<table>"
        "<tr><th>ID</th><th>Tipo</th><th>Estado</th>"
        "<th>&Uacute;ltimo Valor</th><th>Timestamp</th><th>Alerta</th></tr>",
        CSS_STYLE, nav);

    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS && (size_t)offset < sizeof(body) - 512; i++) {
        if (!g_state.sensors[i].active)
            continue;

        Sensor *s = &g_state.sensors[i];

        const char *status_badge = (s->status == STATUS_ACTIVE)
            ? "<span class='badge badge-active'>ACTIVE</span>"
            : "<span class='badge badge-inactive'>INACTIVE</span>";

        const char *alert_badge = (s->alert_level == ALERT_NORMAL)
            ? "<span class='badge badge-normal'>NORMAL</span>"
            : "<span class='badge badge-alert'>ALERT</span>";

        offset += snprintf(body + offset, sizeof(body) - offset,
            "<tr><td>%s</td><td>%s</td><td>%s</td>"
            "<td>%.2f</td><td>%s</td><td>%s</td></tr>",
            s->id, sensor_type_to_str(s->type),
            status_badge, s->last_value,
            s->last_timestamp, alert_badge);
    }

    pthread_mutex_unlock(&g_state.lock);

    offset += snprintf(body + offset, sizeof(body) - offset,
        "</table></div></div></body></html>");

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /alerts ────────────────────────────────────────────────── */

static void route_alerts(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    int  offset = 0;

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>Alertas</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<h1>&#x1F6A8; Alertas</h1>"
        "<div class='card'>"
        "<table>"
        "<tr><th>Sensor</th><th>Tipo</th><th>Valor</th>"
        "<th>Umbral</th><th>Mensaje</th><th>Timestamp</th></tr>",
        CSS_STYLE, nav);

    pthread_mutex_lock(&g_state.lock);

    /* Mostrar desde la más reciente a la más antigua */
    int count = g_state.alert_count;
    for (int j = 0; j < count && (size_t)offset < sizeof(body) - 512; j++) {
        int i = (g_state.alert_next - 1 - j + MAX_ALERTS) % MAX_ALERTS;
        Alert *a = &g_state.alerts[i];

        offset += snprintf(body + offset, sizeof(body) - offset,
            "<tr><td>%s</td><td>%s</td><td>%.2f</td>"
            "<td>%.2f</td><td>%s</td><td>%s</td></tr>",
            a->sensor_id, a->type_str, a->value,
            a->threshold, a->message, a->timestamp);
    }

    pthread_mutex_unlock(&g_state.lock);

    offset += snprintf(body + offset, sizeof(body) - offset,
        "</table></div></div></body></html>");

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /metrics ───────────────────────────────────────────────── */

static void route_metrics(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    int  offset = 0;

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>M&eacute;tricas</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<h1>&#x1F4C8; &Uacute;ltimas M&eacute;tricas</h1>"
        "<div class='card'>"
        "<table>"
        "<tr><th>Sensor</th><th>Tipo</th>"
        "<th>Valor</th><th>Timestamp</th></tr>",
        CSS_STYLE, nav);

    pthread_mutex_lock(&g_state.lock);

    int count = g_state.metric_count;
    for (int j = 0; j < count && (size_t)offset < sizeof(body) - 512; j++) {
        int i = (g_state.metric_next - 1 - j + MAX_METRICS) % MAX_METRICS;
        Metric *m = &g_state.metrics[i];

        offset += snprintf(body + offset, sizeof(body) - offset,
            "<tr><td>%s</td><td>%s</td>"
            "<td>%.2f</td><td>%s</td></tr>",
            m->sensor_id, m->type_str, m->value, m->timestamp);
    }

    pthread_mutex_unlock(&g_state.lock);

    offset += snprintf(body + offset, sizeof(body) - offset,
        "</table></div></div></body></html>");

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── 404 ────────────────────────────────────────────────────────── */

static void route_not_found(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>404</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<div class='card'>"
        "<h1>&#x1F50D; 404 — P&aacute;gina no encontrada</h1>"
        "<p>La ruta solicitada no existe.</p>"
        "<a href='/'>&#x2190; Volver al inicio</a>"
        "</div></div></body></html>",
        CSS_STYLE, nav);

    http_log_and_send(fd, ip, port, req_line, 404, "Not Found", body);
}

/* ── Enviar respuesta HTTP ──────────────────────────────────────── */

static void send_http_response(int fd, int status_code,
                                const char *status_text,
                                const char *body, size_t body_len)
{
    char header[512];
    int  header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, body_len);

    send(fd, header, header_len, 0);
    if (body != NULL && body_len > 0) {
        size_t total = 0;
        while (total < body_len) {
            ssize_t n = send(fd, body + total, body_len - total, 0);
            if (n <= 0) break;
            total += n;
        }
    }
}

static void http_log_and_send(int fd, const char *ip, int port,
                               const char *req_line, int status_code,
                               const char *status_text, const char *body)
{
    size_t body_len = strlen(body);
    send_http_response(fd, status_code, status_text, body, body_len);

    char resp_summary[64];
    snprintf(resp_summary, sizeof(resp_summary), "%d %s", status_code, status_text);
    log_http(ip, port, req_line, resp_summary);
}

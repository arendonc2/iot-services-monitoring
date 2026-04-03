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
    ":root { --bg:#0b1220; --panel:#101a2d; --panel-soft:#16233a; --line:#2b3f5f;"
    "        --text:#edf4ff; --muted:#8ea4c5; --cyan:#68d5ff; --amber:#ffc857;"
    "        --good:#52d39a; --bad:#ff7c70; --accent:#7aa2ff; }"
    "* { margin:0; padding:0; box-sizing:border-box; }"
    "body { font-family:'Avenir Next','Helvetica Neue','Segoe UI',sans-serif;"
    "       color:var(--text); min-height:100vh; background:radial-gradient(circle at top left,#14304f 0,#0b1220 45%),"
    "       linear-gradient(135deg,#08111f,#0b1220 55%,#0f1a2d); }"
    "body::before { content:''; position:fixed; inset:0; pointer-events:none;"
    "       background-image:linear-gradient(rgba(104,213,255,.08) 1px,transparent 1px),"
    "       linear-gradient(90deg,rgba(104,213,255,.08) 1px,transparent 1px);"
    "       background-size:36px 36px; mask-image:linear-gradient(to bottom,rgba(0,0,0,.6),transparent 75%); }"
    "nav { position:sticky; top:0; z-index:5; display:flex; align-items:center; gap:14px;"
    "      padding:18px 28px; background:rgba(6,13,24,.82); backdrop-filter:blur(18px);"
    "      border-bottom:1px solid rgba(122,162,255,.18); }"
    "nav .brand { margin-right:auto; color:#fff; font-size:1.1rem; font-weight:800;"
    "      letter-spacing:.08em; text-transform:uppercase; }"
    "nav .brand small { display:block; font-size:.68rem; color:var(--muted); font-weight:600; }"
    "nav a { color:var(--muted); text-decoration:none; font-weight:700; font-size:.93rem;"
    "      letter-spacing:.03em; padding:10px 14px; border-radius:999px; border:1px solid transparent;"
    "      transition:all .2s ease; }"
    "nav a:hover { color:var(--text); border-color:rgba(122,162,255,.28); background:rgba(122,162,255,.1); }"
    ".container { max-width:1200px; margin:0 auto; padding:36px 24px 64px; }"
    ".hero { display:grid; grid-template-columns:1.5fr 1fr; gap:20px; margin-bottom:24px; }"
    ".hero-panel, .card, .stat-card, .info-card { background:linear-gradient(180deg,rgba(19,31,52,.96),rgba(11,20,34,.94));"
    "      border:1px solid rgba(122,162,255,.16); border-radius:22px; box-shadow:0 24px 60px rgba(0,0,0,.3); }"
    ".hero-panel { padding:28px; position:relative; overflow:hidden; }"
    ".hero-panel::after { content:''; position:absolute; inset:auto -60px -60px auto; width:180px; height:180px;"
    "      background:radial-gradient(circle,rgba(255,200,87,.22),transparent 70%); }"
    ".eyebrow { display:inline-flex; align-items:center; gap:10px; padding:8px 12px; border-radius:999px;"
    "      background:rgba(104,213,255,.1); color:var(--cyan); font-size:.78rem; font-weight:800; letter-spacing:.08em;"
    "      text-transform:uppercase; margin-bottom:18px; }"
    "h1 { font-size:clamp(2rem,4vw,3.4rem); line-height:1; margin-bottom:14px; letter-spacing:-.04em; }"
    "h2 { font-size:1.45rem; margin-bottom:10px; }"
    "p { color:var(--muted); line-height:1.6; }"
    ".hero-copy { max-width:42rem; }"
    ".hero-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:14px; margin-top:24px; }"
    ".mini-panel { padding:16px; border-radius:18px; background:rgba(255,255,255,.03); border:1px solid rgba(255,255,255,.06); }"
    ".mini-panel strong { display:block; color:var(--text); margin-bottom:4px; font-size:1.2rem; }"
    ".card { padding:24px; margin-bottom:20px; }"
    ".card-title { display:flex; align-items:flex-end; justify-content:space-between; gap:12px; margin-bottom:18px; }"
    ".card-title p { max-width:44rem; }"
    ".stats { display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:16px; margin-bottom:24px; }"
    ".stat-card { padding:22px; position:relative; overflow:hidden; }"
    ".stat-card::before { content:''; position:absolute; inset:0 auto 0 0; width:4px; background:linear-gradient(180deg,var(--cyan),var(--amber)); }"
    ".stat-card .number { font-size:2.4rem; font-weight:800; letter-spacing:-.05em; color:var(--text); }"
    ".stat-card .label { color:var(--muted); text-transform:uppercase; letter-spacing:.08em; font-size:.74rem; margin-top:8px; }"
    ".stat-card .delta { color:var(--cyan); font-size:.82rem; margin-top:10px; }"
    ".info-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(260px,1fr)); gap:18px; margin-top:18px; }"
    ".info-card { padding:20px; }"
    ".info-card ul { list-style:none; margin-top:12px; }"
    ".info-card li { color:var(--muted); padding:8px 0; border-bottom:1px solid rgba(255,255,255,.06); }"
    ".info-card li:last-child { border-bottom:none; }"
    ".table-wrap { overflow:auto; border-radius:18px; border:1px solid rgba(255,255,255,.06); }"
    "table { width:100%; border-collapse:collapse; min-width:720px; }"
    "th { padding:14px 16px; text-align:left; font-size:.78rem; letter-spacing:.08em; text-transform:uppercase;"
    "     color:var(--cyan); background:rgba(8,17,31,.88); }"
    "td { padding:14px 16px; border-top:1px solid rgba(255,255,255,.06); color:#d6e4fb; }"
    "tr:hover td { background:rgba(122,162,255,.06); }"
    ".badge { display:inline-flex; align-items:center; justify-content:center; padding:6px 12px; border-radius:999px;"
    "      font-size:.78rem; font-weight:800; letter-spacing:.05em; text-transform:uppercase; }"
    ".badge-active, .badge-normal { background:rgba(82,211,154,.14); color:var(--good); }"
    ".badge-inactive, .badge-alert { background:rgba(255,124,112,.14); color:var(--bad); }"
    ".badge-accent { background:rgba(255,200,87,.14); color:var(--amber); }"
    ".login-box { max-width:460px; margin:0 auto; text-align:left; }"
    ".login-box form { margin-top:22px; }"
    ".login-box input[type=text] { width:100%; margin-bottom:14px; padding:14px 16px; border-radius:14px;"
    "      border:1px solid rgba(122,162,255,.22); background:rgba(255,255,255,.04); color:var(--text); font-size:1rem; }"
    ".login-box button, .cta-link { display:inline-flex; align-items:center; justify-content:center; gap:8px;"
    "      padding:13px 20px; border:none; border-radius:999px; cursor:pointer; font-weight:800; font-size:.95rem;"
    "      color:#06111d; background:linear-gradient(135deg,var(--amber),#ff9d5c); text-decoration:none; box-shadow:0 12px 30px rgba(255,157,92,.22); }"
    ".login-box button:hover, .cta-link:hover { filter:brightness(1.05); }"
    ".msg-ok { color:var(--good); margin:18px 0; }"
    ".msg-error { color:var(--bad); margin:18px 0; }"
    ".empty-state { padding:28px; border:1px dashed rgba(122,162,255,.24); border-radius:18px; color:var(--muted); }"
    ".footer-note { margin-top:16px; font-size:.84rem; color:var(--muted); }"
    "@media (max-width: 820px) { .hero { grid-template-columns:1fr; } nav { flex-wrap:wrap; } .container { padding:24px 16px 48px; } }"
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
        "<span class='brand'>IoT Monitor<small>Operator console</small></span>"
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
        "<div class='hero'>"
        "<section class='hero-panel'>"
        "<div class='eyebrow'>Centro de operaciones</div>"
        "<h1>Monitoreo industrial para operadores en tiempo real</h1>"
        "<p class='hero-copy'>Supervisa el pulso de la red IoT, detecta anomal&iacute;as y accede al estado operativo de sensores desde una consola clara y enfocada en respuesta r&aacute;pida.</p>"
        "<div class='hero-grid'>"
        "<div class='mini-panel'><strong>Sensores</strong><span>Vista operativa de TEMP, VIB y POWER.</span></div>"
        "<div class='mini-panel'><strong>Alertas</strong><span>Umbrales visibles y trazabilidad de incidentes.</span></div>"
        "<div class='mini-panel'><strong>M&eacute;tricas</strong><span>Registro continuo con &uacute;ltimo valor y timestamp.</span></div>"
        "<div class='mini-panel'><strong>HTTP + TCP</strong><span>Operaci&oacute;n integrada con panel y protocolo SMP.</span></div>"
        "</div>"
        "</section>"
        "<aside class='hero-panel login-box'>"
        "<div class='eyebrow'>Acceso seguro</div>"
        "<h2>Ingresar como operador</h2>"
        "<p>Escribe tu usuario para validar acceso y abrir el panel de supervisi&oacute;n.</p>"
        "<form action='/login' method='get'>"
        "<input type='text' name='user' placeholder='Usuario operador' required>"
        "<button type='submit'>Entrar al panel</button>"
        "</form>"
        "<p class='footer-note'>Si el auth service no est&aacute; disponible, ver&aacute;s una respuesta expl&iacute;cita del sistema.</p>"
        "</aside>"
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
        "<div class='hero'>"
        "<section class='hero-panel'>"
        "<div class='eyebrow'>Dashboard operativo</div>"
        "<h1>Estado general de la instalaci&oacute;n</h1>"
        "<p class='hero-copy'>Lectura r&aacute;pida del sistema para turnos operativos: sensores registrados, actividad actual, incidentes y flujo de telemetr&iacute;a acumulado.</p>"
        "</section>"
        "<aside class='hero-panel'>"
        "<div class='eyebrow'>Prioridad</div>"
        "<h2>Foco actual</h2>"
        "<p>Usa Sensores para revisar estado individual, Alertas para incidentes y M&eacute;tricas para la trazabilidad reciente.</p>"
        "<div class='footer-note'>HTTP en 8080 y SMP en 9090 seg&uacute;n configuraci&oacute;n actual del servidor.</div>"
        "</aside>"
        "</div>"
        "<div class='stats'>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Sensores Registrados</div>"
        "  <div class='delta'>Inventario total conocido</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Sensores Activos</div>"
        "  <div class='delta'>Con sesi&oacute;n SMP abierta</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>Alertas Registradas</div>"
        "  <div class='delta'>Eventos fuera de umbral</div>"
        "</div>"
        "<div class='stat-card'>"
        "  <div class='number'>%d</div>"
        "  <div class='label'>M&eacute;tricas Recibidas</div>"
        "  <div class='delta'>Telemetr&iacute;a acumulada</div>"
        "</div>"
        "</div>"
        "<div class='info-grid'>"
        "<section class='info-card'>"
        "<h2>Lectura recomendada</h2>"
        "<ul>"
        "<li>Si alertas sube y activos cae, revisa desconexiones o QUIT de sensores.</li>"
        "<li>Valores an&oacute;malos actualizan el nivel ALERT del sensor inmediatamente.</li>"
        "<li>El dashboard resume el sistema; el detalle fino est&aacute; en las tablas.</li>"
        "</ul>"
        "</section>"
        "<section class='info-card'>"
        "<h2>Accesos r&aacute;pidos</h2>"
        "<ul>"
        "<li><a class='cta-link' href='/sensors'>Revisar sensores</a></li>"
        "<li><a class='cta-link' href='/alerts'>Abrir alertas</a></li>"
        "<li><a class='cta-link' href='/metrics'>Ver m&eacute;tricas</a></li>"
        "</ul>"
        "</section>"
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
    int  total_sensors = 0;
    int  active_sensors = 0;
    int  alert_sensors = 0;

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>Sensores</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<div class='card'>"
        "<div class='card-title'>"
        "<div><div class='eyebrow'>Inventario de campo</div><h2>Sensores registrados</h2>"
        "<p>Vista de estado, &uacute;ltimo valor reportado y nivel de alerta por dispositivo.</p></div>"
        "<span class='badge badge-accent'>Tabla operativa</span>"
        "</div>"
        "<div class='stats'>"
        "<div class='stat-card'><div class='number'>%d</div><div class='label'>Registrados</div></div>"
        "<div class='stat-card'><div class='number'>%d</div><div class='label'>Activos</div></div>"
        "<div class='stat-card'><div class='number'>%d</div><div class='label'>En alerta</div></div>"
        "</div>"
        "</div>"
        "<div class='card'>"
        "<div class='table-wrap'><table>"
        "<tr><th>ID</th><th>Tipo</th><th>Estado</th>"
        "<th>&Uacute;ltimo Valor</th><th>Timestamp</th><th>Alerta</th></tr>",
        CSS_STYLE, nav, 0, 0, 0);

    pthread_mutex_lock(&g_state.lock);

    for (int i = 0; i < MAX_SENSORS && (size_t)offset < sizeof(body) - 512; i++) {
        if (!g_state.sensors[i].active)
            continue;

        Sensor *s = &g_state.sensors[i];
        total_sensors++;
        if (s->status == STATUS_ACTIVE)
            active_sensors++;
        if (s->alert_level == ALERT_ALERT)
            alert_sensors++;

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

    {
        char stats_html[1024];
        snprintf(stats_html, sizeof(stats_html),
            "<div class='stat-card'><div class='number'>%d</div><div class='label'>Registrados</div></div>"
            "<div class='stat-card'><div class='number'>%d</div><div class='label'>Activos</div></div>"
            "<div class='stat-card'><div class='number'>%d</div><div class='label'>En alerta</div></div>",
            total_sensors, active_sensors, alert_sensors);

        char *stats_pos = strstr(body, "<div class='stat-card'><div class='number'>0</div><div class='label'>Registrados</div></div>"
                                      "<div class='stat-card'><div class='number'>0</div><div class='label'>Activos</div></div>"
                                      "<div class='stat-card'><div class='number'>0</div><div class='label'>En alerta</div></div>");
        if (stats_pos != NULL) {
            char tail[HTTP_RESPONSE_MAX];
            safe_strncpy(tail, stats_pos + strlen("<div class='stat-card'><div class='number'>0</div><div class='label'>Registrados</div></div>"
                                                   "<div class='stat-card'><div class='number'>0</div><div class='label'>Activos</div></div>"
                                                   "<div class='stat-card'><div class='number'>0</div><div class='label'>En alerta</div></div>"),
                         sizeof(tail));
            *stats_pos = '\0';
            strncat(body, stats_html, sizeof(body) - strlen(body) - 1);
            strncat(body, tail, sizeof(body) - strlen(body) - 1);
            offset = (int)strlen(body);
        }
    }

    offset += snprintf(body + offset, sizeof(body) - offset,
        "</table></div></div></div></body></html>");

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /alerts ────────────────────────────────────────────────── */

static void route_alerts(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    int  offset = 0;
    int  count_snapshot = state_get_alert_count();

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>Alertas</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<div class='card'>"
        "<div class='card-title'>"
        "<div><div class='eyebrow'>Eventos de riesgo</div><h2>Alertas registradas</h2>"
        "<p>Incidentes ordenados desde el m&aacute;s reciente, con valor observado, umbral y timestamp.</p></div>"
        "<span class='badge badge-alert'>Atenci&oacute;n prioritaria</span>"
        "</div>"
        "<div class='stats'>"
        "<div class='stat-card'><div class='number'>%d</div><div class='label'>Alertas acumuladas</div></div>"
        "<div class='stat-card'><div class='number'>70 / 50 / 10-100</div><div class='label'>Umbrales TEMP / VIB / POWER</div></div>"
        "</div>"
        "</div>"
        "<div class='card'>"
        "<div class='table-wrap'><table>"
        "<tr><th>Sensor</th><th>Tipo</th><th>Valor</th>"
        "<th>Umbral</th><th>Mensaje</th><th>Timestamp</th></tr>",
        CSS_STYLE, nav, count_snapshot);

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
        "</table></div></div></div></body></html>");

    http_log_and_send(fd, ip, port, req_line, 200, "OK", body);
}

/* ── GET /metrics ───────────────────────────────────────────────── */

static void route_metrics(int fd, const char *ip, int port, const char *req_line)
{
    char nav[1024];
    build_nav(nav, sizeof(nav));

    char body[HTTP_RESPONSE_MAX];
    int  offset = 0;
    int  count_snapshot = state_get_metric_count();

    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'><title>M&eacute;tricas</title>%s"
        "</head><body>%s"
        "<div class='container'>"
        "<div class='card'>"
        "<div class='card-title'>"
        "<div><div class='eyebrow'>Historial reciente</div><h2>&Uacute;ltimas m&eacute;tricas</h2>"
        "<p>Traza descendente del flujo de telemetr&iacute;a almacenado en memoria por el servidor.</p></div>"
        "<span class='badge badge-accent'>Buffer circular</span>"
        "</div>"
        "<div class='stats'>"
        "<div class='stat-card'><div class='number'>%d</div><div class='label'>M&eacute;tricas almacenadas</div></div>"
        "<div class='stat-card'><div class='number'>512</div><div class='label'>Capacidad m&aacute;xima del buffer</div></div>"
        "</div>"
        "</div>"
        "<div class='card'>"
        "<div class='table-wrap'><table>"
        "<tr><th>Sensor</th><th>Tipo</th>"
        "<th>Valor</th><th>Timestamp</th></tr>",
        CSS_STYLE, nav, count_snapshot);

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
        "</table></div></div></div></body></html>");

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
        "<div class='hero-panel'>"
        "<div class='eyebrow'>Ruta no disponible</div>"
        "<h1>404 &mdash; P&aacute;gina no encontrada</h1>"
        "<p>La ruta solicitada no existe dentro de la consola de operadores.</p>"
        "<p class='footer-note'><a class='cta-link' href='/'>Volver al inicio</a></p>"
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

# Checklist para Sustentación

## 1. Arquitectura y Diseño (Preguntas esperadas)

- [ ] **¿Por qué C?** → Requisito del proyecto. Demuestra manejo de bajo nivel: sockets, memoria, threads.
- [ ] **¿Por qué threads y no procesos?** → Comparten memoria (estado global), más eficientes. pthreads es estándar POSIX.
- [ ] **¿Por qué un solo mutex?** → Con <100 sensores, un mutex global evita deadlocks y es suficiente. Secciones críticas son cortas (copiar/insertar datos).
- [ ] **¿Por qué arrays fijos y no malloc dinámico?** → Memoria predecible, sin fugas, sin fragmentación. 64 sensores × sizeof(Sensor) ≈ pocos KB.
- [ ] **¿Por qué buffers circulares?** → Métricas y alertas recientes sobreescriben las antiguas. Sin crecimiento infinito de memoria.

## 2. Protocolo SMP 1.0

- [ ] Explicar que es **texto plano sobre TCP**, un mensaje por línea con `\n`.
- [ ] Demostrar con netcat: `echo "REGISTER sensor01 TEMP" | nc <host> 9090`
- [ ] Mostrar los 5 comandos: REGISTER, METRIC, STATUS, PING, QUIT.
- [ ] Explicar validación: tipo de sensor, formato de timestamp ISO 8601, valor numérico.
- [ ] Mostrar códigos de error: 400, 403, 404, 409, 422, 500.

## 3. Servidor HTTP

- [ ] Mostrar los 6 endpoints en el navegador: `/`, `/login`, `/dashboard`, `/sensors`, `/alerts`, `/metrics`.
- [ ] Demostrar login exitoso (`admin`) y fallido (`fake`).
- [ ] Mostrar que la interfaz se genera con HTML inline — sin archivos externos.
- [ ] Explicar por qué se parsea HTTP manualmente (sin librerías).

## 4. Concurrencia

- [ ] Explicar modelo thread-per-connection para sensores.
- [ ] Explicar thread-per-request para HTTP.
- [ ] Mostrar que el estado está protegido por mutex (`pthread_mutex_lock/unlock`).
- [ ] Explicar `pthread_detach` — threads auto-liberan recursos.
- [ ] Explicar `SIGPIPE SIG_IGN` — evita crash al escribir en socket cerrado.
- [ ] Explicar `strtok_r` en vez de `strtok` — thread-safety.

## 5. Autenticación

- [ ] El servidor C **no almacena usuarios** — consulta un servicio externo.
- [ ] Mostrar `auth_client.c`: abre socket, envía HTTP GET, parsea JSON.
- [ ] Mostrar que usa `getaddrinfo()` para resolver hostname (no IP fija).
- [ ] Demostrar que si el auth service se cae, el servidor C muestra error pero **no se cae**.

## 6. DNS y Configuración

- [ ] Mostrar que NO hay IPs hardcodeadas en el código.
- [ ] Todo configurable por CLI: `--auth-host`, `--auth-port`, `--sensor-port`, `--http-port`, `--log-dir`.
- [ ] En Docker Compose, `auth-service` se resuelve por nombre (Docker DNS).
- [ ] En AWS, `getaddrinfo("auth.iot-monitor.tudominio.com")` resuelve Route 53.

## 7. Docker y Despliegue

- [ ] Mostrar `Dockerfile` — compila con gcc, expone 8080+9090.
- [ ] Mostrar `docker-compose.yml` — orquesta server + auth service.
- [ ] Demostrar `docker-compose up --build -d` y que ambos containers suben.
- [ ] Mostrar que logs persisten en volumen (`./logs:/app/logs`).
- [ ] Mostrar Security Group de EC2 con puertos abiertos.

## 8. Logging

- [ ] Mostrar archivo `logs/server.log` con el formato exacto requerido.
- [ ] Mostrar que se loggea: IP, puerto, mensaje recibido, respuesta enviada.
- [ ] Mostrar que hay logging en **consola Y archivo** simultáneamente.
- [ ] Explicar thread-safety del logger (mutex interno).

## 9. Clientes Sensor

- [ ] Mostrar `sensor.py` enviando métricas reales al servidor C.
- [ ] Mostrar `sensor_launcher.py` levantando 5 sensores simultáneos.
- [ ] Mostrar `SensorClient.java` como segundo cliente en otro lenguaje.
- [ ] Verificar en `/dashboard` que los contadores suben.
- [ ] Verificar en `/alerts` que aparecen alertas cuando hay anomalías.

## 10. Demo Completa (Orden sugerido)

1. [ ] `docker-compose up --build -d` — levantar todo
2. [ ] `curl http://localhost:8080/` — mostrar página de inicio
3. [ ] `curl "http://localhost:8080/login?user=admin"` — login exitoso
4. [ ] `curl "http://localhost:8080/login?user=fake"` — login fallido (401)
5. [ ] Abrir navegador en `http://<IP>:8080/dashboard` — dashboard vacío
6. [ ] `python sensor_launcher.py --host localhost --port 9090 --max-messages 10` — lanzar sensores
7. [ ] Refrescar `/dashboard` — ver contadores crecer
8. [ ] Abrir `/sensors` — ver tabla de sensores con estado
9. [ ] Abrir `/alerts` — ver alertas generadas por anomalías
10. [ ] Abrir `/metrics` — ver métricas recientes
11. [ ] `cat logs/server.log` — mostrar logging dual
12. [ ] `docker-compose logs -f iot-server` — ver logs en vivo
13. [ ] `docker-compose down` — cierre limpio

## 11. Configuración DNS — Registro y Evidencia

### ¿Qué se usó?

**nip.io** — DNS wildcard gratuito. No se compró ni registró ningún dominio.
El servicio funciona embebiendo la IP dentro del hostname: nip.io la resuelve automáticamente.

| Hostname | Resuelve a | Uso |
|---|---|---|
| `iot-monitor.44.193.101.27.nip.io` | `44.193.101.27` | Dashboard HTTP + conexión de sensores |
| `auth.44.193.101.27.nip.io` | `44.193.101.27` | Referencia al servicio de autenticación |

### Verificación

```bash
nslookup iot-monitor.44.193.101.27.nip.io
# Resultado: 44.193.101.27
```

### Resolución interna (Docker DNS)

En `docker-compose.yml` (línea 14), el servidor C recibe `--auth-host auth-service`.
Docker Compose crea una red virtual donde el nombre del servicio (`auth-service`) se resuelve
a la IP interna del contenedor Flask. No hay configuración DNS manual.

El puerto 5000 del auth service usa `expose` (línea 24-25) en lugar de `ports`,
lo que lo hace accesible **solo dentro de la red Docker**, no desde internet.

### Dónde verlo evidenciado en el código

- [ ] **Docker DNS interno** → `docker-compose.yml` (líneas 14, 21-25): `--auth-host auth-service` + `expose: 5000`
- [ ] **getaddrinfo() en C** → `server/src/auth_client.c` (líneas 33-49): `getaddrinfo(g_config.auth_host, ...)` resuelve cualquier hostname
- [ ] **Documentación DNS** → `docs/dns.md`: estrategia completa de 3 niveles
- [ ] **nip.io en producción** → `docs/despliegue_aws.md` (líneas 37, 166-170): URL real de acceso
- [ ] **Elastic IP** → `44.193.101.27` asignada a la instancia EC2 para que los hostnames nip.io siempre apunten al mismo servidor

### Resumen: 3 niveles de DNS

```
Nivel 1: Docker DNS (interno)     → auth-service → 172.x.x.x (automático)
Nivel 2: nip.io (acceso público)  → iot-monitor.44.193.101.27.nip.io → 44.193.101.27
Nivel 3: getaddrinfo() (código C) → resuelve Docker DNS, nip.io, Route 53, /etc/hosts
```

**Clave:** El mismo binario corre en local, Docker y AWS sin cambiar código. Solo cambia `--auth-host`.


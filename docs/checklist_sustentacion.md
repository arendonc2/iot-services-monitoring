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

## 11. Preguntas Difíciles y Respuestas

| Pregunta | Respuesta |
|----------|-----------|
| ¿Qué pasa si dos sensores se registran al mismo tiempo? | El mutex serializa las escrituras. Solo uno entra a la sección crítica a la vez. |
| ¿Qué pasa si la tabla de sensores se llena? | Retorna `500 INTERNAL_ERROR`. El servidor sigue funcionando para los sensores ya registrados. |
| ¿Qué pasa si el auth service se cae? | `auth_client.c` retorna error, el HTTP server muestra "500 Internal Server Error" con mensaje descriptivo. El servidor no se cae. |
| ¿Qué pasa si un sensor envía datos sin registrarse? | Retorna `404 SENSOR_NOT_FOUND`. |
| ¿Hay race conditions? | No. Todo acceso a `g_state` está protegido por mutex. El logger tiene su propio mutex. |
| ¿Por qué no usaste `select()` o `epoll()`? | Thread-per-connection es más simple, más fácil de explicar, y suficiente para el volumen esperado. |
| ¿Hay memory leaks? | No. Los arrays son fijos (stack/global). Los únicos `malloc` son para `SensorClientInfo` y `HttpClientInfo`, ambos con `free` en el thread. |
| ¿Por qué Flask para el auth y no C? | Es un servicio **externo** separado. Usar otro lenguaje demuestra desacoplamiento de servicios, que es el objetivo. |

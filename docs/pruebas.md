# Guía de Pruebas

## 1. Pruebas del Protocolo TCP (Sensor)

### Con netcat

```bash
# Conectar al servidor
nc localhost 9090

# Enviar comandos manualmente (escribir y presionar Enter):
REGISTER sensor01 TEMP
METRIC sensor01 27.5 2026-03-30T14:05:00Z
METRIC sensor01 75.0 2026-03-30T14:06:00Z
STATUS sensor01
PING sensor01
QUIT sensor01
```

### Respuestas esperadas

```
200 REGISTERED sensor01
200 OK
200 OK
200 STATUS sensor01 ACTIVE TEMP 75.00 2026-03-30T14:06:00Z ALERT
200 PONG
200 BYE
```

### Casos de error

```bash
# Mensaje vacío
echo "" | nc localhost 9090
# → 400 BAD_REQUEST

# Tipo inválido
echo "REGISTER sensor01 GPS" | nc localhost 9090
# → 422 INVALID_SENSOR_TYPE

# Métrica sin registro previo
echo "METRIC sensorX 10.0 2026-03-30T14:05:00Z" | nc localhost 9090
# → 404 SENSOR_NOT_FOUND

# Valor no numérico
echo "METRIC sensor01 abc 2026-03-30T14:05:00Z" | nc localhost 9090
# → 422 INVALID_VALUE

# Timestamp inválido
echo "METRIC sensor01 25.0 invalid-ts" | nc localhost 9090
# → 400 BAD_REQUEST

# Sensor duplicado
echo -e "REGISTER sensor01 TEMP" | nc localhost 9090
echo -e "REGISTER sensor01 TEMP" | nc localhost 9090
# → 409 SENSOR_ALREADY_EXISTS
```

## 2. Pruebas HTTP (Operadores)

### Con curl

```bash
# Página de inicio
curl http://localhost:8080/

# Login exitoso
curl "http://localhost:8080/login?user=admin"

# Login fallido
curl "http://localhost:8080/login?user=invalido"

# Dashboard
curl http://localhost:8080/dashboard

# Tabla de sensores
curl http://localhost:8080/sensors

# Alertas
curl http://localhost:8080/alerts

# Métricas
curl http://localhost:8080/metrics

# 404
curl http://localhost:8080/inexistente

# Sin parámetro user
curl http://localhost:8080/login
```

### Verificar códigos HTTP

```bash
curl -o /dev/null -s -w "%{http_code}" http://localhost:8080/
# → 200

curl -o /dev/null -s -w "%{http_code}" "http://localhost:8080/login?user=admin"
# → 200

curl -o /dev/null -s -w "%{http_code}" "http://localhost:8080/login?user=fake"
# → 401

curl -o /dev/null -s -w "%{http_code}" http://localhost:8080/notfound
# → 404
```

## 3. Pruebas con Clientes Python

```bash
cd clients/python_sensor

# Un sensor individual
python sensor.py --host localhost --port 9090 --id sensor01 --type TEMP --max-messages 5

# Múltiples sensores simultáneos
python sensor_launcher.py --host localhost --port 9090 --max-messages 10
```

## 4. Pruebas con Cliente Java

```bash
cd clients/java_sensor/src
javac SensorClient.java
java SensorClient --host localhost --port 9090 --id sensorJ1 --type VIB --max-messages 5
```

## 5. Pruebas del Auth Service

```bash
# Probar directamente el microservicio
curl "http://localhost:5000/auth?user=admin"
# → {"exists": true, "role": "operator"}

curl "http://localhost:5000/auth?user=invalido"
# → {"exists": false, "role": null}

curl "http://localhost:5000/health"
# → {"status": "ok"}
```

## 6. Verificar Logs

```bash
# El archivo de log se genera automáticamente
cat logs/server.log

# Buscar entradas específicas
grep "SENSOR" logs/server.log
grep "HTTP" logs/server.log
grep "ERROR" logs/server.log
```

## 7. Prueba de Concurrencia

```bash
# Lanzar múltiples sensores en paralelo
python sensor_launcher.py --host localhost --port 9090 --interval 1 --max-messages 20 &

# Mientras tanto, hacer requests HTTP
curl http://localhost:8080/dashboard
curl http://localhost:8080/sensors
curl http://localhost:8080/alerts
```

## 8. Pruebas Docker Compose

```bash
# Levantar todo
docker-compose up --build -d

# Verificar contenedores
docker ps

# Probar desde el host
curl http://localhost:8080/dashboard
echo -e "REGISTER test01 TEMP\n" | nc localhost 9090

# Ver logs
docker-compose logs -f

# Detener
docker-compose down
```

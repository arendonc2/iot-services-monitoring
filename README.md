# IoT Services Monitoring

Plataforma distribuida de monitoreo IoT donde sensores simulados envían mediciones a un servidor central en C, que detecta anomalías y permite a operadores consultar el estado del sistema mediante una interfaz web.

## Arquitectura

```
Sensores (Python/Java) ──TCP:9090──► Servidor C ◄──HTTP:8080── Navegador (Operadores)
                                         │
                                         ▼
                                    Auth Service (Flask :5000)
```

- **Servidor principal**: C puro con Berkeley Sockets, pthreads, servidor HTTP artesanal.
- **Protocolo sensor**: SMP 1.0 — texto plano sobre TCP (ver `protocolo/SMP_1_0.md`).
- **Autenticación**: Microservicio externo en Python/Flask.
- **Despliegue**: Docker + Docker Compose + AWS EC2.

## Estructura del Proyecto

```
├── server/              # Servidor principal en C
│   ├── include/         # Headers (.h)
│   └── src/             # Implementaciones (.c)
├── clients/             # Clientes sensor simulados
│   ├── python_sensor/   # Cliente Python + lanzador
│   └── java_sensor/     # Cliente Java
├── auth_service/        # Microservicio de autenticación (Flask)
├── docs/                # Documentación técnica
├── protocolo/           # Especificación del protocolo SMP 1.0
├── logs/                # Archivos de log (generados en runtime)
├── Dockerfile           # Imagen Docker del servidor C
├── docker-compose.yml   # Orquestación servidor + auth
└── Makefile             # Compilación y utilidades
```

## Compilación y Ejecución Local

### Requisitos

- `gcc` con soporte para pthreads
- `python3` con `pip` (para el auth service)
- `netcat` (para pruebas manuales)

### Compilar

```bash
make all
```

### Ejecutar el auth service

```bash
cd auth_service
pip install -r requirements.txt
python auth_server.py &
cd ..
```

### Ejecutar el servidor

```bash
./server_bin
```

O con opciones:

```bash
./server_bin --sensor-port 9090 --http-port 8080 --auth-host localhost --auth-port 5000 --log-dir logs
```

### Probar

```bash
# Protocolo sensor (netcat)
echo -e "REGISTER sensor01 TEMP" | nc localhost 9090

# HTTP
curl http://localhost:8080/dashboard

# Login
curl "http://localhost:8080/login?user=admin"

# Cliente Python
cd clients/python_sensor
python sensor.py --host localhost --port 9090 --id sensor01 --type TEMP --max-messages 5

# Lanzar múltiples sensores
python sensor_launcher.py --host localhost --port 9090 --max-messages 10
```

## Ejecución con Docker Compose

```bash
# Construir y levantar
docker-compose up --build -d

# Verificar
docker ps
curl http://localhost:8080/dashboard

# Detener
docker-compose down
```

## Despliegue en AWS EC2

Ver [docs/despliegue_aws.md](docs/despliegue_aws.md) para instrucciones paso a paso.

## Documentación

- [Arquitectura del sistema](docs/arquitectura.md)
- [Protocolo SMP 1.0](protocolo/SMP_1_0.md)
- [Despliegue en AWS](docs/despliegue_aws.md)
- [Configuración DNS](docs/dns.md)
- [Guía de pruebas](docs/pruebas.md)

## Endpoints HTTP

| Ruta | Descripción |
|------|-------------|
| `GET /` | Página de inicio / login |
| `GET /login?user=X` | Autenticación contra servicio externo |
| `GET /dashboard` | Resumen: sensores, alertas, métricas |
| `GET /sensors` | Tabla de sensores registrados |
| `GET /alerts` | Tabla de alertas activas |
| `GET /metrics` | Últimas métricas recibidas |

## Usuarios de Prueba

| Usuario | Rol |
|---------|-----|
| admin | operator |
| operator1 | operator |
| operator2 | operator |
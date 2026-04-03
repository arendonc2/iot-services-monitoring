# Despliegue en AWS EC2

## Arquitectura de Despliegue

```
                    ┌── Internet ──┐
                    │              │
        Operadores (HTTP:8080)    Sensores (TCP:9090)
                    │              │
                    ▼              ▼
           ┌───────────────────────────────┐
           │   AWS EC2 (t2.micro)          │
           │   Ubuntu 22.04 LTS            │
           │   Elastic IP: 44.193.101.27   │
           │                               │
           │   ┌─── Docker Compose ──────┐ │
           │   │                         │ │
           │   │  ┌─────────────────┐    │ │
           │   │  │   iot-server    │    │ │
           │   │  │   (C — gcc)     │    │ │
           │   │  │   :8080  :9090  │────│─│──► puertos públicos
           │   │  └────────┬────────┘    │ │
           │   │           │ Docker DNS  │ │
           │   │           ▼             │ │
           │   │  ┌─────────────────┐    │ │
           │   │  │   iot-auth      │    │ │
           │   │  │   (Flask)       │    │ │
           │   │  │   :5000         │────│─│──► solo interno
           │   │  └─────────────────┘    │ │
           │   │                         │ │
           │   └─────────────────────────┘ │
           │                               │
           │   ./logs/server.log           │ ◄── volumen persistente
           └───────────────────────────────┘
                         │
                         ▼
              DNS: iot-monitor.44.193.101.27.nip.io
```

## Prerrequisitos

- Cuenta de AWS con acceso a EC2
- Par de llaves SSH (.pem) configurado
- Docker y Docker Compose instalados en la instancia

## Paso 1: Crear instancia EC2

1. Ir a **EC2 → Launch Instance**
2. Configurar:
   - **Name**: `iot-monitor-server`
   - **AMI**: Ubuntu Server 22.04 LTS (64-bit x86)
   - **Instance Type**: `t2.micro` (Free Tier eligible)
   - **Key Pair**: crear nuevo → `iot-key` → descargar `.pem`
   - **Security Group**: crear `iot-monitor-sg` con las reglas del Paso 2
   - **Storage**: 8 GB gp3 (default)
3. Launch Instance

## Paso 2: Configurar Security Group

Crear grupo `iot-monitor-sg` con las siguientes **Inbound Rules**:

| Tipo | Protocolo | Puerto | Origen | Descripción |
|------|-----------|--------|--------|-------------|
| SSH | TCP | 22 | Mi IP | Acceso SSH administración |
| Custom TCP | TCP | 8080 | 0.0.0.0/0 | HTTP operadores (dashboard) |
| Custom TCP | TCP | 9090 | 0.0.0.0/0 | TCP sensores (protocolo SMP) |

> **Nota**: El puerto 5000 del auth service NO se abre. La comunicación
> entre el servidor C y el auth service ocurre por la red interna de Docker.

## Paso 3: Asignar IP Elástica

1. Ir a **EC2 → Elastic IPs → Allocate Elastic IP address**
2. Seleccionar la IP → **Actions → Associate**
3. Asociar a la instancia `iot-monitor-server`

**IP Elástica asignada**: `44.193.101.27`

## Paso 4: Conectar por SSH

Desde Windows (PowerShell):

```powershell
# Ajustar permisos de la llave
icacls iot-key.pem /inheritance:r /grant:r "$($env:USERNAME):(R)"

# Conectar
ssh -i iot-key.pem ubuntu@44.193.101.27
```

## Paso 5: Instalar Docker y Docker Compose

```bash
# Actualizar el sistema
sudo apt update && sudo apt upgrade -y

# Instalar Docker y Docker Compose
sudo apt install -y docker.io docker-compose

# Agregar usuario al grupo docker
sudo usermod -aG docker $USER

# Cerrar sesión y volver a entrar para aplicar cambios
exit
```

Reconectar por SSH y verificar:

```bash
docker --version
docker-compose --version
```

## Paso 6: Clonar el repositorio

```bash
git clone https://github.com/arendonc2/iot-services-monitoring.git
cd iot-services-monitoring
```

## Paso 7: Construir y ejecutar con Docker Compose

```bash
# Construir imágenes y levantar servicios en segundo plano
docker-compose up --build -d

# Verificar que los contenedores están corriendo
docker ps
```

Resultado esperado:

```
NAMES        STATUS         PORTS
iot-server   Up X seconds   0.0.0.0:8080->8080/tcp, 0.0.0.0:9090->9090/tcp
iot-auth     Up X seconds   5000/tcp
```

## Paso 8: Validar el despliegue

### Desde la instancia EC2:

```bash
# HTTP
curl http://localhost:8080/
curl "http://localhost:8080/login?user=admin"

# Sensor TCP
echo -e "REGISTER testSensor TEMP" | nc localhost 9090
```

### Desde cualquier máquina externa:

```bash
# HTTP (navegador o curl)
curl http://44.193.101.27:8080/
curl http://44.193.101.27:8080/dashboard

# Login
curl "http://44.193.101.27:8080/login?user=admin"

# Sensor Python
python sensor_launcher.py --host 44.193.101.27 --port 9090 --max-messages 10
```

### Acceso por nombre de dominio (nip.io):

```bash
# Usando DNS wildcard gratuito
curl http://iot-monitor.44.193.101.27.nip.io:8080/
```

## Paso 9: Verificar comunicación inter-servicios

```bash
# Ver que el servidor C resuelve "auth-service" por Docker DNS
docker exec iot-server sh -c 'getent hosts auth-service'

# Verificar que el auth service responde internamente
docker exec iot-server sh -c 'wget -qO- http://auth-service:5000/health'
# Resultado: {"status": "ok"}
```

## Comandos útiles

```bash
# Detener servicios
docker-compose down

# Reconstruir después de cambios
docker-compose up --build -d

# Ver logs en tiempo real
docker-compose logs -f

# Logs de un solo servicio
docker logs -f iot-server
docker logs -f iot-auth

# Acceder al contenedor del servidor
docker exec -it iot-server /bin/bash

# Ver logs persistentes
cat logs/server.log
tail -f logs/server.log
```

## Costos

| Recurso | Costo (Free Tier) |
|---------|-------------------|
| t2.micro (750 hrs/mes) | $0 |
| Elastic IP (asociada) | $0 |
| Transferencia de datos | $0 (hasta 100 GB) |

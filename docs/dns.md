# Configuración DNS

## Objetivo

Usar hostnames en lugar de IPs hardcodeadas para localizar los servicios.
El sistema soporta múltiples niveles de DNS según el entorno de ejecución.

## Registros DNS utilizados

### Producción (nip.io — DNS wildcard gratuito)

Se utiliza el servicio [nip.io](https://nip.io) para resolución DNS sin necesidad de
comprar un dominio. nip.io convierte automáticamente cualquier subdomain con una IP
embebida en un hostname resoluble:

| Nombre | Resuelve a | Uso |
|--------|-----------|-----|
| `iot-monitor.44.193.101.27.nip.io` | `44.193.101.27` | Acceso al dashboard HTTP y conexión de sensores |
| `auth.44.193.101.27.nip.io` | `44.193.101.27` | Referencia al servicio de autenticación |

#### Verificar resolución

```bash
nslookup iot-monitor.44.193.101.27.nip.io
# Resultado: 44.193.101.27
```

#### Acceso por hostname

```bash
# Dashboard HTTP
curl http://iot-monitor.44.193.101.27.nip.io:8080/

# Sensores
python sensor.py --host iot-monitor.44.193.101.27.nip.io --port 9090 ...

# Navegador
# Abrir: http://iot-monitor.44.193.101.27.nip.io:8080/
```

### Docker Compose (Desarrollo/Local)

Docker Compose crea una red interna donde cada servicio se resuelve por su nombre:

```yaml
services:
  server:
    command: ["./server_bin", "--auth-host", "auth-service"]
  auth-service:
    ...
```

El servidor C usa `getaddrinfo("auth-service", ...)` y Docker DNS resuelve
automáticamente a la IP interna del contenedor Flask.

**No se necesita configuración DNS externa para desarrollo.**

### Alternativa: AWS Route 53 (Dominio propio)

Si se dispone de un dominio propio, se pueden crear registros en Route 53:

| Nombre | Tipo | Valor | TTL |
|--------|------|-------|-----|
| `iot-monitor.midominio.com` | A | IP Elástica del EC2 | 300 |
| `auth.iot-monitor.midominio.com` | CNAME | `iot-monitor.midominio.com` | 300 |

## Niveles de DNS en el Proyecto

El sistema opera con **tres niveles de resolución DNS** según el entorno:

```
┌────────────────────────────────────────────────────────┐
│  Nivel 1: Docker DNS (interno entre contenedores)      │
│  auth-service → 172.x.x.x (automático)                │
├────────────────────────────────────────────────────────┤
│  Nivel 2: nip.io (acceso externo sin dominio)          │
│  iot-monitor.44.193.101.27.nip.io → 44.193.101.27     │
├────────────────────────────────────────────────────────┤
│  Nivel 3: Route 53 (si se tiene dominio propio)        │
│  iot-monitor.midominio.com → Elastic IP                │
└────────────────────────────────────────────────────────┘
```

## Uso en el Servidor C

El servidor **nunca** tiene IPs hardcodeadas. Se configura por CLI:

```bash
# Desarrollo local
./server_bin --auth-host localhost --auth-port 5000

# Docker Compose (usa Docker DNS)
./server_bin --auth-host auth-service --auth-port 5000

# Producción AWS (usa nip.io)
./server_bin --auth-host auth.44.193.101.27.nip.io --auth-port 5000

# Producción AWS (con dominio propio)
./server_bin --auth-host auth.iot-monitor.midominio.com --auth-port 5000
```

## Uso en los Clientes Sensor

```bash
# Desarrollo local
python sensor.py --host localhost --port 9090 ...

# Producción (por IP)
python sensor.py --host 44.193.101.27 --port 9090 ...

# Producción (por hostname nip.io)
python sensor.py --host iot-monitor.44.193.101.27.nip.io --port 9090 ...

# Java
java SensorClient --host iot-monitor.44.193.101.27.nip.io --port 9090 ...
```

## Resolución DNS en C

El servidor usa `getaddrinfo()` (definido en `<netdb.h>`) para resolver
cualquier hostname, sin importar si viene de Docker DNS, nip.io, Route 53,
o `/etc/hosts`:

```c
struct addrinfo hints, *res;
memset(&hints, 0, sizeof(hints));
hints.ai_family   = AF_INET;
hints.ai_socktype = SOCK_STREAM;

// El hostname viene del argumento --auth-host (nunca hardcodeado)
int err = getaddrinfo(g_config.auth_host, port_str, &hints, &res);
// Luego: socket() + connect(sockfd, res->ai_addr, res->ai_addrlen)
```

Esta función resuelve:
- Hostnames de sistema (`/etc/hosts`)
- DNS de Docker (en redes de Compose)
- DNS público (nip.io, Route 53, Cloudflare, etc.)

## IP Elástica

Se asignó una **Elastic IP** (`44.193.101.27`) a la instancia EC2 para garantizar
que la dirección no cambie al reiniciar la instancia. Esto es necesario para que
los hostnames basados en nip.io sigan funcionando correctamente.

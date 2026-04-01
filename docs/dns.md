# Configuración DNS

## Objetivo

Usar hostnames en lugar de IPs hardcodeadas para localizar los servicios:

- `iot-monitor.tudominio.com` → Servidor principal (HTTP + TCP)
- `auth.iot-monitor.tudominio.com` → Servicio de autenticación

## Niveles de DNS

### 1. Docker Compose (Desarrollo/Local)

Docker Compose crea una red interna donde cada servicio se resuelve por su nombre:

```yaml
services:
  server:
    command: ["./server_bin", "--auth-host", "auth-service"]
  auth-service:
    ...
```

El servidor C usa `getaddrinfo("auth-service", ...)` y Docker DNS resuelve automáticamente.

**No se necesita configuración DNS externa para desarrollo.**

### 2. AWS Route 53 (Producción)

#### Configuración de zona hospedada

1. Ir a **Route 53 → Hosted Zones → Create**
2. Ingresar tu dominio (ej: `tudominio.com`)
3. Copiar los nameservers NS y configurarlos en tu registrador de dominio

#### Registros DNS

| Nombre | Tipo | Valor | TTL |
|--------|------|-------|-----|
| `iot-monitor.tudominio.com` | A | IP Elástica del EC2 | 300 |
| `auth.iot-monitor.tudominio.com` | CNAME | `iot-monitor.tudominio.com` | 300 |

Si el auth service está en el mismo EC2, usa CNAME apuntando al registro A.
Si está en otra instancia, usa otro registro A con su IP.

#### Verificar resolución

```bash
# Desde cualquier máquina
nslookup iot-monitor.tudominio.com
dig iot-monitor.tudominio.com

# Resultado esperado
iot-monitor.tudominio.com -> <IP-ELASTICA>
```

### 3. Uso en el Servidor C

El servidor **nunca** tiene IPs hardcodeadas. Se configura por CLI:

```bash
# Desarrollo local
./server_bin --auth-host localhost --auth-port 5000

# Docker Compose
./server_bin --auth-host auth-service --auth-port 5000

# Producción AWS
./server_bin --auth-host auth.iot-monitor.tudominio.com --auth-port 5000
```

### 4. Uso en los Clientes Sensor

```bash
# Desarrollo local
python sensor.py --host localhost --port 9090 ...

# Producción
python sensor.py --host iot-monitor.tudominio.com --port 9090 ...

# Java
java SensorClient --host iot-monitor.tudominio.com --port 9090 ...
```

## Resolución DNS en C

El servidor usa `getaddrinfo()` (definido en `<netdb.h>`):

```c
struct addrinfo hints, *res;
memset(&hints, 0, sizeof(hints));
hints.ai_family   = AF_INET;
hints.ai_socktype = SOCK_STREAM;

int err = getaddrinfo("auth.iot-monitor.tudominio.com", "5000", &hints, &res);
// Luego: socket() + connect(sockfd, res->ai_addr, res->ai_addrlen)
```

Esta función resuelve:
- Hostnames de sistema (`/etc/hosts`)
- DNS de Docker (en redes de Compose)
- DNS público (Route 53, Cloudflare, etc.)

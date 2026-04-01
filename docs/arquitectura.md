# Arquitectura del Sistema

## Diagrama General

```
┌─────────────────────────────────────────────────────────┐
│                  SERVIDOR PRINCIPAL (C)                  │
│                                                         │
│  ┌─────────────────┐       ┌──────────────────────┐    │
│  │  sensor_server   │       │    http_server        │    │
│  │  TCP :9090       │       │    HTTP :8080         │    │
│  │                  │       │                      │    │
│  │  ┌────────────┐  │       │  GET /               │    │
│  │  │ handler    │  │       │  GET /login?user=X   │    │
│  │  │ thread 1   │  │       │  GET /dashboard      │    │
│  │  ├────────────┤  │       │  GET /sensors        │    │
│  │  │ handler    │  │       │  GET /alerts         │    │
│  │  │ thread N   │  │       │  GET /metrics        │    │
│  │  └────────────┘  │       └──────────────────────┘    │
│  └─────────────────┘                 │                  │
│           │                          │                  │
│           ▼                          ▼                  │
│  ┌─────────────────────────────────────────────┐       │
│  │           GlobalState (mutex)                │       │
│  │  sensores[] | métricas[] | alertas[]         │       │
│  └─────────────────────────────────────────────┘       │
│           │                          │                  │
│           ▼                          ▼                  │
│  ┌──────────────┐          ┌──────────────────┐        │
│  │    alerts     │          │   auth_client     │        │
│  │  evaluación   │          │   HTTP → auth     │        │
│  └──────────────┘          └──────────────────┘        │
│                                      │                  │
│  ┌──────────────┐                    │                  │
│  │    logger     │                   │                  │
│  │  consola+file │                   │                  │
│  └──────────────┘                    │                  │
└──────────────────────────────────────│──────────────────┘
                                       │
                              ┌────────▼─────────┐
                              │  Auth Service     │
                              │  Flask :5000      │
                              │  GET /auth?user=X │
                              └──────────────────┘
```

## Módulos

| Módulo | Archivo | Responsabilidad |
|--------|---------|----------------|
| config | `config.h` | Constantes, tamaños, puertos por defecto |
| utils | `utils.h/c` | Funciones auxiliares: strings, sockets, timestamps |
| logger | `logger.h/c` | Logging dual (consola + archivo), thread-safe |
| state | `state.h/c` | Estado global con mutex: sensores, métricas, alertas |
| alerts | `alerts.h/c` | Evaluación de umbrales y registro de anomalías |
| sensor_server | `sensor_server.h/c` | Accept loop TCP para sensores, spawn de threads |
| sensor_handler | `sensor_handler.h/c` | Parsing SMP 1.0, lógica de comandos |
| http_server | `http_server.h/c` | Servidor HTTP, routing, generación HTML |
| auth_client | `auth_client.h/c` | Cliente HTTP para consultar auth service (DNS) |
| main | `main.c` | Entry point, CLI args, inicialización, threads |

## Modelo de Concurrencia

- **main()** lanza 2 threads: sensor_server y http_server.
- Cada conexión de sensor crea un **thread detached** worker.
- Cada request HTTP crea un **thread detached** que muere al responder.
- **GlobalState** protegido por un solo `pthread_mutex_t`.
- **Logger** tiene su propio mutex interno para serializar escrituras.

## Flujo de Datos

1. Sensor → TCP:9090 → sensor_handler → state (mutex) → alerts → response
2. Operador → HTTP:8080 → http_server → state (mutex) → HTML response
3. Login → http_server → auth_client → auth_service (DNS) → response

# SMP 1.0 — Sensor Monitoring Protocol

## Descripción General

Protocolo de texto plano sobre TCP, puerto **9090**, diseñado para la comunicación entre sensores IoT simulados y el servidor de monitoreo.

## Reglas Generales

- Un mensaje por línea.
- Fin de mensaje con `\n`.
- Tokens separados por espacio simple.
- Comandos en **MAYÚSCULA**.
- Texto plano ASCII.
- El sensor **debe registrarse** antes de enviar métricas.

## Comandos

### REGISTER

Registra un sensor nuevo en el servidor.

```
REGISTER <sensor_id> <sensor_type>
```

**Ejemplo:**
```
REGISTER sensor01 TEMP
```

**Respuestas:**
| Código | Mensaje | Descripción |
|--------|---------|-------------|
| 200 | `REGISTERED <sensor_id>` | Registro exitoso |
| 400 | `BAD_REQUEST` | Mensaje mal formado |
| 409 | `SENSOR_ALREADY_EXISTS` | El sensor ya está registrado |
| 422 | `INVALID_SENSOR_TYPE` | Tipo de sensor no válido |
| 500 | `INTERNAL_ERROR` | Error interno del servidor |

### METRIC

Envía una medición del sensor.

```
METRIC <sensor_id> <value> <timestamp>
```

**Ejemplo:**
```
METRIC sensor01 27.5 2026-03-30T14:05:00Z
```

**Respuestas:**
| Código | Mensaje | Descripción |
|--------|---------|-------------|
| 200 | `OK` | Métrica registrada |
| 400 | `BAD_REQUEST` | Mensaje mal formado o timestamp inválido |
| 403 | `SENSOR_NOT_REGISTERED` | Sensor inactivo |
| 404 | `SENSOR_NOT_FOUND` | Sensor no existe |
| 422 | `INVALID_VALUE` | Valor numérico inválido |
| 500 | `INTERNAL_ERROR` | Error interno |

### STATUS

Consulta el estado actual de un sensor.

```
STATUS <sensor_id>
```

**Respuesta exitosa:**
```
200 STATUS sensor01 ACTIVE TEMP 27.5 2026-03-30T14:05:00Z NORMAL
```

Campos: `sensor_id estado tipo último_valor timestamp nivel_alerta`

### PING

Verifica conectividad.

```
PING <sensor_id>
```

**Respuesta:** `200 PONG`

### QUIT

Cierra la sesión del sensor.

```
QUIT <sensor_id>
```

**Respuesta:** `200 BYE`

El servidor marca el sensor como **INACTIVE** y cierra la conexión.

## Tipos de Sensor Válidos

| Tipo | Descripción |
|------|-------------|
| `TEMP` | Temperatura |
| `VIB` | Vibración |
| `POWER` | Potencia/energía |

## Reglas de Anomalía

| Tipo | Condición de Alerta |
|------|---------------------|
| TEMP | valor > 70 |
| VIB | valor > 50 |
| POWER | valor < 10 **o** valor > 100 |

## Formato de Timestamp

ISO 8601 UTC sin fracciones de segundo:
```
YYYY-MM-DDTHH:MM:SSZ
```

Ejemplo: `2026-03-30T14:05:00Z`

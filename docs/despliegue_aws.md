# Despliegue en AWS EC2

## Prerrequisitos

- Cuenta de AWS con acceso a EC2
- Par de llaves SSH (.pem) configurado
- Dominio registrado (opcional, para DNS)

## Paso 1: Crear instancia EC2

1. Ir a **EC2 → Launch Instance**
2. Configurar:
   - **AMI**: Ubuntu 22.04 LTS (o Amazon Linux 2023)
   - **Instance Type**: `t2.micro` (suficiente para el proyecto)
   - **Key Pair**: seleccionar o crear uno nuevo
   - **Security Group**: crear uno nuevo con las reglas del Paso 2
3. Launch

## Paso 2: Configurar Security Group

Abrir los siguientes puertos **Inbound Rules**:

| Tipo | Protocolo | Puerto | Origen | Descripción |
|------|-----------|--------|--------|-------------|
| SSH | TCP | 22 | Tu IP | Acceso SSH |
| Custom TCP | TCP | 8080 | 0.0.0.0/0 | HTTP operadores |
| Custom TCP | TCP | 9090 | 0.0.0.0/0 | TCP sensores |

## Paso 3: Conectar por SSH

```bash
chmod 400 tu-llave.pem
ssh -i tu-llave.pem ubuntu@<IP-PUBLICA-EC2>
```

## Paso 4: Instalar Docker y Docker Compose

```bash
# Actualizar el sistema
sudo apt update && sudo apt upgrade -y

# Instalar Docker
sudo apt install -y docker.io docker-compose

# Agregar usuario al grupo docker
sudo usermod -aG docker $USER

# Cerrar sesión y volver a entrar para aplicar cambios
exit
ssh -i tu-llave.pem ubuntu@<IP-PUBLICA-EC2>

# Verificar
docker --version
docker-compose --version
```

## Paso 5: Clonar el repositorio

```bash
git clone https://github.com/tu-usuario/iot-services-monitoring.git
cd iot-services-monitoring
```

## Paso 6: Construir y ejecutar con Docker Compose

```bash
# Construir imágenes y levantar servicios
docker-compose up --build -d

# Verificar que los contenedores están corriendo
docker ps

# Ver logs del servidor
docker logs -f iot-server

# Ver logs del auth service
docker logs -f iot-auth
```

## Paso 7: Validar acceso externo

Desde tu máquina local:

```bash
# Probar HTTP
curl http://<IP-PUBLICA-EC2>:8080/
curl http://<IP-PUBLICA-EC2>:8080/dashboard

# Probar login
curl "http://<IP-PUBLICA-EC2>:8080/login?user=admin"

# Probar sensor TCP
echo -e "REGISTER sensor01 TEMP\n" | nc <IP-PUBLICA-EC2> 9090

# Probar con el cliente Python
cd clients/python_sensor
python sensor.py --host <IP-PUBLICA-EC2> --port 9090 --id sensor01 --type TEMP --max-messages 5
```

## Paso 8: Comandos útiles

```bash
# Detener servicios
docker-compose down

# Reconstruir después de cambios
docker-compose up --build -d

# Ver logs en tiempo real
docker-compose logs -f

# Eliminar todo (imágenes + volúmenes)
docker-compose down --rmi all -v

# Acceder al contenedor
docker exec -it iot-server /bin/bash
```

## IP Elástica (Recomendado)

Para tener una IP fija que no cambie al reiniciar la instancia:

1. Ir a **EC2 → Elastic IPs → Allocate**
2. Asociar la IP elástica a tu instancia
3. Usar esa IP para DNS y para conectar sensores

## Costos

- `t2.micro`: gratuita con Free Tier (primeros 12 meses)
- IP Elástica: gratuita mientras esté asociada a una instancia en ejecución
- Transferencia de datos: cobro mínimo

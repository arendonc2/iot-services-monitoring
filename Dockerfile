FROM gcc:latest

WORKDIR /app

# Copiar código fuente del servidor
COPY server/ server/
COPY Makefile .

# Compilar
RUN make all

# Crear directorio de logs
RUN mkdir -p /app/logs

# Exponer puertos
EXPOSE 8080 9090

# Volumen para logs persistentes
VOLUME ["/app/logs"]

# Ejecutar el servidor
CMD ["./server_bin", "--log-dir", "/app/logs"]

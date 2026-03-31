import argparse
import random
import socket
import sys
import time
from dataclasses import dataclass
from typing import Optional


VALID_SENSOR_TYPES = {"TEMP", "VIB", "POWER"}


@dataclass
class SensorConfig:
    host: str
    port: int
    sensor_id: str
    sensor_type: str
    interval: float = 3.0
    timeout: float = 5.0
    anomaly_chance: float = 0.15


class SensorClient:
    def __init__(self, config: SensorConfig) -> None:
        if config.sensor_type not in VALID_SENSOR_TYPES:
            raise ValueError(
                f"Tipo de sensor inválido: {config.sensor_type}. "
                f"Usa uno de {sorted(VALID_SENSOR_TYPES)}"
            )

        self.config = config
        self.sock: Optional[socket.socket] = None
        self.running = False

    def connect(self) -> None:
        """Abre la conexión TCP con el servidor."""
        self.sock = socket.create_connection(
            (self.config.host, self.config.port),
            timeout=self.config.timeout,
        )
        self.running = True
        print(
            f"[INFO] Conectado a {self.config.host}:{self.config.port} "
            f"como {self.config.sensor_id} ({self.config.sensor_type})"
        )

    def close(self) -> None:
        """Cierra la conexión ordenadamente."""
        if not self.sock:
            return

        try:
            self.send_quit()
        except Exception as exc:
            print(f"[WARN] No se pudo enviar QUIT: {exc}")

        try:
            self.sock.close()
        except Exception:
            pass

        self.sock = None
        self.running = False
        print(f"[INFO] Conexión cerrada para {self.config.sensor_id}")

    def _ensure_connected(self) -> None:
        if self.sock is None:
            raise ConnectionError("El sensor no está conectado al servidor.")

    def _send_line(self, message: str) -> None:
        """Envía una línea terminada en \\n según el protocolo."""
        self._ensure_connected()
        payload = f"{message}\n".encode("utf-8")
        self.sock.sendall(payload)
        print(f"[SEND] {message}")

    def _recv_line(self) -> str:
        """Lee una línea completa terminada en \\n."""
        self._ensure_connected()
        chunks = []

        while True:
            data = self.sock.recv(1)
            if not data:
                raise ConnectionError("El servidor cerró la conexión.")
            if data == b"\n":
                break
            chunks.append(data)

        response = b"".join(chunks).decode("utf-8").strip()
        print(f"[RECV] {response}")
        return response

    def register(self) -> str:
        command = f"REGISTER {self.config.sensor_id} {self.config.sensor_type}"
        self._send_line(command)
        return self._recv_line()

    def send_metric(self, value: float, timestamp: str) -> str:
        command = f"METRIC {self.config.sensor_id} {value:.2f} {timestamp}"
        self._send_line(command)
        return self._recv_line()

    def send_ping(self) -> str:
        command = f"PING {self.config.sensor_id}"
        self._send_line(command)
        return self._recv_line()

    def send_status(self) -> str:
        command = f"STATUS {self.config.sensor_id}"
        self._send_line(command)
        return self._recv_line()

    def send_quit(self) -> str:
        command = f"QUIT {self.config.sensor_id}"
        self._send_line(command)
        return self._recv_line()

    def generate_value(self) -> float:
        """
        Genera un valor según el tipo de sensor.
        Incluye una pequeña probabilidad de anormalidad para probar alertas.
        """
        anomaly = random.random() < self.config.anomaly_chance

        if self.config.sensor_type == "TEMP":
            return random.uniform(71, 95) if anomaly else random.uniform(20, 65)

        if self.config.sensor_type == "VIB":
            return random.uniform(51, 90) if anomaly else random.uniform(5, 45)

        if self.config.sensor_type == "POWER":
            if anomaly:
                return random.choice([
                    random.uniform(0, 9),
                    random.uniform(101, 130),
                ])
            return random.uniform(20, 90)

        raise ValueError(f"Tipo no soportado: {self.config.sensor_type}")

    @staticmethod
    def utc_timestamp() -> str:
        """Devuelve timestamp en formato ISO 8601 UTC."""
        return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    def run(self, max_messages: Optional[int] = None) -> None:
        """
        Flujo principal:
        - conectar
        - registrar
        - enviar métricas periódicas
        """
        sent_count = 0

        try:
            self.connect()

            register_response = self.register()
            if not register_response.startswith("200"):
                raise RuntimeError(
                    f"Registro rechazado por el servidor: {register_response}"
                )

            while self.running:
                value = self.generate_value()
                timestamp = self.utc_timestamp()

                response = self.send_metric(value, timestamp)
                if not response.startswith("200"):
                    print(f"[WARN] Respuesta inesperada al enviar métrica: {response}")

                sent_count += 1
                if max_messages is not None and sent_count >= max_messages:
                    break

                time.sleep(self.config.interval)

        except KeyboardInterrupt:
            print("\n[INFO] Interrupción manual del usuario.")
        except Exception as exc:
            print(f"[ERROR] {exc}")
        finally:
            self.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Cliente sensor TCP para el proyecto IoT Services Monitoring."
    )
    parser.add_argument("--host", required=True, help="Hostname o IP del servidor")
    parser.add_argument("--port", type=int, required=True, help="Puerto TCP del servidor")
    parser.add_argument("--id", required=True, dest="sensor_id", help="ID único del sensor")
    parser.add_argument(
        "--type",
        required=True,
        dest="sensor_type",
        choices=sorted(VALID_SENSOR_TYPES),
        help="Tipo del sensor",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=3.0,
        help="Segundos entre métricas",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Timeout del socket en segundos",
    )
    parser.add_argument(
        "--anomaly-chance",
        type=float,
        default=0.15,
        help="Probabilidad de generar un valor anómalo",
    )
    parser.add_argument(
        "--max-messages",
        type=int,
        default=None,
        help="Cantidad máxima de métricas a enviar antes de cerrar",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    config = SensorConfig(
        host=args.host,
        port=args.port,
        sensor_id=args.sensor_id,
        sensor_type=args.sensor_type,
        interval=args.interval,
        timeout=args.timeout,
        anomaly_chance=args.anomaly_chance,
    )

    client = SensorClient(config)
    client.run(max_messages=args.max_messages)
    return 0


if __name__ == "__main__":
    sys.exit(main())
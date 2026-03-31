import argparse
import re
import socket
import threading
from dataclasses import dataclass

VALID_SENSOR_TYPES = {"TEMP", "VIB", "POWER"}
ISO_8601_UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")


@dataclass
class ClientState:
    registered: bool = False
    sensor_id: str | None = None
    sensor_type: str | None = None


def recv_line(conn: socket.socket) -> str | None:
    chunks = []

    while True:
        data = conn.recv(1)
        if not data:
            return None
        if data == b"\n":
            break
        chunks.append(data)

    return b"".join(chunks).decode("utf-8", errors="replace").strip()


def send_line(conn: socket.socket, response: str) -> None:
    conn.sendall(f"{response}\n".encode("utf-8"))


def handle_client(conn: socket.socket, addr: tuple[str, int]) -> None:
    print(f"[MOCK] Cliente conectado: {addr[0]}:{addr[1]}")
    state = ClientState()

    try:
        while True:
            line = recv_line(conn)
            if line is None:
                break

            print(f"[MOCK][RECV] {line}")
            parts = line.split()

            if not parts:
                send_line(conn, "400 BAD_REQUEST")
                print("[MOCK][SEND] 400 BAD_REQUEST")
                continue

            command = parts[0]

            if command == "REGISTER":
                if len(parts) != 3:
                    response = "400 BAD_REQUEST"
                else:
                    sensor_id, sensor_type = parts[1], parts[2]
                    if sensor_type not in VALID_SENSOR_TYPES:
                        response = "400 BAD_REQUEST"
                    elif state.registered:
                        response = "409 SENSOR_ALREADY_EXISTS"
                    else:
                        state.registered = True
                        state.sensor_id = sensor_id
                        state.sensor_type = sensor_type
                        response = f"200 REGISTERED {sensor_id}"

            elif command == "METRIC":
                if len(parts) != 4:
                    response = "400 BAD_REQUEST"
                elif not state.registered:
                    response = "403 SENSOR_NOT_REGISTERED"
                else:
                    sensor_id, value_str, timestamp = parts[1], parts[2], parts[3]

                    if sensor_id != state.sensor_id:
                        response = "404 SENSOR_NOT_FOUND"
                    else:
                        try:
                            float(value_str)
                        except ValueError:
                            response = "422 INVALID_VALUE"
                        else:
                            response = (
                                "400 BAD_REQUEST"
                                if not ISO_8601_UTC_RE.match(timestamp)
                                else "200 OK"
                            )

            elif command == "PING":
                response = "400 BAD_REQUEST" if len(parts) != 2 else "200 PONG"

            elif command == "STATUS":
                response = "400 BAD_REQUEST" if len(parts) != 2 else "200 OK"

            elif command == "QUIT":
                if len(parts) != 2:
                    response = "400 BAD_REQUEST"
                else:
                    response = "200 BYE"
                    send_line(conn, response)
                    print(f"[MOCK][SEND] {response}")
                    break

            else:
                response = "400 BAD_REQUEST"

            send_line(conn, response)
            print(f"[MOCK][SEND] {response}")

    except ConnectionError:
        pass
    finally:
        conn.close()
        print(f"[MOCK] Cliente desconectado: {addr[0]}:{addr[1]}")


def serve(host: str, port: int) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen()

        print(f"[MOCK] Escuchando en {host}:{port}")

        while True:
            conn, addr = server.accept()
            thread = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
            thread.start()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Servidor mock TCP para pruebas de sensores")
    parser.add_argument("--host", default="127.0.0.1", help="Host de escucha")
    parser.add_argument("--port", type=int, default=5050, help="Puerto de escucha")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    serve(args.host, args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

import argparse
import subprocess
import sys
import time
from pathlib import Path


DEFAULT_SENSORS = [
    {"id": "sensor01", "type": "TEMP"},
    {"id": "sensor02", "type": "TEMP"},
    {"id": "sensor03", "type": "VIB"},
    {"id": "sensor04", "type": "VIB"},
    {"id": "sensor05", "type": "POWER"},
]


def build_command(
    python_executable: str,
    sensor_script: Path,
    host: str,
    port: int,
    sensor_id: str,
    sensor_type: str,
    interval: float,
    anomaly_chance: float,
    max_messages: int | None,
) -> list[str]:
    command = [
        python_executable,
        str(sensor_script),
        "--host",
        host,
        "--port",
        str(port),
        "--id",
        sensor_id,
        "--type",
        sensor_type,
        "--interval",
        str(interval),
        "--anomaly-chance",
        str(anomaly_chance),
    ]

    if max_messages is not None:
        command.extend(["--max-messages", str(max_messages)])

    return command


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Lanzador de múltiples sensores simulados."
    )
    parser.add_argument("--host", required=True, help="Hostname o IP del servidor")
    parser.add_argument("--port", type=int, required=True, help="Puerto TCP del servidor")
    parser.add_argument(
        "--interval",
        type=float,
        default=3.0,
        help="Segundos entre métricas para todos los sensores",
    )
    parser.add_argument(
        "--anomaly-chance",
        type=float,
        default=0.15,
        help="Probabilidad de valor anómalo para todos los sensores",
    )
    parser.add_argument(
        "--max-messages",
        type=int,
        default=None,
        help="Cantidad máxima de métricas por sensor",
    )
    parser.add_argument(
        "--stagger",
        type=float,
        default=0.5,
        help="Segundos de espera entre el arranque de cada sensor",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    launcher_dir = Path(__file__).resolve().parent
    sensor_script = launcher_dir / "sensor.py"

    if not sensor_script.exists():
        print(f"[ERROR] No se encontró sensor.py en {sensor_script}")
        return 1

    processes: list[subprocess.Popen] = []

    try:
        for sensor in DEFAULT_SENSORS:
            cmd = build_command(
                python_executable=sys.executable,
                sensor_script=sensor_script,
                host=args.host,
                port=args.port,
                sensor_id=sensor["id"],
                sensor_type=sensor["type"],
                interval=args.interval,
                anomaly_chance=args.anomaly_chance,
                max_messages=args.max_messages,
            )

            print(
                f"[INFO] Lanzando {sensor['id']} ({sensor['type']}) "
                f"contra {args.host}:{args.port}"
            )

            process = subprocess.Popen(cmd)
            processes.append(process)

            time.sleep(args.stagger)

        print(f"[INFO] {len(processes)} sensores en ejecución.")

        while True:
            all_finished = True

            for process in processes:
                if process.poll() is None:
                    all_finished = False

            if all_finished:
                print("[INFO] Todos los sensores finalizaron.")
                break

            time.sleep(1)

    except KeyboardInterrupt:
        print("\n[INFO] Interrupción manual. Cerrando sensores...")
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()

        time.sleep(1)

        for process in processes:
            if process.poll() is None:
                process.kill()

        print("[INFO] Lanzador finalizado.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
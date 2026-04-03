"""
auth_server.py — Microservicio de autenticación para IoT Services Monitoring.

Expone un endpoint GET /auth?user=<username> que retorna si el usuario
existe y su rol en formato JSON.

Usuarios mock:
  - admin     (operator)
  - operator1 (operator)
  - operator2 (operator)
"""

import os

from flask import Flask, request, jsonify

app = Flask(__name__)

USERS = {
    "admin":     {"role": "operator"},
    "operator1": {"role": "operator"},
    "operator2": {"role": "operator"},
}


@app.route("/auth")
def auth():
    user = request.args.get("user", "").strip()

    if not user:
        return jsonify({"error": "Missing 'user' parameter"}), 400

    if user in USERS:
        return jsonify({"exists": True, "role": USERS[user]["role"]})

    return jsonify({"exists": False, "role": None})


@app.route("/health")
def health():
    return jsonify({"status": "ok"})


if __name__ == "__main__":
    port = int(os.getenv("AUTH_PORT", "5000"))
    app.run(host="0.0.0.0", port=port, debug=False)

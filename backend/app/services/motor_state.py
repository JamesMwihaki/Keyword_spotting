"""
Motor State Service.

Thread-safe in-memory store tracking the last known position of each motor.
Updated by both voice commands (websocket.py) and manual commands (dashboard.py).

State resets on process restart — physical motor position is always authoritative.

Author: James Karui
"""

from threading import Lock

MOTOR_COUNT = 6
MOTOR_NAMES = [
    "Green Screen",
    "Blue Screen",
    "White Screen",
    "Black Screen",
    "Grey Screen",
    "Rose Screen",
]

_state = {
    i: {"position": "UNKNOWN", "last_direction": None, "last_status": None}
    for i in range(MOTOR_COUNT)
}
_lock = Lock()


def update_motor(motor_id: int, direction: str, status: str) -> None:
    """
    Update tracked position after a motor command completes.

    Args:
        motor_id:  Motor index (0-5)
        direction: "UP" or "DOWN"
        status:    "OK" | "LIMIT_TOP" | "LIMIT_BOTTOM" | "ERROR"
    """
    if not (0 <= motor_id < MOTOR_COUNT):
        return
    with _lock:
        s = _state[motor_id]
        s["last_direction"] = direction
        s["last_status"] = status
        if status == "LIMIT_TOP":
            s["position"] = "TOP"
        elif status == "LIMIT_BOTTOM":
            s["position"] = "BOTTOM"
        elif status == "OK":
            s["position"] = "TOP" if direction == "UP" else "BOTTOM"


def get_all_states() -> dict:
    """Return a snapshot of all motor states, safe to serialise as JSON."""
    with _lock:
        return {
            i: {**_state[i], "name": MOTOR_NAMES[i]}
            for i in range(MOTOR_COUNT)
        }

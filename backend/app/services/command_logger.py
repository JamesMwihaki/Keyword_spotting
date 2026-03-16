"""
Logger.

Appends one JSON record per command to audio_commands/command_log.jsonl.
Uses newline-delimited JSON (crash-safe, append-only — no risk of corrupting
the file if the process dies mid-write).

Author: James Karui
"""

import json
import os
import logging
from datetime import datetime, timezone
from typing import Optional

logger = logging.getLogger(__name__)

LOG_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
    "audio_commands"
)
LOG_FILE = os.path.join(LOG_DIR, "command_log.jsonl")


def log_command(
    audio_filename: str,
    gemini_outcome: str,
    motor_id: Optional[int] = None,
    direction: Optional[str] = None,
    motor_status: Optional[str] = None,
    text_response: Optional[str] = None,
) -> None:
    """
    Append one record to command_log.jsonl. Never raises.

    Args:
        audio_filename: Path or filename of the saved WAV command
        gemini_outcome: "success" | "no_command" | "api_error"
        motor_id:       Motor that was commanded (None if no command)
        direction:      "UP" | "DOWN" (None if no command)
        motor_status:   "OK" | "LIMIT_TOP" | "LIMIT_BOTTOM" | "ERROR" (None if no command)
        text_response:  Gemini text reply when no command was found
    """
    record = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "audio_file": os.path.basename(audio_filename),
        "gemini_outcome": gemini_outcome,
        "motor_id": motor_id,
        "direction": direction,
        "motor_status": motor_status,
        "gemini_text": text_response,
    }
    try:
        os.makedirs(LOG_DIR, exist_ok=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps(record) + "\n")
    except Exception as e:
        logger.error(f"command_logger: failed to write record: {e}")

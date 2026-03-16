"""
WebSocket API.

This module handles WebSocket connections from ESP32 clients, processes
audio streams through Gemini AI, and controls backdrop motors via voice commands.

Key Features:
- Audio streaming and buffering from ESP32 microphone
- Retry logic with 3 attempts for misunderstood commands
- Real-time audio responses using ElevenLabs or gTTS

Author: James Karui + claude
"""

from fastapi import APIRouter, WebSocket, WebSocketDisconnect, Query, status
from app.config import settings
from app.services.gemini_service import GeminiManager
from app.services.hardware import send_command_to_esp32
from app.services.command_logger import log_command
from app.services.motor_state import update_motor
from app.utils.audio_tools import create_wav_header
from app.utils.audio_utils import text_to_pcm
from datetime import datetime
import os
import logging
import asyncio
from typing import List

AUDIO_COMMANDS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
    "audio_commands"
)
os.makedirs(AUDIO_COMMANDS_DIR, exist_ok=True)

logger = logging.getLogger(__name__)
router = APIRouter()

# Backdrop motor name mapping (motor_id -> display name)
BLIND_NAMES = ["Green Screen", "Blue Screen", "White Screen", "Black Screen", "Grey Screen", "Rose Screen"]


class ConnectionManager:
    """
    Manages WebSocket connections for real-time audio communication.
    
    Handles multiple simultaneous connections and broadcasts audio
    responses to all connected clients (ESP32 speaker units).
    """
    
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        """Accept and register a new WebSocket connection."""
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"Client connected. Total: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        """Remove a WebSocket from active connections."""
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info(f"Client disconnected. Total: {len(self.active_connections)}")

    async def broadcast_bytes(self, data: bytes):
        """
        Broadcast audio data to all connected clients in chunks.

        Uses 1KB chunks to stay within ESP32 WebSocket frame buffer limits
        and includes small delays to allow network stack to process.
        """
        CHUNK_SIZE = 1024
        disconnected = []

        for i in range(0, len(data), CHUNK_SIZE):
            chunk = data[i:i + CHUNK_SIZE]
            for connection in list(self.active_connections):
                try:
                    await connection.send_bytes(chunk)
                except Exception:
                    if connection not in disconnected:
                        disconnected.append(connection)
            await asyncio.sleep(0.01)  # Yield to event loop

        for conn in disconnected:
            self.disconnect(conn)


manager = ConnectionManager()


@router.websocket("/ws/audio")
async def websocket_endpoint(websocket: WebSocket, token: str = Query(None)):
    """
    Main WebSocket endpoint for ESP32 audio communication.
    
    Flow:
    1. Authenticate connection using token
    2. Receive audio stream from ESP32 microphone
    3. Process audio through Gemini AI
    4. Execute motor commands and send audio responses
    5. Handle retry logic for misunderstood commands
    
    Events sent to ESP32:
    - {"event": "retry_listening"} - Command not understood, retry without wake word
    - {"event": "command_fulfilled"} - Command executed, return to wake word listening
    """
    # Token authentication
    if settings.WEBSOCKET_TOKEN and token != settings.WEBSOCKET_TOKEN:
        logger.warning(f"Auth failed. Token mismatch.")
        await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
        return

    await manager.connect(websocket)

    # Initialize Gemini chat session for this connection
    gemini = GeminiManager()
    audio_buffer = bytearray()
    retry_count = 0
    MAX_RETRIES = 3
    
    try:
        while True:
            try:
                message = await websocket.receive()
            except RuntimeError:
                logger.info("WebSocket disconnected")
                manager.disconnect(websocket)
                break
            
            # Handle incoming audio bytes
            if "bytes" in message:
                audio_buffer.extend(message["bytes"])
                # Memory protection: clear if buffer exceeds ~60 seconds
                if len(audio_buffer) > 2000000:
                    logger.warning("Audio buffer overflow, clearing")
                    audio_buffer = bytearray()
            
            # Handle text messages (commands from ESP32)
            elif "text" in message:
                text_data = message["text"]
                
                if "end_of_audio" in text_data:
                    if len(audio_buffer) > 0:
                        # Process audio and get result
                        outcome = await process_and_respond(
                            websocket, audio_buffer, gemini, retry_count, MAX_RETRIES
                        )
                        audio_buffer = bytearray()

                        # Only burn a retry on genuine "didn't understand" —
                        # not on API timeouts/errors (ESP32 self-recovers via RESPONSE_TIMEOUT)
                        if outcome == "success":
                            retry_count = 0
                        elif outcome == "no_command":
                            retry_count += 1
                            if retry_count >= MAX_RETRIES:
                                retry_count = 0
                        # outcome == "api_error": leave retry_count unchanged

    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
        manager.disconnect(websocket)


async def process_and_respond(
    websocket: WebSocket,
    audio_data: bytearray,
    gemini: GeminiManager,
    retry_count: int = 0,
    max_retries: int = 3,
    send_audio: bool = True
) -> str:
    """
    Process audio command through Gemini AI and respond.

    Args:
        websocket: Active WebSocket connection
        audio_data: Raw PCM audio bytes from ESP32
        gemini: Gemini AI manager instance
        retry_count: Current retry attempt number
        max_retries: Maximum retry attempts before reset
        send_audio: Whether to send audio response

    Returns:
        "success"   — command found and executed
        "no_command" — Gemini responded but found nothing actionable (counts as retry)
        "api_error"  — timeout or exception, do not count against retries
    """
    wav_data = create_wav_header(audio_data)
    client_ip = websocket.client.host

    # Save the audio command sent to Gemini
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    wav_path = os.path.join(AUDIO_COMMANDS_DIR, f"command_{timestamp}.wav")
    with open(wav_path, "wb") as f:
        f.write(wav_data)

    # Process through Gemini AI with timeout protection
    try:
        commands, text_response = await asyncio.wait_for(
            gemini.process_audio(wav_data), timeout=9.0
        )
    except asyncio.TimeoutError:
        logger.error("Gemini API timeout — not counting as retry")
        log_command(wav_path, "api_error")
        return "api_error"
    except Exception as e:
        logger.error(f"Gemini error: {e}")
        log_command(wav_path, "api_error")
        return "api_error"

    # No commands detected - handle retry logic
    if not commands:
        await _handle_no_command(
            websocket, text_response, retry_count, max_retries, send_audio
        )
        log_command(wav_path, "no_command", text_response=text_response)
        return "no_command"

    # Execute motor commands — await so audio is sent BEFORE command_fulfilled.
    # This ensures the ESP32 is in STATE_PLAYBACK when it receives command_fulfilled,
    # not STATE_AWAITING_RESPONSE where a race with RESPONSE_TIMEOUT can drop the audio.
    for cmd in commands:
        await _execute_motor_command(cmd, client_ip, send_audio, wav_path)

    # Signal command completion to ESP32 (after audio has been sent)
    try:
        await websocket.send_text('{"event": "command_fulfilled"}')
    except Exception as e:
        logger.warning(f"Failed to send command_fulfilled: {e}")
    
    return "success"


async def _handle_no_command(
    websocket: WebSocket,
    text_response: str,
    retry_count: int,
    max_retries: int,
    send_audio: bool
) -> bool:
    """Handle case when no valid command was detected in audio."""
    is_final_retry = (retry_count >= max_retries - 1)
    
    if is_final_retry:
        # Max retries reached - reset to wake word listening
        resp_text = "I couldn't process your command. Resetting the system. Please say Hey Dawn or Hello Dawn to issue a new command."
    else:
        # Allow retry without wake word
        resp_text = text_response if text_response else "I didn't understand that, please repeat the command."

    # Send audio BEFORE the event signal so ESP32 is still listening when audio arrives
    if send_audio:
        pcm_audio = text_to_pcm(resp_text)
        if pcm_audio:
            try:
                await manager.broadcast_bytes(pcm_audio)
            except Exception as e:
                logger.warning(f"Audio broadcast failed: {e}")

    # Signal the ESP32 after audio has been sent
    if is_final_retry:
        try:
            await websocket.send_text('{"event": "command_fulfilled"}')
        except Exception:
            pass
    else:
        try:
            await websocket.send_text('{"event": "retry_listening"}')
        except Exception:
            pass
    
    return False


async def _execute_motor_command(
    cmd: dict,
    client_ip: str,
    send_audio: bool,
    wav_path: str = ""
):
    """Execute a single motor command and send audio feedback."""
    motor_id = cmd["motor_id"]
    direction = cmd["direction"]
    blind_name = BLIND_NAMES[motor_id] if 0 <= motor_id < len(BLIND_NAMES) else f"Motor {motor_id}"

    # Send command first so the response reflects the actual motor outcome
    result = await send_command_to_esp32(motor_id, direction, ip=client_ip)
    status_msg = result.get("status", "ERROR")

    update_motor(motor_id, direction, status_msg)

    if status_msg == "LIMIT_TOP":
        response_text = f"The {blind_name} is already up."
    elif status_msg == "LIMIT_BOTTOM":
        response_text = f"The {blind_name} is already down."
    elif status_msg == "OK":
        action = "Raising" if direction == "UP" else "Dropping"
        response_text = f"{action} the {blind_name}."
    else:
        response_text = f"I had trouble controlling the {blind_name}."

    log_command(wav_path, "success", motor_id=motor_id, direction=direction, motor_status=status_msg, text_response=response_text)

    if send_audio:
        pcm_audio = text_to_pcm(response_text)
        if pcm_audio:
            await manager.broadcast_bytes(pcm_audio)

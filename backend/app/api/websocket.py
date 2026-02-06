
from fastapi import APIRouter, WebSocket, WebSocketDisconnect, Query, status
from app.config import settings
from app.services.gemini_service import GeminiManager
from app.utils.audio_tools import create_wav_header
from datetime import datetime
import os
import logging
import asyncio

logger = logging.getLogger(__name__)
router = APIRouter()



@router.websocket("/ws/audio")
async def websocket_endpoint(websocket: WebSocket, token: str = Query(None)):
    if settings.WEBSOCKET_TOKEN and token != settings.WEBSOCKET_TOKEN:
        logger.warning(f"Auth failed. Got {token}, expected {settings.WEBSOCKET_TOKEN}")
        await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
        return

    await websocket.accept()
    logger.info("WebSocket connection accepted")

    gemini = GeminiManager() # New chat session per connection

    audio_buffer = bytearray()
    
    try:
        while True:
            data = await websocket.receive_bytes()
            audio_buffer.extend(data)
            
            # Process every ~2.5 seconds of audio (16000 * 2 * 2.5 = 80000 bytes)
            if len(audio_buffer) > 80000:
                await process_and_respond(websocket, audio_buffer, gemini)
                audio_buffer = bytearray()

    except WebSocketDisconnect:
        logger.info("Client disconnected")
        # Ensure buffer is cleared effectively by logic flow, 
        # but here we just process what remains if sensible, or discard.
        if len(audio_buffer) > 0:
             logger.info(f"Client disconnected with {len(audio_buffer)} bytes remaining. Processing...")
             try:
                 await process_and_respond(websocket, audio_buffer, gemini)
             except Exception as e:
                logger.error(f"Error processing final buffer: {e}")
        audio_buffer = bytearray() # Cleanup

async def process_and_respond(websocket: WebSocket, audio_data: bytearray, gemini: GeminiManager):
    logger.info(f"Processing {len(audio_data)} bytes of audio")
    wav_data = create_wav_header(audio_data)

    # Save audio
    try:
        os.makedirs("audio_commands", exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"audio_commands/command_{timestamp}.wav"
        with open(filename, "wb") as f:
            f.write(wav_data)
        logger.info(f"Saved audio command to {filename}")
    except Exception as e:
        logger.error(f"Failed to save audio file: {e}")

    try:
        # TIMEOUT PROTECTION: 10 seconds
        commands = await asyncio.wait_for(gemini.process_audio(wav_data), timeout=9.0)
    except asyncio.TimeoutError:
        logger.error("Gemini API timed out")
        try:
            await websocket.send_json({"error": "timeout"})
        except Exception:
            pass # Client might be gone
        return
    except Exception as e:
        logger.error(f"Gemini Error: {e}")
        return

    
    for cmd in commands:
        try:
            await websocket.send_json(cmd)
            logger.info(f"Sent command: {cmd}")
        except Exception as e:
             logger.warning(f"Failed to send command: {e}")

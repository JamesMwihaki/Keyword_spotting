"""
Gemini AI Service for Voice Command Processing.

This module handles communication with Google's Gemini AI API
to process voice commands and trigger backdrop motor functions.

Author: James Karui
"""

import logging
import google.generativeai as genai
from app.config import settings

logger = logging.getLogger(__name__)


class GeminiManager:
    """
    Manages Gemini AI interactions for voice command processing.
    
    Uses Gemini 2.0 Flash model with function calling to detect
    backdrop control commands from audio input.
    
    Supported Commands:
    - "Raise/Lower the [color] screen"
    - "Drop/Lift the [color] backdrop"
    - Natural variations of above commands
    
    Motor IDs: 0=Green, 1=Blue, 2=White, 3=Black, 4=Grey, 5=Rose
    """
    
    def __init__(self):
        """Initialize Gemini model with function calling tools."""
        if not settings.GEMINI_API_KEY:
            logger.error("GEMINI_API_KEY not configured")
        else:
            genai.configure(api_key=settings.GEMINI_API_KEY)
        
        self.tools_list = [self.control_backdrop_tool_def]
        self.model = genai.GenerativeModel(
            model_name='gemini-2.0-flash',
            tools=self.tools_list
        )
        # Manual function calling for custom handling
        self.chat = self.model.start_chat(enable_automatic_function_calling=False)

    @staticmethod
    def control_backdrop_tool_def(motor_id: int, direction: str):
        """
        Tool definition for Gemini function calling.
        
        This function signature is exposed to Gemini AI for automatic
        detection and parsing of voice commands.
        
        Args:
            motor_id: Motor to control (0-5)
                      0=Green, 1=Blue, 2=White, 3=Black, 4=Grey, 5=Rose
            direction: Movement direction - "UP", "DOWN", or "STOP"
        
        Returns:
            True (placeholder - actual execution handled separately)
        """
        return True

    async def process_audio(self, wav_data: bytes) -> tuple:
        """
        Process audio through Gemini AI and extract commands.
        
        Sends audio to Gemini with a prompt requesting function calls
        for detected backdrop commands.
        
        Args:
            wav_data: WAV format audio bytes
            
        Returns:
            Tuple of (commands, text_response):
            - commands: List of dicts with motor_id and direction
            - text_response: Text if no command detected, None otherwise
        """
        prompt = (
            "Listen to this audio command and control the backdrop motors. "
            "If no command is detected, reply with 'I didn't understand that, "
            "please repeat the command'. If a command is detected, DO NOT "
            "generate text, just call the function."
        )
        
        try:
            response = await self.chat.send_message_async(
                [prompt, {"mime_type": "audio/wav", "data": wav_data}]
            )
            
            commands = []
            text_response = None
            
            if response.parts:
                for part in response.parts:
                    # Extract function calls
                    if fn := part.function_call:
                        if fn.name in ("control_backdrop_tool_def", "control_backdrop"):
                            commands.append({
                                "motor_id": int(fn.args["motor_id"]),
                                "direction": fn.args["direction"]
                            })
                    # Extract text response
                    if part.text:
                        text_response = part.text
            
            # Clear text if commands found (avoid double responses)
            if commands:
                text_response = None

            return commands, text_response

        except Exception as e:
            logger.error(f"Gemini API error: {e}")
            return [], "System Error"

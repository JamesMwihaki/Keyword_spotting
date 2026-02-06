
import os
import logging
import google.generativeai as genai
from google.generativeai.types import content_types
from app.config import settings

logger = logging.getLogger(__name__)

class GeminiManager:
    def __init__(self):
        if not settings.GEMINI_API_KEY:
            logger.error("GEMINI_API_KEY not found in settings")
        else:
            genai.configure(api_key=settings.GEMINI_API_KEY)
        
        self.tools_list = [self.control_backdrop_tool_def]
        self.model = genai.GenerativeModel(
            model_name='gemini-2.0-flash',
            tools=self.tools_list
        )
        # We handle Function Calling manually to send to client
        self.chat = self.model.start_chat(enable_automatic_function_calling=False)

    @staticmethod
    def control_backdrop_tool_def(motor_id: int, direction: str):
        """
        Controls a specific backdrop motor.
        
        Args:
            motor_id: The ID of the motor to control.
                      0 (Green), 1 (Blue), 2 (White), 3 (Black), 4 (Grey), 5 (Rose).
            direction: The direction to move the motor.
                       "UP", "DOWN", or "STOP".
        """
        return True

    async def process_audio(self, wav_data: bytes):
        """
        Sends audio to Gemini and returns any function calls found.
        """
        prompt = "Listen to this audio command and control the backdrop motors. If no command is detected, DO NOTHING"
        
        try:
            response = await self.chat.send_message_async(
                [prompt, {"mime_type": "audio/wav", "data": wav_data}]
            )
            
            commands = []
            
            if response.parts:
                for part in response.parts:
                    logger.info(f"Gemini Response Part: {part}")
                    if fn := part.function_call:
                        logger.info(f"Function call: {fn.name}({fn.args})")
                        if fn.name == "control_backdrop_tool_def" or fn.name == "control_backdrop": # Handle both potentially
                             commands.append({
                                "motor_id": int(fn.args["motor_id"]),
                                "direction": fn.args["direction"]
                            })
                    if part.text:
                         logger.info(f"Gemini Text: {part.text}")
            
            return commands

        except Exception as e:
            logger.error(f"Error calling Gemini: {e}")
            return []

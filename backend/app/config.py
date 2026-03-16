"""
Dawn Voice Control System - Configuration.

This module uses Pydantic Settings to manage application configuration
from environment variables and .env file.

Author: James Karui
"""

from pydantic_settings import BaseSettings
from pydantic import field_validator
from typing import Optional


class Settings(BaseSettings):
    """
    Application settings loaded from environment variables.

    Required (must be set in .env — app will refuse to start if missing):
        ADMIN_USER: Dashboard login username
        ADMIN_PASS: Dashboard login password
        SECRET_KEY: Session signing key (min 32 characters)

    Optional:
        GEMINI_API_KEY: Google Gemini API key for voice processing
        ESP32_IP: IP address of ESP32 motor controller
        WEBSOCKET_TOKEN: Authentication token for WebSocket connections
        ELEVENLABS_API_KEY: ElevenLabs API key for voice cloning
        ELEVENLABS_VOICE_ID: ElevenLabs cloned voice ID
    """
    # Core Configuration
    GEMINI_API_KEY: Optional[str] = None
    ESP32_IP: str = "192.168.1.148"
    WEBSOCKET_TOKEN: Optional[str] = None

    # Authentication — no defaults; app refuses to start if these are missing
    ADMIN_USER: str
    ADMIN_PASS: str
    SECRET_KEY: str

    # Remote Access (ngrok)
    NGROK_AUTHTOKEN: Optional[str] = None
    NGROK_DOMAIN: Optional[str] = None

    # Voice Cloning (ElevenLabs)
    ELEVENLABS_API_KEY: Optional[str] = None
    ELEVENLABS_VOICE_ID: Optional[str] = None

    @field_validator("SECRET_KEY")
    @classmethod
    def secret_key_min_length(cls, v: str) -> str:
        if len(v) < 32:
            raise ValueError("SECRET_KEY must be at least 32 characters long")
        return v

    class Config:
        env_file = ".env"
        env_file_encoding = "utf-8"


settings = Settings()

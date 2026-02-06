
from pydantic_settings import BaseSettings
from typing import Optional

class Settings(BaseSettings):
    GEMINI_API_KEY: Optional[str] = None
    ADMIN_USER: str = "admin"
    ADMIN_PASS: str = "password"
    ESP32_IP: str = "192.168.1.148"
    WEBSOCKET_TOKEN: Optional[str] = None
    NGROK_AUTHTOKEN: Optional[str] = None
    NGROK_DOMAIN: Optional[str] = None
    SECRET_KEY: str = "super-secret-random-key"

    class Config:
        env_file = ".env"
        env_file_encoding = "utf-8"

settings = Settings()


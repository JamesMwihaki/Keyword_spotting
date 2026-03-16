"""
Hardware Communication Service for ESP32 Motor Control.

This module handles HTTP communication with the ESP32 motor controller
to execute backdrop movement commands.

Author: James Karui
"""

import httpx
import logging
from app.config import settings

logger = logging.getLogger(__name__)


async def send_command_to_esp32(motor_id: int, direction: str, ip: str = None) -> dict:
    """
    Send motor control command to ESP32 via HTTP GET request.
    
    The ESP32 motor controller exposes a /move endpoint that accepts
    motor ID and direction parameters.
    
    Args:
        motor_id: Motor to control (0-5)
        direction: Movement direction - "UP" or "DOWN"
        ip: Optional ESP32 IP (defaults to ESP32_IP from settings)
        
    Returns:
        Dict with:
        - success: Boolean indicating request success
        - status: Response text or error message
                  "OK" - Command executed
                  "LIMIT_TOP" - Motor at upper limit
                  "LIMIT_BOTTOM" - Motor at lower limit
                  Other values indicate errors
    
    Example:
        result = await send_command_to_esp32(0, "DOWN")
        # Sends: GET http://192.168.1.148/move?id=0&dir=DOWN
    """
    target_ip = ip if ip else settings.ESP32_IP
    
    if not target_ip:
        logger.error("No ESP32 IP configured")
        return {"success": False, "status": "No IP"}
        
    url = f"http://{target_ip}/move"
    params = {"id": motor_id, "dir": direction}
    
    try:
        async with httpx.AsyncClient() as client:
            resp = await client.get(url, params=params, timeout=2.0)
            
        if resp.status_code == 200:
            return {"success": True, "status": resp.text}
        else:
            return {"success": False, "status": "ESP Error"}
            
    except httpx.TimeoutException:
        logger.error(f"ESP32 timeout: {url}")
        return {"success": False, "status": "Timeout"}
    except Exception as e:
        logger.error(f"ESP32 connection failed: {e}")
        return {"success": False, "status": "Connection Failed"}

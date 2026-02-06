
import httpx
import logging
from app.config import settings

logger = logging.getLogger(__name__)

async def send_command_to_esp32(motor_id: int, direction: str):
    """
    Proxies command to ESP32 using async HTTP client.
    """
    url = f"http://{settings.ESP32_IP}/move"
    params = {"id": motor_id, "dir": direction}
    
    logger.info(f"Proxying command to: {url} with params {params}")
    
    try:
        async with httpx.AsyncClient() as client:
            resp = await client.get(url, params=params, timeout=2.0)
            
        if resp.status_code == 200:
            return {"success": True, "status": resp.text}
        else:
            return {"success": False, "status": "ESP Error"}
            
    except Exception as e:
        logger.error(f"Proxy failed: {e}")
        return {"success": False, "status": "Connection Failed"}

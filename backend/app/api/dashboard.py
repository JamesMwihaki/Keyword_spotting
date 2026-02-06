
from fastapi import APIRouter, Depends, status, Request
from fastapi.responses import HTMLResponse, RedirectResponse, JSONResponse
from fastapi.templating import Jinja2Templates
from app.api.auth import get_current_user
from app.services.hardware import send_command_to_esp32

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")

@router.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request, user: str = Depends(get_current_user)):
    if not user:
        return RedirectResponse(url="/")
    return templates.TemplateResponse("dashboard.html", {"request": request})

@router.post("/control")
async def control_proxy(id: int, dir: str, user: str = Depends(get_current_user)):
    if not user:
        return JSONResponse({"success": False, "error": "Unauthorized"}, status_code=401)
    
    result = await send_command_to_esp32(id, dir)
    return JSONResponse(result)

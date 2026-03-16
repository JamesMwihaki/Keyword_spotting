
from fastapi import APIRouter, Depends, status, Request
from fastapi.responses import HTMLResponse, RedirectResponse, JSONResponse
from fastapi.templating import Jinja2Templates
from app.api.auth import get_current_user
from app.services.hardware import send_command_to_esp32
from app.services.motor_state import update_motor, get_all_states

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")

@router.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request, user: str = Depends(get_current_user)):
    if not user:
        return RedirectResponse(url="/")
    return templates.TemplateResponse("dashboard.html", {"request": request})

VALID_MOTOR_IDS = set(range(6))
VALID_DIRECTIONS = {"UP", "DOWN", "STOP"}

@router.post("/control")
async def control_proxy(id: int, dir: str, user: str = Depends(get_current_user)):
    if not user:
        return JSONResponse({"success": False, "error": "Unauthorized"}, status_code=401)

    if id not in VALID_MOTOR_IDS:
        return JSONResponse(
            {"success": False, "error": f"Invalid motor ID {id}. Must be 0-5."},
            status_code=422
        )

    dir_upper = dir.upper()
    if dir_upper not in VALID_DIRECTIONS:
        return JSONResponse(
            {"success": False, "error": f"Invalid direction '{dir}'. Must be UP, DOWN, or STOP."},
            status_code=422
        )

    result = await send_command_to_esp32(id, dir_upper)
    update_motor(id, dir_upper, result.get("status", "ERROR"))
    return JSONResponse(result)


@router.get("/motor-states")
async def motor_states(user: str = Depends(get_current_user)):
    if not user:
        return JSONResponse({"success": False, "error": "Unauthorized"}, status_code=401)
    return JSONResponse(get_all_states())

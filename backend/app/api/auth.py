
from fastapi import APIRouter, Depends, Form, Request, status
from fastapi.responses import RedirectResponse
from fastapi.templating import Jinja2Templates
from app.config import settings

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")

def get_current_user(request: Request):
    user = request.session.get("user")
    if not user:
        return None
    return user

@router.get("/", response_class=RedirectResponse)
async def login_page(request: Request):
    if get_current_user(request):
        return RedirectResponse(url="/dashboard")
    return templates.TemplateResponse("login.html", {"request": request})

@router.post("/login")
async def login(request: Request, username: str = Form(...), password: str = Form(...)):
    if username == settings.ADMIN_USER and password == settings.ADMIN_PASS:
        request.session["user"] = username
        return RedirectResponse(url="/dashboard", status_code=status.HTTP_303_SEE_OTHER)
    return templates.TemplateResponse("login.html", {"request": request, "error": "Invalid Credentials"})

@router.get("/logout")
async def logout(request: Request):
    request.session.clear()
    return RedirectResponse(url="/")

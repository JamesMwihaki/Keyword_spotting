
from fastapi import FastAPI
from starlette.middleware.sessions import SessionMiddleware
from fastapi.staticfiles import StaticFiles
from app.config import settings
from app.api import auth, dashboard, websocket

app = FastAPI()

# SECURITY: Add Session Middleware. 
app.add_middleware(SessionMiddleware, secret_key=settings.SECRET_KEY)

# Mount routes
app.include_router(auth.router)
app.include_router(dashboard.router)
app.include_router(websocket.router)

@app.get("/health")
async def health_check():
    return {"status": "ok"}

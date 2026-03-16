"""
Dawn Voice Control System - FastAPI Application Entry Point.

This is the main entry point for the Dawn backdrop control system backend.
It initializes the FastAPI application and mounts all routers.

Author: James Karui
"""

from fastapi import FastAPI
from starlette.middleware.sessions import SessionMiddleware
from app.config import settings
from app.api import auth, dashboard, websocket

app = FastAPI(
    title="Dawn Voice Control System",
    description="Voice-controlled backdrop system using Gemini AI",
    version="1.0.0"
)

# Session middleware for secure authentication
app.add_middleware(SessionMiddleware, secret_key=settings.SECRET_KEY)

# Mount API routers
app.include_router(auth.router)       # Login/logout endpoints
app.include_router(dashboard.router)  # Web dashboard
app.include_router(websocket.router)  # Audio WebSocket


@app.get("/health")
async def health_check():
    """Health check endpoint for monitoring."""
    return {"status": "ok"}

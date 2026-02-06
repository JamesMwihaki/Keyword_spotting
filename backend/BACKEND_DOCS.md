
# Backend Architecture Documentation

This document explains the Python backend for the Curtain Control System. The backend is built using **FastAPI** and is designed to be modular, asynchronous, and resilient.

## 📂 Directory Structure

The backend code is located in the `backend/` directory.

```text
backend/
├── app/
│   ├── api/                 # API Route Handlers
│   │   ├── auth.py          # Login & Session Management
│   │   ├── dashboard.py     # Web Dashboard & Proxy Logic
│   │   └── websocket.py     # Real-time Audio Streaming
│   │
│   ├── services/            # Business Logic
│   │   ├── gemini_service.py # Google Gemini AI Integration
│   │   └── hardware.py      # ESP32 Communication Proxy
│   │
│   ├── utils/
│   │   └── audio_tools.py   # WAV Header & Audio Helpers
│   │
│   ├── templates/           # HTML/Jinja2 Templates (UI)
│   ├── config.py            # Application Configuration
│   └── main.py              # Application Entry Point
│
├── requirements.txt         # Python Dependencies
└── run_remote.py            # Development Server Runner
```

## 🧩 Key Components

### 1. Application Entry (`app/main.py`)
This is the heart of the application. It:
- Initializes the `FastAPI` app.
- Sets up **Session Middleware** for secure login.
- Includes all the routers from `app/api/`.

### 2. Configuration (`app/config.py`)
We use `pydantic-settings` to manage configuration. This reads environment variables from `.env` (like `GEMINI_API_KEY`, `ESP32_IP`).
- **Benefit**: It validates that all required secrets are present when the app starts.

### 3. Services Layer (`app/services/`)
This layer handles the "heavy lifting" so the API routes stay clean.
- **`gemini_service.py`**: Manages the connection to Google Gemini. It handles the `process_audio` function, ensuring timeouts are handled and function calls (like `control_backdrop`) are parsed correctly.
- **`hardware.py`**: A clean interface to send commands to the ESP32. It uses `httpx` for **asynchronous** HTTP requests, meaning the server doesn't freeze while waiting for the ESP32 to reply.

### 4. WebSocket Bridge (`app/api/websocket.py`)
This is where the magic happens for Voice Control.
- **Endpoint**: `/ws/audio`
- **Flow**:
    1.  Receives raw audio bytes from ESP32.
    2.  Buffers audio until a chunks is ready (or stream ends).
    3.  Wraps audio in a WAV header (`app/utils/audio_tools.py`).
    4.  Sends audio to `GeminiService`.
    5.  Receives a **Command** (JSON) from Gemini.
    6.  Sends the command back to ESP32.
- **Resilience**: It includes a 10-second timeout. If Gemini is too slow, it returns an error so the ESP32 doesn't hang forever.

## 🔄 Data Flow

### Voice Command Flow
1.  **User** says "Dawn... [Command]".
2.  **ESP32** detects wake word and connects to WebSocket.
3.  **ESP32** streams audio data to Backend.
4.  **Backend** (`websocket.py`) accumulates data.
5.  **Backend** sends audio to **Gemini API**.
6.  **Gemini** analyzes audio and determines the intent (e.g., "Move Green Up").
7.  **Gemini** returns a function call `control_backdrop(motor_id=0, direction="UP")`.
8.  **Backend** returns this JSON to **ESP32**.
9.  **ESP32** receives JSON and moves the motor.

### Dashboard Control Flow
1.  **User** clicks "Up" on the Dashboard.
2.  **Browser** sends POST request to `/control`.
3.  **Backend** (`dashboard.py`) verifies the user is logged in.
4.  **Backend** (`hardware.py`) sends HTTP GET request to `http://<ESP32_IP>/move`.
5.  **ESP32** moves motor.

## 🚀 How to Run

1.  **Install Dependencies**:
    ```bash
    pip install -r backend/requirements.txt
    ```
2.  **Start Server**:
    ```bash
    python backend/run_remote.py
    ```
    This will start the server on `0.0.0.0:8000` and also launch an **ngrok** tunnel for remote access.

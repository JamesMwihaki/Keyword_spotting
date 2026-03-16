# Dawn — Voice-Controlled Backdrop System

A voice-activated motor control system for a studio. Say **"Hey Dawn"** and give a natural language command to raise or lower any of 6 fabric backdrop screens. The ESP32 handles wake word detection locally using TinyML; everything else is processed by a FastAPI backend powered by Google Gemini AI.

---

## How It Works

```
"Hey Dawn, lower the green screen"
        │
        ▼
ESP32 (TinyML wake word detection)
        │  streams 3s of audio via WebSocket (Might think about using a silence detection algorithm)
        ▼
FastAPI Backend
        │  sends WAV to Gemini 2.0 Flash (function calling)
        ▼
Motor command → HTTP → ESP32 motor controller
        │
        ▼
TTS response (ElevenLabs / gTTS) → played back through speaker
```

---

## Features

- **Edge Wake Word Detection**: TinyML model on the ESP32 (via Edge Impulse) listens for the wake phrase locally — no cloud round-trip until triggered.
- **Natural Language Commands**: Google Gemini 2.0 Flash interprets free-form voice commands, no rigid phrasing required.
- **6 Backdrop Motors**: Independent stepper motor control (green, blue, white, black, grey, rose screens) via TMC2209 drivers.
- **Voice Feedback**: Spoken responses via ElevenLabs voice cloning or gTTS fallback, played through an I2S speaker.
- **Web Dashboard**: Manual motor control UI with real-time position sync, accessible via browser.
- **Retry Logic**: Up to 3 attempts if a command isn't understood. API errors don't count against retries.
- **Command Logging**: All commands logged to JSONL for auditing and debugging.

---

## Architecture

### Hardware (ESP32)

| Component | Part | Interface |
| :--- | :--- | :--- |
| Microcontroller | ESP32 | — |
| Microphone | INMP441 MEMS | I2S |
| Motor Drivers | TMC2209 (×6) | Step/Dir/Enable |
| Speaker | I2S DAC | I2S |
| Status LEDs | 5× individual | GPIO |

### Backend (FastAPI)

```
backend/app/
├── main.py                  # App entry point, router mounting
├── config.py                # Pydantic Settings, env validation
├── api/
│   ├── auth.py              # Session-based login/logout
│   ├── dashboard.py         # Manual motor control UI
│   └── websocket.py         # Audio streaming + Gemini processing
├── services/
│   ├── gemini_service.py    # Gemini 2.0 Flash with function calling
│   ├── hardware.py          # HTTP commands to ESP32
│   ├── motor_state.py       # Thread-safe motor position tracking
│   └── command_logger.py    # JSONL audit logging
└── utils/
    ├── audio_tools.py       # WAV header generation
    └── audio_utils.py       # TTS (ElevenLabs / gTTS)
```

### ESP32 State Machine

| State | LED | Description |
| :--- | :--- | :--- |
| Listening | Green | Running TinyML wake word detection |
| Streaming | Blue | Sending audio to backend |
| Awaiting Response | Red | Waiting for Gemini result |
| Playback | Yellow | Playing TTS response |

---

## Machine Learning Model

Built with **Edge Impulse**.

- **Wake Phrase**: "Hey Dawn"
- **Training Data**: Human voices including accent-specific samples (Kenyan English)
- **DSP Block**: MFE (Mel Frequency Energy)
- **Inference Engine**: EON™ Compiler, deployed as Arduino library

---

## Setup

### Requirements

**Python dependencies** (`backend/requirements.txt`):
- FastAPI, Uvicorn
- google-generativeai
- websockets, httpx
- pydantic-settings
- gtts, pydub
- pyngrok

**Arduino libraries**:
- ESP32 Arduino Core
- WebsocketsClient
- ArduinoJson
- Edge Impulse inferencing library (JamesMwihaki-project-1)

### Environment Variables

Create `backend/.env`:

```
ADMIN_USER=your_username
ADMIN_PASS=your_password
SECRET_KEY=<32+ character random string>
GEMINI_API_KEY=...
ESP32_IP=192.168.x.x
ELEVENLABS_API_KEY=...          # optional, falls back to gTTS
ELEVENLABS_VOICE_ID=...         # optional
NGROK_AUTHTOKEN=...             # optional, for remote access
NGROK_DOMAIN=...                # optional
```

### Firmware

Flash the `Main/` directory to your ESP32 using Arduino IDE. WiFi credentials and the WebSocket token go in `Main/Secrets.h` (gitignored — create locally):

```cpp
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"
#define SECRET_KEY "your_websocket_token"
```

---

## Challenges
- We need to make sure we don't loose track of the current status of the screens, even when the esp32 shuts off


---

## License

This project is open-source under the [MIT License](LICENSE).

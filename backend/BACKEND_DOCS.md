# Dawn Voice Control System - Backend Documentation

Voice-controlled backdrop system using ESP32 and Google Gemini AI.

## Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Configure .env file (see below)
# Start server with ngrok tunnel
python run_remote.py
```

## Configuration (.env)

```env
# Required
GEMINI_API_KEY=your_gemini_api_key

# ESP32 Motor Controller
ESP32_IP=192.168.1.148

# WebSocket Auth
WEBSOCKET_TOKEN=my_secure_token_123

# Remote Access (optional)
NGROK_AUTHTOKEN=your_ngrok_token

# Voice Cloning (optional - uses gTTS if not set)
ELEVENLABS_API_KEY=your_elevenlabs_key
ELEVENLABS_VOICE_ID=your_cloned_voice_id
```

## Directory Structure

```
backend/
├── app/
│   ├── api/
│   │   ├── auth.py           # Login/logout
│   │   ├── dashboard.py      # Web dashboard
│   │   └── websocket.py      # Audio streaming
│   ├── services/
│   │   ├── gemini_service.py # Gemini AI integration
│   │   └── hardware.py       # ESP32 communication
│   ├── utils/
│   │   ├── audio_tools.py    # WAV header generation
│   │   └── audio_utils.py    # Text-to-speech (ElevenLabs/gTTS)
│   ├── templates/            # HTML templates
│   ├── config.py             # Settings
│   └── main.py               # FastAPI app
├── .env                      # Environment variables
├── requirements.txt          # Dependencies
└── run_remote.py             # Dev server launcher
```

## Voice Command Flow

1. **Wake Word**: ESP32 detects "Dawn" using TinyML
2. **Audio Stream**: ESP32 streams audio to backend via WebSocket
3. **AI Processing**: Backend sends audio to Gemini AI
4. **Command Extraction**: Gemini returns function call (motor_id, direction)
5. **Motor Control**: Backend sends HTTP request to ESP32 motor controller
6. **Audio Response**: Backend generates TTS and streams back to ESP32

## WebSocket Events

### ESP32 → Backend
- Audio bytes (PCM 16kHz mono)
- `{"end_of_audio": true}` - Signals end of recording

### Backend → ESP32
- Audio bytes (PCM 16kHz stereo)
- `{"event": "retry_listening"}` - Retry without wake word
- `{"event": "command_fulfilled"}` - Return to wake word listening

## Motor IDs

| ID | Screen |
|----|--------|
| 0  | Green  |
| 1  | Blue   |
| 2  | White  |
| 3  | Black  |
| 4  | Grey   |
| 5  | Rose   |

## Text-to-Speech

The system supports two TTS providers:

1. **ElevenLabs** (Recommended): High-quality voice cloning
   - Configure `ELEVENLABS_API_KEY` and `ELEVENLABS_VOICE_ID`
   - Clone your voice at https://elevenlabs.io

2. **Google TTS** (Fallback): Free, decent quality
   - Used automatically if ElevenLabs not configured

## Retry Logic

If Gemini doesn't understand a command:
1. First 2 attempts: "I didn't understand, please repeat"
2. Third attempt: "Resetting system, say Hey Dawn to start"
3. Returns to wake word listening

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/ws/audio` | WS | Audio streaming |
| `/health` | GET | Health check |
| `/login` | POST | User login |
| `/logout` | GET | User logout |
| `/` | GET | Dashboard |
| `/control` | POST | Motor control |

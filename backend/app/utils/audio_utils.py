"""
Audio Utilities for Text-to-Speech Conversion.

This module handles text-to-speech conversion using ElevenLabs (preferred)
or Google TTS as fallback. Outputs PCM audio compatible with ESP32 speaker.

Audio Format: 16kHz, Stereo, 16-bit PCM

Author: James Karui
"""

import requests
from pydub import AudioSegment
import logging
import io

from app.config import settings

logger = logging.getLogger(__name__)


def text_to_pcm(text: str) -> bytes:
    """
    Convert text to PCM audio using configured TTS service.

    Uses ElevenLabs if API key and voice ID are configured,
    otherwise falls back to Google TTS (gTTS).

    Args:
        text: Text to convert to speech

    Returns:
        Raw PCM bytes (16kHz, Stereo, 16-bit) or empty bytes on error
    """
    #if settings.ELEVENLABS_API_KEY and settings.ELEVENLABS_VOICE_ID:
    #    return _text_to_pcm_elevenlabs(text)
    #else:
    return _text_to_pcm_gtts(text)


def _text_to_pcm_elevenlabs(text: str) -> bytes:
    """
    Convert text to speech using ElevenLabs API.

    ElevenLabs provides high-quality voice cloning. Configure your
    cloned voice ID in the .env file.

    Args:
        text: Text to convert (supports SSML tags)

    Returns:
        Raw PCM bytes or falls back to gTTS on error
    """
    api_key = settings.ELEVENLABS_API_KEY
    voice_id = settings.ELEVENLABS_VOICE_ID

    if not api_key or not voice_id:
        logger.warning("ElevenLabs not configured, using gTTS")
        return _text_to_pcm_gtts(text)

    try:
        url = f"https://api.elevenlabs.io/v1/text-to-speech/{voice_id}"

        headers = {
            "Accept": "audio/mpeg",
            "Content-Type": "application/json",
            "xi-api-key": api_key
        }

        data = {
            "text": text,
            "model_id": "eleven_monolingual_v1",
            "voice_settings": {
                "stability": 1.0,
                "similarity_boost": 1.0
            }
        }

        response = requests.post(url, json=data, headers=headers, timeout=30)

        if response.status_code != 200:
            logger.error(f"ElevenLabs error {response.status_code}: {response.text}")
            return _text_to_pcm_gtts(text)

        # Convert MP3 to PCM (16kHz Stereo 16-bit)
        audio = AudioSegment.from_mp3(io.BytesIO(response.content))
        audio = audio.set_frame_rate(16000).set_channels(2).set_sample_width(2)

        return audio.raw_data

    except Exception as e:
        logger.error(f"ElevenLabs error: {e}")
        return _text_to_pcm_gtts(text)


def _text_to_pcm_gtts(text: str) -> bytes:
    """
    Convert text to speech using Google TTS (gTTS).

    Free fallback option with decent quality.

    Args:
        text: Text to convert

    Returns:
        Raw PCM bytes or empty bytes on error
    """
    try:
        from gtts import gTTS
        import re

        # Strip SSML tags if present (gTTS doesn't support them)
        clean_text = re.sub(r'<[^>]+>', '', text) if text.startswith("<speak>") else text

        mp3_buffer = io.BytesIO()
        gTTS(text=clean_text, lang='en').write_to_fp(mp3_buffer)
        mp3_buffer.seek(0)

        # Convert to PCM (16kHz Stereo 16-bit)
        audio = AudioSegment.from_mp3(mp3_buffer)
        audio = audio.set_frame_rate(16000).set_channels(2).set_sample_width(2)

        return audio.raw_data

    except Exception as e:
        logger.error(f"gTTS error: {e}")
        return b""

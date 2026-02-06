
import struct

def create_wav_header(pcm_data: bytes, sample_rate=16000, channels=1, bits_per_sample=16) -> bytes:
    """
    Creates a WAV header for the given PCM data.
    """
    byte_count = len(pcm_data)
    header = struct.pack('<4sI4s', b'RIFF', byte_count + 36, b'WAVE')
    header += struct.pack('<4sIHHIIHH', b'fmt ', 16, 1, channels, sample_rate, 
                          sample_rate * channels * bits_per_sample // 8, 
                          channels * bits_per_sample // 8, bits_per_sample)
    header += struct.pack('<4sI', b'data', byte_count)
    return header + pcm_data


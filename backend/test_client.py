import asyncio
import websockets
import json
import random
import sys

# Usage: python test_client.py

async def test_streaming():
    # Retrieve token from env or use default for test
    token = "my_secure_token_123" # Must match .env
    uri = f"ws://localhost:8000/ws/audio?token={token}"
    print(f"Connecting to {uri}...")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("Connected!")
            
            # Simulate streaming audio for 5 seconds
            print("Streaming dummy audio data...")
            for i in range(50): # 50 chunks * 0.1s = 5s
                # Send 3200 bytes (approx 1600 samples = 0.1s at 16kHz)
                # Dummy content: Silence mostly, or random noise
                dummy_audio = bytes([0] * 3200) 
                await websocket.send(dummy_audio)
                await asyncio.sleep(0.1)
                print(f"Sent chunk {i+1}/50", end='\r')
            
            print("\nFinished streaming. Waiting for response...")
            
            # Keep connection open for a bit to receive response
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=10.0)
                print(f"Received response: {response}")
                data = json.loads(response)
                print(f"Parsed Command: Motor {data.get('motor_id')} -> {data.get('direction')}")
            except asyncio.TimeoutError:
                print("No response received (Timeout). This might be normal if Gemini didn't detect a command in safety silence.")
            except websockets.exceptions.ConnectionClosed:
                print("Connection closed by server.")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    asyncio.run(test_streaming())

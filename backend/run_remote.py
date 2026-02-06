import os
import sys
import threading
import time
from pyngrok import ngrok
import uvicorn
from dotenv import load_dotenv

# Load env vars
load_dotenv()

# Ensure we're in the backend directory for imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

def start_server():
    uvicorn.run("app.main:app", host="0.0.0.0", port=8000, log_level="error")

if __name__ == "__main__":
    print("----------------------------------------------------------------")
    print("Starting Voice Bridge Backend + Remote Access...")
    print("----------------------------------------------------------------")

    # Start Uvicorn in a separate thread
    server_thread = threading.Thread(target=start_server)
    server_thread.daemon = True
    server_thread.start()

    # Give server a moment to start
    time.sleep(2)

    try:
        # Check for Auth Token
        token = os.getenv("NGROK_AUTHTOKEN")
        if token and token != "your_token_here":
            ngrok.set_auth_token(token)
        else:
            print("\n⚠️  WARNING: NGROK_AUTHTOKEN not found in backend/.env")
            print("   You may need to sign up at https://dashboard.ngrok.com/signup")
            print("   and add your token to backend/.env to avoid errors.\n")

        # Open a HTTP tunnel on the default port 8000
        # <NgrokTunnel: "http://<public_sub>.ngrok.io" -> "http://localhost:8000">
        domain = os.getenv("NGROK_DOMAIN")
        if domain:
            public_url = ngrok.connect(8000, domain=domain).public_url
        else:
            public_url = ngrok.connect(8000).public_url
            
        print(f"\n🚀 Remote Access Ready! \n   Access your dashboard here: {public_url}\n")
        print("   (Login with admin / password)")
        print("----------------------------------------------------------------")

        # Keep the main thread alive
        server_thread.join()
    except KeyboardInterrupt:
        print("\nShutting down...")
        ngrok.kill()
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}")
        ngrok.kill()
        sys.exit(1)

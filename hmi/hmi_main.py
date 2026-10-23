"""
hmi_main.py
===========
Main HMI Digital Instrument Cluster Application.
Connects to Telemetry, evaluates Warning Logic, formats Dashboard metrics,
and launches the interactive high-FPS display.
"""

import os
import sys
import webbrowser
import http.server
import socketserver
import threading
from typing import Dict, Any

# Ensure base directory is in sys.path
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

try:
    from hmi.dashboard_ui import DashboardUI
    from hmi.warning_logic import WarningLogic
    from hmi.charging_screen import ChargingScreenUI
except ImportError:
    from dashboard_ui import DashboardUI
    from warning_logic import WarningLogic
    from charging_screen import ChargingScreenUI

PORT = 8080


class DashboardServerHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        directory = os.path.dirname(os.path.abspath(__file__))
        super().__init__(*args, directory=directory, **kwargs)

    def log_message(self, format, *args):
        # Suppress verbose standard HTTP request logging in terminal
        pass

def start_hmi_server(port: int = PORT) -> str:
    """Starts local HTTP server for the HMI dashboard."""
    handler = DashboardServerHandler
    # Enable address reuse to avoid port binding errors on restarts
    socketserver.TCPServer.allow_reuse_address = True
    httpd = socketserver.TCPServer(("", port), handler)
    
    server_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    server_thread.start()
    url = f"http://localhost:{port}/web_dashboard.html"
    return url

def main():
    print("[HMI] Starting EV Digital Cockpit...", flush=True)
    url = start_hmi_server(PORT)
    print(f"[HMI] Dashboard online at: {url}", flush=True)
    try:
        # Attempt to open web browser dashboard
        webbrowser.open(url)
    except Exception as e:
        print(f"[HMI] Could not auto-open browser: {e}", flush=True)

    print("[HMI] Cockpit is running. Press Ctrl+C to terminate.", flush=True)
    try:
        while True:
            threading.Event().wait(1.0)
    except KeyboardInterrupt:
        print("\n[HMI] Shutting down.", flush=True)

if __name__ == "__main__":
    main()


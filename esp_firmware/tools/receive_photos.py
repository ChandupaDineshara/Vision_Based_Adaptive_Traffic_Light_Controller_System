"""
Receives the JPEGs POSTed by the ESP32-CAM and saves them in ./photos/.

Run on the laptop:   python receive_photos.py
Then put this laptop's IP (ipconfig) into UPLOAD_URL in esp32_cam_node.ino.
The laptop firewall must allow inbound TCP on PORT (Python on a private network).
"""

import os
from datetime import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 8000
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "photos")


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/upload":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", 0))
        data = self.rfile.read(length)

        name = datetime.now().strftime("%Y%m%d_%H%M%S") + ".jpg"
        with open(os.path.join(OUT_DIR, name), "wb") as f:
            f.write(data)
        print(f"saved {name} ({len(data)} bytes) from {self.client_address[0]}")

        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"OK")


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"Listening on 0.0.0.0:{PORT}, saving to {OUT_DIR}")
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()

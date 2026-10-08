"""
Receives the JPEGs POSTed by the ESP32-CAM tuning build and saves them in ./photos/.

Based on the team-mate's laptop_receiver/receive_photos.py. Change: the numbers the ESP32 computed
for each photo (sent in HTTP headers) are put in the file name and in photos/log.csv, so the photos
can later be labelled and the thresholds tuned:

    20261008_141502_cap0007_L2_G63.jpg      L = density level, G = mean gradient, cap = wake counter

Run on the laptop:   python receive_photos.py
Then put this laptop's IP address (ipconfig) into UPLOAD_URL in include/secrets.h.
The laptop firewall must allow inbound TCP on PORT (Python on a private network).
"""

import os
from datetime import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 8000
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "photos")
LOG_PATH = os.path.join(OUT_DIR, "log.csv")


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/upload":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", 0))
        data = self.rfile.read(length)

        level = self.headers.get("X-Density-Level", "x")
        grad = self.headers.get("X-Mean-Grad", "x")
        cap = self.headers.get("X-Capture-No", "0")

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        name = "%s_cap%04d_L%s_G%s.jpg" % (stamp, int(cap) if cap.isdigit() else 0, level, grad)
        with open(os.path.join(OUT_DIR, name), "wb") as f:
            f.write(data)

        new_log = not os.path.exists(LOG_PATH)
        with open(LOG_PATH, "a") as log:
            if new_log:
                log.write("file,level,mean_grad,capture_no,bytes\n")
            log.write("%s,%s,%s,%s,%d\n" % (name, level, grad, cap, len(data)))

        print("saved %s (%d bytes) from %s" % (name, len(data), self.client_address[0]))

        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"OK")


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    print("Listening on 0.0.0.0:%d, saving to %s" % (PORT, OUT_DIR))
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()

from flask import Flask, request, jsonify
import time
import csv
import os
from datetime import datetime, timezone

app = Flask(__name__)

CSV_PATH = "soil_log.csv"
CSV_HEADER = ["timestamp_iso", "timestamp_unix", "moisture", "tempC", "client_ip"]

def append_to_csv(row):
    file_exists = os.path.exists(CSV_PATH)
    with open(CSV_PATH, "a", newline="") as f:
        writer = csv.writer(f)
        if not file_exists:
            writer.writerow(CSV_HEADER)
        writer.writerow(row)

latest = {
    "ts": None,
    "moisture": None,
    "tempC": None,
}

@app.route("/update", methods=["POST"])
def update():
    data = request.get_json(force=True, silent=False) or {}

    now = datetime.now(timezone.utc)
    ts_iso = now.isoformat()
    ts_unix = now.timestamp()

    moisture = data.get("moisture")
    tempC    = data.get("tempC")
    client_ip = request.remote_addr

    latest["ts"] = ts_unix
    latest["moisture"] = moisture
    latest["tempC"] = tempC

    append_to_csv([ts_iso, ts_unix, moisture, tempC, client_ip])

    return jsonify({"ok": True})

@app.route("/latest", methods=["GET"])
def get_latest():
    return jsonify(latest)

@app.route("/", methods=["GET"])
def home():
    # Simple page that fetches /latest every 2 seconds
    return """
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Soil Node Dashboard</title>
  </head>
  <body>
    <h1>Latest Soil Reading</h1>
    <pre id="out">Waiting for first update...</pre>

    <script>
      async function refresh() {
        try {
          const r = await fetch('/latest');
          const j = await r.json();
          document.getElementById('out').textContent = JSON.stringify(j, null, 2);
        } catch (e) {
          document.getElementById('out').textContent = "Error fetching /latest: " + e;
        }
      }
      setInterval(refresh, 2000);
      refresh();
    </script>
  </body>
</html>
"""

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=False)
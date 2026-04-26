#pragma once

// Offline dashboard HTML (served at GET /)
// This is a single self-contained HTML document (CSS + JS included) so it works while connected
// to the ESP32 SoftAP without internet access or external CDNs
//
// JS fetches GET /api/measurements and renders the cards + SVG chart
// Expected JSON shape from GET /api/measurements:
//
// {
//   "sampleCount": <number>,
//   "storage": { "totalBytes": <number>, "usedBytes": <number>, "freeBytes": <number> },
//   "samples": [ { "uptimeSeconds": <number>, "distanceCm": <number> }, ... ]
// }
//
// UI-derived values (computed in JS from samples[]):
// - Latest/Min/Max
// - Δ (last sample) = latest - previous (needs >= 2 samples)
// - Δ (last 24)     = latest - value 24 intervals ago (needs >= 25 samples)

const char WEB_DASHBOARD_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Tank Monitor</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0f172a;
      --panel: #1e293b;
      --text: #e2e8f0;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
    }
    .wrap {
      max-width: 960px;
      margin: 0 auto;
      padding: 16px;
    }
    h1 {
      margin: 0 0 16px 0;
      font-size: 24px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 12px;
      margin-bottom: 16px;
    }
    .card {
      background: var(--panel);
      border-radius: 12px;
      padding: 12px;
    }
    .label {
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 6px;
    }
    .value {
      font-size: 22px;
      font-weight: bold;
    }
    .subvalue {
      color: var(--muted);
      font-size: 13px;
      margin-top: 8px;
    }
    .actions {
      display: flex;
      gap: 12px;
      margin-bottom: 16px;
      flex-wrap: wrap;
    }
    button {
      border: 0;
      border-radius: 10px;
      padding: 10px 14px;
      font-size: 14px;
      cursor: pointer;
      color: white;
      background: var(--accent);
    }
    button.danger {
      background: var(--danger);
    }
    .panel {
      background: var(--panel);
      border-radius: 12px;
      padding: 12px;
    }
    .chart-wrap {
      width: 100%;
      overflow-x: auto;
    }
    svg {
      width: 100%;
      height: 320px;
      display: block;
      background: var(--panel);
    }
    .footer {
      margin-top: 12px;
      color: var(--muted);
      font-size: 12px;
    }
    .empty {
      color: var(--muted);
      padding: 24px 0;
      text-align: center;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32-C3 Tank Monitor</h1>

    <div class="grid">
      <div class="card">
        <div class="label">Samples</div>
        <div class="value" id="sampleCount">-</div>
      </div>
      <div class="card">
        <div class="label">Latest</div>
        <div class="value" id="latest">-</div>
      </div>
      <div class="card">
        <div class="label">Min</div>
        <div class="value" id="min">-</div>
      </div>
      <div class="card">
        <div class="label">Max</div>
        <div class="value" id="max">-</div>
      </div>
      <div class="card">
        <div class="label">Δ (last sample)</div>
        <div class="value" id="deltaLast">-</div>
      </div>
      <div class="card">
        <div class="label">Δ (last 24 samples)</div>
        <div class="value" id="delta24">-</div>
      </div>
      <div class="card">
        <div class="label">Storage</div>
        <div class="value" id="storageUsed">-</div>
        <div class="subvalue" id="storageFree">-</div>
      </div>
    </div>

    <div class="actions">
      <button id="refreshBtn">Refresh</button>
      <button id="clearBtn" class="danger">Clear log</button>
    </div>

    <div class="panel">
      <div id="chartContainer" class="chart-wrap"></div>
      <div class="footer">Full stored history is rendered in the chart.</div>
    </div>
  </div>

  <script>
        function fmtCm(v) {
          return Number.isFinite(v) ? v.toFixed(1) + " cm" : "-";
        }

        function fmtDeltaCm(v) {
          if (!Number.isFinite(v)) return "-";
          const sign = (v > 0) ? "+" : "";
          return sign + v.toFixed(1) + " cm";
        }

        function fmtBytes(v) {
          if (!Number.isFinite(v)) return "-";
          if (v < 1024) return v + " B";
          if (v < 1024 * 1024) return (v / 1024).toFixed(1) + " KB";
          return (v / (1024 * 1024)).toFixed(2) + " MB";
        }

        function setText(id, value) {
          const el = document.getElementById(id);
          if (el) el.textContent = value;
        }

        function renderMeta(data) {
          const samples = data.samples || [];
          setText("sampleCount", String(data.sampleCount ?? samples.length));

          setText("storageUsed", fmtBytes(data.storage?.usedBytes) + " used");
          setText("storageFree", fmtBytes(data.storage?.freeBytes) + " free");

          if (!samples.length) {
            setText("latest", "-");
            setText("min", "-");
            setText("max", "-");
            setText("deltaLast", "-");
            setText("delta24", "-");
            return;
          }

          const values = samples.map(s => Number(s.distanceCm)).filter(Number.isFinite);
          const latest = values[values.length - 1];
          const min = Math.min(...values);
          const max = Math.max(...values);

          setText("latest", fmtCm(latest));
          setText("min", fmtCm(min));
          setText("max", fmtCm(max));

          const prev = (values.length >= 2) ? values[values.length - 2] : NaN;
          const deltaLast = Number.isFinite(prev) ? (latest - prev) : NaN;
          setText("deltaLast", fmtDeltaCm(deltaLast));

          const back24 = (values.length >= 25) ? values[values.length - 25] : NaN;
          const delta24 = Number.isFinite(back24) ? (latest - back24) : NaN;
          setText("delta24", fmtDeltaCm(delta24));
        }

        function renderChart(samples) {
          const container = document.getElementById("chartContainer");
          container.innerHTML = "";

          if (!samples || !samples.length) {
            container.innerHTML = '<div class="empty">No measurements yet</div>';
            return;
          }

          const width = 900;
          const height = 320;
          const padLeft = 50;
          const padRight = 20;
          const padTop = 20;
          const padBottom = 35;

          const values = samples.map(s => Number(s.distanceCm)).filter(Number.isFinite);
          const minY = Math.min(...values);
          const maxY = Math.max(...values);
          const spanY = Math.max(1, maxY - minY);

          const innerWidth = width - padLeft - padRight;
          const innerHeight = height - padTop - padBottom;

          const points = values.map((v, i) => {
            const x = padLeft + (values.length === 1 ? innerWidth / 2 : (i * innerWidth) / (values.length - 1));
            const y = padTop + ((maxY - v) / spanY) * innerHeight;
            return `${x},${y}`;
          }).join(" ");

          const gridLines = [];
          for (let i = 0; i <= 4; i++) {
            const y = padTop + (i * innerHeight) / 4;
            const value = maxY - (i * spanY) / 4;
            gridLines.push(
              `<line x1="${padLeft}" y1="${y}" x2="${width - padRight}" y2="${y}" stroke="#334155" stroke-width="1" />` +
              `<text x="8" y="${y + 4}" fill="#94a3b8" font-size="11">${value.toFixed(1)}</text>`
            );
          }

          const axis = `
            <line x1="${padLeft}" y1="${padTop}" x2="${padLeft}" y2="${height - padBottom}" stroke="#64748b" stroke-width="1.5" />
            <line x1="${padLeft}" y1="${height - padBottom}" x2="${width - padRight}" y2="${height - padBottom}" stroke="#64748b" stroke-width="1.5" />
            <text x="${width / 2}" y="${height - 8}" fill="#94a3b8" font-size="12" text-anchor="middle">Sample index</text>
          `;

          const polyline = `<polyline fill="none" stroke="#38bdf8" stroke-width="2.5" points="${points}" />`;

          container.innerHTML = `
            <svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="none" aria-label="distance chart">
              ${gridLines.join("")}
              ${axis}
              ${polyline}
            </svg>
          `;
        }

    async function loadMeasurements() {
      const res = await fetch("/api/measurements", { cache: "no-store" });
      if (!res.ok)
        throw new Error("Failed to load measurements");

      const data = await res.json();
      renderMeta(data);
      renderChart(data.samples || []);
    }

    async function clearLog() {
      if (!confirm("Clear all stored measurements?"))
        return;

      const res = await fetch("/api/clear", { method: "POST" });
      if (!res.ok)
        throw new Error("Failed to clear log");

      const data = await res.json();
      if (!data.ok)
        throw new Error(data.error || "Clear failed");

      await loadMeasurements();
    }

    document.getElementById("refreshBtn").addEventListener("click", () => {
      loadMeasurements().catch(err => alert(err.message));
    });

    document.getElementById("clearBtn").addEventListener("click", () => {
      clearLog().catch(err => alert(err.message));
    });

    loadMeasurements().catch(err => {
      document.getElementById("chartContainer").innerHTML =
        '<div class="empty">Failed to load measurements</div>';
      alert(err.message);
    });
  </script>
</body>
</html>
)html";
/**
 * SCaDA COMMAND CENTER — Core Application Engine
 * Pure Modern Web Platform (ES Module, Web Serial API, Native Canvas, Web Audio)
 * Compliant with modern-web-guidance: Zero external dependencies, a11y, tabular-nums
 */

// ============================================================================
// 1. STATE & CONSTANTS
// ============================================================================
const STATE = {
  port: null,
  reader: null,
  abortController: null,
  isConnected: false,
  totalPackets: 0,
  lastRssi: null,
  lastSnr: null,
  isAudioMuted: false,
  alarmState: 'OK', // 'OK', 'PRE', 'SOS'
  lastPacketTime: null, // Waktu epoch ms paket telemetri LoRa terakhir
  linkStatus: 'WAITING', // 'WAITING', 'LIVE', 'UNSTABLE', 'LOST'
  lastValidTelemetry: null,
  history: [],      // Sliding window 60 telemetry points for Canvas
  sessionLog: []    // Full session records for CSV Export
};

const MAX_HISTORY_POINTS = 60;
const MAX_TABLE_ROWS = 200;

// Audio Synthesizer State
let audioCtx = null;
let sirenOsc = null;
let sirenGain = null;
let sirenInterval = null;
let sirenFreqToggle = false;

// ============================================================================
// 2. DOM ELEMENT REFERENCES
// ============================================================================
const DOM = {
  // Connection & Links
  btnConnect: document.getElementById('btn-connect'),
  btnDisconnect: document.getElementById('btn-disconnect'),
  statusDot: document.getElementById('status-dot'),
  connectionLabel: document.getElementById('connection-label'),
  nodeId: document.getElementById('node-id'),
  valRssi: document.getElementById('val-rssi'),
  valSnr: document.getElementById('val-snr'),
  valPktCount: document.getElementById('val-pkt-count'),
  valLinkStatus: document.getElementById('val-link-status'),
  valLastSeen: document.getElementById('val-last-seen'),

  // Alarm Banner
  alarmBanner: document.getElementById('alarm-banner'),
  alarmIcon: document.getElementById('alarm-icon'),
  alarmTitle: document.getElementById('alarm-title'),
  alarmDesc: document.getElementById('alarm-desc'),
  btnAckAlarm: document.getElementById('btn-ack-alarm'),

  // Metric Cards
  valHr: document.getElementById('val-hr'),
  statusHr: document.getElementById('status-hr'),
  cardHr: document.getElementById('card-hr'),

  valSpo2: document.getElementById('val-spo2'),
  statusSpo2: document.getElementById('status-spo2'),
  cardSpo2: document.getElementById('card-spo2'),

  valTemp: document.getElementById('val-temp'),
  statusTemp: document.getElementById('status-temp'),
  cardTemp: document.getElementById('card-temp'),

  valPsi: document.getElementById('val-psi'),
  valPsiTag: document.getElementById('val-psi-tag'),
  statusPsi: document.getElementById('status-psi'),
  psiMeterFill: document.getElementById('psi-meter-fill'),
  cardPsi: document.getElementById('card-psi'),

  // Analytics & History
  chartCanvas: document.getElementById('chart-canvas'),
  telemetryTable: document.getElementById('telemetry-table'),
  telemetryTbody: document.getElementById('telemetry-tbody'),
  btnExportCsv: document.getElementById('btn-export-csv'),
  btnClearLog: document.getElementById('btn-clear-log')
};

// ============================================================================
// 3. WEB AUDIO SYNTHESIZER (PASS DISTRESS SIREN)
// ============================================================================
function initAudio() {
  if (!audioCtx) {
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (AudioContextClass) {
      audioCtx = new AudioContextClass();
    }
  }
  if (audioCtx && audioCtx.state === 'suspended') {
    audioCtx.resume();
  }
}

function startSiren() {
  if (STATE.isAudioMuted || sirenInterval) return;
  initAudio();
  if (!audioCtx) return;

  try {
    sirenOsc = audioCtx.createOscillator();
    sirenGain = audioCtx.createGain();

    sirenOsc.type = 'sawtooth';
    sirenOsc.frequency.setValueAtTime(800, audioCtx.currentTime);
    sirenGain.gain.setValueAtTime(0.2, audioCtx.currentTime);

    sirenOsc.connect(sirenGain);
    sirenGain.connect(audioCtx.destination);
    sirenOsc.start();

    // Two-tone high-low alarm sweep: 800 Hz / 1200 Hz
    sirenInterval = setInterval(() => {
      if (!sirenOsc || !audioCtx) return;
      sirenFreqToggle = !sirenFreqToggle;
      const targetFreq = sirenFreqToggle ? 1200 : 800;
      sirenOsc.frequency.cancelScheduledValues(audioCtx.currentTime);
      sirenOsc.frequency.linearRampToValueAtTime(targetFreq, audioCtx.currentTime + 0.15);
    }, 250);
  } catch (err) {
    console.warn('Audio siren start failed:', err);
  }
}

function stopSiren() {
  if (sirenInterval) {
    clearInterval(sirenInterval);
    sirenInterval = null;
  }
  if (sirenOsc) {
    try {
      sirenOsc.stop();
      sirenOsc.disconnect();
    } catch (_) {}
    sirenOsc = null;
  }
  if (sirenGain) {
    try {
      sirenGain.disconnect();
    } catch (_) {}
    sirenGain = null;
  }
}

function toggleAlarmAudio() {
  if (STATE.alarmState !== 'SOS') return;

  if (STATE.isAudioMuted) {
    // Unmute / Bunyikan kembali
    STATE.isAudioMuted = false;
    startSiren();
    DOM.btnAckAlarm.textContent = '🔕 Akui & Bisukan Sirene (Esc)';
    DOM.btnAckAlarm.classList.remove('btn-secondary');
    DOM.btnAckAlarm.classList.add('btn-danger');
  } else {
    // Mute / Bisukan
    STATE.isAudioMuted = true;
    stopSiren();
    DOM.btnAckAlarm.textContent = '🔊 Bunyikan Sirene Kembali';
    DOM.btnAckAlarm.classList.remove('btn-danger');
    DOM.btnAckAlarm.classList.add('btn-secondary');
  }
}

// ============================================================================
// 4. PARSER & DATA INGESTION
// ============================================================================

/**
 * Mem-parsing baris string dari receiver LoRa
 * Mendukung format metadata RSSI/SNR dan format data telemetri multi-sensor
 */
function parseSerialLine(line) {
  const cleanLine = line.trim();
  if (!cleanLine) return null;

  // Format 1: Baris RSSI & SNR
  // [PAKET MASUK!] Panjang: 96 byte | RSSI: -54 dBm | SNR: 8.5 dB
  if (cleanLine.includes('PAKET MASUK') || cleanLine.includes('RSSI:')) {
    const rssiMatch = cleanLine.match(/RSSI:\s*(-?\d+)\s*dBm/i);
    const snrMatch = cleanLine.match(/SNR:\s*(-?[\d.]+)\s*dB/i);

    if (rssiMatch) STATE.lastRssi = parseInt(rssiMatch[1], 10);
    if (snrMatch) STATE.lastSnr = parseFloat(snrMatch[1]);

    updateRadioLinkUI();
    return { type: 'METADATA', rssi: STATE.lastRssi, snr: STATE.lastSnr };
  }

  // Format 2: Baris Data Telemetri
  // Data Diterima : SCaDA Cuff #1 | HR: 65 BPM | SpO2: 98% | Temp: 32.4 C | PSI: 1.2(SAFE) | Stat: MOVE | Alarm: OK | Pkt: 101
  if (cleanLine.includes('HR:') && cleanLine.includes('SpO2:')) {
    const hrMatch = cleanLine.match(/HR:\s*(\d+)/i);
    const spo2Match = cleanLine.match(/SpO2:\s*(\d+)/i);
    const tempMatch = cleanLine.match(/Temp:\s*([\d.]+)/i);
    const psiMatch = cleanLine.match(/PSI:\s*([\d.]+)(?:\(([A-Za-z]+)\))?/i);
    const statMatch = cleanLine.match(/Stat:\s*([A-Za-z0-9_-]+)/i);
    const alarmMatch = cleanLine.match(/Alarm:\s*([A-Za-z0-9_-]+)/i);
    const pktMatch = cleanLine.match(/Pkt:\s*(\d+)/i);
    const nodeMatch = cleanLine.match(/(?:Data Diterima\s*:\s*)?([A-Za-z0-9\s#]+?)\s*\|/i);

    if (!hrMatch || !spo2Match || !tempMatch) return null;

    const data = {
      type: 'TELEMETRY',
      timestamp: new Date().toLocaleTimeString('id-ID', { hour12: false }),
      isoTime: new Date().toISOString(),
      node: nodeMatch ? nodeMatch[1].trim() : 'Cuff #1',
      hr: parseInt(hrMatch[1], 10),
      spo2: parseInt(spo2Match[1], 10),
      temp: parseFloat(tempMatch[1]),
      psi: psiMatch ? parseFloat(psiMatch[1]) : 0.0,
      psiTag: psiMatch && psiMatch[2] ? psiMatch[2].toUpperCase() : 'SAFE',
      stat: statMatch ? statMatch[1].toUpperCase() : 'MOVE',
      alarm: alarmMatch ? alarmMatch[1].toUpperCase() : 'OK',
      pkt: pktMatch ? parseInt(pktMatch[1], 10) : ++STATE.totalPackets,
      rssi: STATE.lastRssi,
      snr: STATE.lastSnr
    };

    return data;
  }

  return null;
}

/**
 * Titik masuk konsumsi data: Mengupdate UI, tabel, grafik, dan alarm
 */
function ingestTelemetryData(data) {
  if (!data || data.type !== 'TELEMETRY') return;

  STATE.totalPackets++;
  STATE.lastPacketTime = Date.now();
  STATE.lastValidTelemetry = data;
  DOM.valPktCount.textContent = STATE.totalPackets;
  DOM.nodeId.textContent = data.node;

  // 1. Update Kartu Metrik
  DOM.valHr.textContent = data.hr;
  if (data.hr < 50 || data.hr > 120) {
    DOM.cardHr.className = 'metric-card alert-warn';
    DOM.statusHr.textContent = data.hr < 50 ? 'Bradikardia' : 'Takikardia Ringan';
  } else if (data.hr < 40 || data.hr > 140) {
    DOM.cardHr.className = 'metric-card alert-crit';
    DOM.statusHr.textContent = 'Kritis Tidak Wajar';
  } else {
    DOM.cardHr.className = 'metric-card';
    DOM.statusHr.textContent = 'Rentang Normal';
  }

  DOM.valSpo2.textContent = data.spo2;
  if (data.spo2 < 90) {
    DOM.cardSpo2.className = 'metric-card alert-crit';
    DOM.statusSpo2.textContent = 'Hipoksia Kritis (<90%)';
  } else if (data.spo2 < 95) {
    DOM.cardSpo2.className = 'metric-card alert-warn';
    DOM.statusSpo2.textContent = 'Waspada Perfusi (<95%)';
  } else {
    DOM.cardSpo2.className = 'metric-card';
    DOM.statusSpo2.textContent = 'Perfusi Oksigen Baik';
  }

  DOM.valTemp.textContent = data.temp.toFixed(1);
  if (data.temp > 38.0) {
    DOM.cardTemp.className = 'metric-card alert-crit';
    DOM.statusTemp.textContent = 'Hipertermia Kritis';
  } else if (data.temp > 37.5) {
    DOM.cardTemp.className = 'metric-card alert-warn';
    DOM.statusTemp.textContent = 'Suhu Meningkat';
  } else if (data.temp < 30.0) {
    // Di bawah suhu kulit wajar: sensor kemungkinan membaca udara ruangan
    DOM.cardTemp.className = 'metric-card alert-warn';
    DOM.statusTemp.textContent = 'Sensor Tidak Menempel Kulit';
  } else {
    DOM.cardTemp.className = 'metric-card';
    DOM.statusTemp.textContent = 'Suhu Kulit Normal';
  }

  DOM.valPsi.textContent = data.psi.toFixed(1);
  DOM.valPsiTag.textContent = data.psiTag;

  // Visual meter progress bar (skala 0 - 10)
  const psiPercent = Math.min(100, Math.max(0, (data.psi / 10) * 100));
  DOM.psiMeterFill.style.width = `${psiPercent}%`;

  // Ambang identik dengan firmware updateMPSI(): WARN >= 3.0, CRIT >= 6.5
  if (data.psi >= 6.5) {
    DOM.valPsiTag.className = 'metric-tag tag-crit';
    DOM.cardPsi.className = 'metric-card alert-crit';
    DOM.statusPsi.textContent = 'Strain Kritis (Risiko Kolaps)';
  } else if (data.psi >= 3.0) {
    DOM.valPsiTag.className = 'metric-tag tag-warn';
    DOM.cardPsi.className = 'metric-card alert-warn';
    DOM.statusPsi.textContent = 'Beban Termal/Kardio Tinggi';
  } else {
    DOM.valPsiTag.className = 'metric-tag tag-safe';
    DOM.cardPsi.className = 'metric-card';
    DOM.statusPsi.textContent = 'Beban Fisiologis Aman';
  }

  // 2. Update Alarm FSM Banner
  updateAlarmFsm(data);

  // 3. Masukkan ke buffer history grafik
  STATE.history.push({
    hr: data.hr,
    psi: data.psi,
    time: data.timestamp
  });
  if (STATE.history.length > MAX_HISTORY_POINTS) {
    STATE.history.shift();
  }

  // 4. Masukkan ke tabel telemetri dan log sesi
  appendTableRow(data);
  STATE.sessionLog.push(data);
  updateWatchdog();
}

function updateRadioLinkUI() {
  if (STATE.lastRssi !== null) {
    DOM.valRssi.textContent = `${STATE.lastRssi} dBm`;
  }
  if (STATE.lastSnr !== null) {
    DOM.valSnr.textContent = `${STATE.lastSnr} dB`;
  }
}

/**
 * LoRa Link Heartbeat Watchdog: Memeriksa apakah aliran paket telemetri aktif
 */
function updateWatchdog() {
  if (!STATE.isConnected) {
    if (DOM.valLinkStatus) {
      DOM.valLinkStatus.textContent = 'DISCONNECTED';
      DOM.valLinkStatus.className = 'stat-value link-waiting';
    }
    if (DOM.valLastSeen) {
      DOM.valLastSeen.textContent = '--';
    }
    return;
  }

  if (!STATE.lastPacketTime) {
    if (DOM.valLinkStatus) {
      DOM.valLinkStatus.textContent = 'MENUNGGU DATA...';
      DOM.valLinkStatus.className = 'stat-value link-waiting';
    }
    if (DOM.valLastSeen) {
      DOM.valLastSeen.textContent = 'Belum ada data';
    }
    return;
  }

  const elapsedSec = Math.floor((Date.now() - STATE.lastPacketTime) / 1000);

  // Label durasi terakhir diterima
  if (DOM.valLastSeen) {
    DOM.valLastSeen.textContent = elapsedSec <= 1 ? 'Baru saja' : `${elapsedSec}s lalu`;
  }

  // Evaluasi status tautan radio:
  // < 6s  : LIVE
  // 6 - 14s : UNSTABLE
  // >= 15s : SIGNAL LOST / TERPUTUS
  if (elapsedSec < 6) {
    STATE.linkStatus = 'LIVE';
    if (DOM.valLinkStatus) {
      DOM.valLinkStatus.textContent = '🟢 LIVE';
      DOM.valLinkStatus.className = 'stat-value link-live';
    }

    // Hapus kelas is-stale dari kartu metrik
    DOM.cardHr.classList.remove('is-stale');
    DOM.cardSpo2.classList.remove('is-stale');
    DOM.cardTemp.classList.remove('is-stale');
    DOM.cardPsi.classList.remove('is-stale');

    // Jika banner sempat menampilkan status link lost dan paket kembali normal, kembalikan ke FSM
    if (DOM.alarmBanner.classList.contains('status-link-lost') && STATE.lastValidTelemetry) {
      updateAlarmFsm(STATE.lastValidTelemetry);
    }
  } else if (elapsedSec < 15) {
    STATE.linkStatus = 'UNSTABLE';
    if (DOM.valLinkStatus) {
      DOM.valLinkStatus.textContent = '🟡 TIDAK STABIL';
      DOM.valLinkStatus.className = 'stat-value link-unstable';
    }
  } else {
    // Sinyal Terputus (Lost >= 15 detik)
    STATE.linkStatus = 'LOST';
    if (DOM.valLinkStatus) {
      DOM.valLinkStatus.textContent = '🔴 TERPUTUS';
      DOM.valLinkStatus.className = 'stat-value link-lost';
    }

    // Tandai seluruh kartu metrik sebagai data stale/usang
    DOM.cardHr.classList.add('is-stale');
    DOM.cardSpo2.classList.add('is-stale');
    DOM.cardTemp.classList.add('is-stale');
    DOM.cardPsi.classList.add('is-stale');

    // Tampilkan peringatan putus kontak pada alarm banner
    DOM.alarmBanner.className = 'alarm-banner status-link-lost';
    DOM.alarmIcon.textContent = '📡';
    DOM.alarmTitle.textContent = 'PERINGATAN: KOMUNIKASI LORA TERPUTUS!';
    DOM.alarmDesc.textContent = `Posko kehilangan sinyal telemetri dari ${DOM.nodeId.textContent || 'Cuff #1'} selama ${elapsedSec} detik! Periksa status fisik atau baterai personel.`;
    DOM.btnAckAlarm.hidden = true;
  }
}

/**
 * Update Banner Status FSM PASS NFPA 1982
 */
function updateAlarmFsm(data) {
  const alarm = data.alarm;
  STATE.alarmState = alarm;

  if (alarm === 'SOS') {
    DOM.alarmBanner.className = 'alarm-banner status-sos';
    DOM.alarmIcon.textContent = '🚨';
    DOM.alarmTitle.textContent = 'STATUS: FULL DISTRESS SOS (KRITIS)';
    DOM.alarmDesc.textContent = `DARURAT! Personel henti gerak (${data.stat}), tombol SOS ditekan, atau SpO2/PSI kritis! Segera evakuasi!`;
    DOM.btnAckAlarm.hidden = false;

    if (!STATE.isAudioMuted) {
      DOM.btnAckAlarm.textContent = '🔕 Akui & Bisukan Sirene (Esc)';
      DOM.btnAckAlarm.classList.remove('btn-secondary');
      DOM.btnAckAlarm.classList.add('btn-danger');
      startSiren();
    } else {
      DOM.btnAckAlarm.textContent = '🔊 Bunyikan Sirene Kembali';
      DOM.btnAckAlarm.classList.remove('btn-danger');
      DOM.btnAckAlarm.classList.add('btn-secondary');
    }
  } else if (alarm === 'PRE') {
    DOM.alarmBanner.className = 'alarm-banner status-pre-alarm';
    DOM.alarmIcon.textContent = '⚠️';
    DOM.alarmTitle.textContent = 'STATUS: PRE-ALARM (PERINGATAN AWAL)';
    DOM.alarmDesc.textContent = `Personel tidak bergerak (${data.stat}) selama ≥15 detik. Menunggu respons gerak wearable.`;
    DOM.btnAckAlarm.hidden = true;
    stopSiren();
    STATE.isAudioMuted = false;
  } else {
    // Normal / OK
    DOM.alarmBanner.className = 'alarm-banner status-normal';
    DOM.alarmIcon.textContent = '🛡️';
    DOM.alarmTitle.textContent = 'STATUS: NORMAL (SAFE)';
    DOM.alarmDesc.textContent = `Personel terdeteksi ${data.stat === 'MOVE' ? 'bergerak aktif' : 'stabil'}. Parameter fisiologis dalam batas aman.`;
    DOM.btnAckAlarm.hidden = true;
    stopSiren();
    STATE.isAudioMuted = false;
  }
}

/**
 * Menambahkan baris ke tabel riwayat telemetri
 */
function appendTableRow(data) {
  const emptyRow = DOM.telemetryTbody.querySelector('.empty-row');
  if (emptyRow) {
    emptyRow.remove();
  }

  const tr = document.createElement('tr');

  let alarmBadgeClass = 'badge-row-ok';
  if (data.alarm === 'PRE') alarmBadgeClass = 'badge-row-pre';
  if (data.alarm === 'SOS') alarmBadgeClass = 'badge-row-sos';

  tr.innerHTML = `
    <td>${data.timestamp}</td>
    <td>#${data.pkt}</td>
    <td><strong>${data.hr}</strong> BPM</td>
    <td>${data.spo2}%</td>
    <td>${data.temp.toFixed(1)}°C</td>
    <td>${data.psi.toFixed(1)} (${data.psiTag})</td>
    <td>${data.stat}</td>
    <td><span class="badge-row-alarm ${alarmBadgeClass}">${data.alarm}</span></td>
    <td>${data.rssi !== null ? data.rssi + ' dBm' : '--'}</td>
  `;

  // Sisipkan baris terbaru di paling atas
  DOM.telemetryTbody.insertBefore(tr, DOM.telemetryTbody.firstChild);

  // Batasi hingga 200 baris terbaru
  while (DOM.telemetryTbody.children.length > MAX_TABLE_ROWS) {
    DOM.telemetryTbody.removeChild(DOM.telemetryTbody.lastChild);
  }
}

// ============================================================================
// 5. NATIVE 2D CANVAS SPARKLINE RENDERER (60 FPS)
// ============================================================================
function initCanvasRenderer() {
  const canvas = DOM.chartCanvas;
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  function resizeCanvas() {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    ctx.resetTransform?.();
    ctx.scale(dpr, dpr);
  }

  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();

  function renderChart() {
    const rect = canvas.getBoundingClientRect();
    const width = rect.width;
    const height = rect.height;

    // Bersihkan canvas
    ctx.clearRect(0, 0, width, height);

    // Padding
    const padL = 40;
    const padR = 20;
    const padT = 20;
    const padB = 30;
    const plotW = width - padL - padR;
    const plotH = height - padT - padB;

    // Grid garis horizontal & label y
    ctx.lineWidth = 1;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.06)';
    ctx.fillStyle = '#64748b';
    ctx.font = '10px monospace';
    ctx.textAlign = 'right';

    const ySteps = [40, 80, 120, 160]; // BPM grid
    for (const val of ySteps) {
      const y = padT + plotH - ((val - 40) / (160 - 40)) * plotH;
      ctx.beginPath();
      ctx.moveTo(padL, y);
      ctx.lineTo(width - padR, y);
      ctx.stroke();
      ctx.fillText(`${val}`, padL - 6, y + 3);
    }

    // Border grid
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.12)';
    ctx.strokeRect(padL, padT, plotW, plotH);

    const history = STATE.history;
    if (history.length >= 2) {
      const stepX = plotW / (MAX_HISTORY_POINTS - 1);
      const startOffset = (MAX_HISTORY_POINTS - history.length) * stepX;

      // 1. Gambar Garis Denyut Jantung (HR - Biru Cyan)
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let i = 0; i < history.length; i++) {
        const x = padL + startOffset + i * stepX;
        const hrClamped = Math.max(40, Math.min(160, history[i].hr));
        const y = padT + plotH - ((hrClamped - 40) / (160 - 40)) * plotH;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();

      // 2. Gambar Garis PSI x 10 (Amber)
      ctx.strokeStyle = '#f59e0b';
      ctx.lineWidth = 2;
      ctx.setLineDash([4, 3]);
      ctx.beginPath();
      for (let i = 0; i < history.length; i++) {
        const x = padL + startOffset + i * stepX;
        const psiScaled = history[i].psi * 12; // Skala agar selaras dengan rentang plot
        const y = padT + plotH - Math.min(plotH, (psiScaled / 120) * plotH);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.setLineDash([]); // Reset dash

      // Titik pulse terakhir
      const lastIdx = history.length - 1;
      const lastX = padL + startOffset + lastIdx * stepX;
      const lastHr = Math.max(40, Math.min(160, history[lastIdx].hr));
      const lastY = padT + plotH - ((lastHr - 40) / (160 - 40)) * plotH;

      ctx.fillStyle = '#38bdf8';
      ctx.beginPath();
      ctx.arc(lastX, lastY, 4, 0, Math.PI * 2);
      ctx.fill();
    } else {
      // Empty hint
      ctx.fillStyle = '#475569';
      ctx.textAlign = 'center';
      ctx.fillText('Grafik aktif saat data telemetri mulai mengalir...', width / 2, height / 2);
    }

    requestAnimationFrame(renderChart);
  }

  requestAnimationFrame(renderChart);
}

// ============================================================================
// 6. WEB SERIAL API ENGINE
// ============================================================================
async function connectSerial() {
  if (!('serial' in navigator)) {
    alert('Browser ini tidak mendukung Web Serial API. Silakan buka halaman ini di Google Chrome atau Microsoft Edge terbaru.');
    return;
  }

  try {
    initAudio(); // Bangun AudioContext lewat user gesture
    STATE.port = await navigator.serial.requestPort();
    await STATE.port.open({ baudRate: 115200 });

    STATE.isConnected = true;
    STATE.lastPacketTime = null;
    STATE.abortController = new AbortController();

    DOM.btnConnect.disabled = true;
    DOM.btnDisconnect.disabled = false;
    DOM.statusDot.className = 'status-dot connected';
    DOM.connectionLabel.textContent = 'TERHUBUNG (115200 BAUD)';
    updateWatchdog();

    readSerialStream(STATE.port, STATE.abortController.signal);
  } catch (err) {
    console.error('Koneksi port serial gagal:', err);
    if (err.name !== 'NotFoundError') {
      alert(`Gagal membuka port serial: ${err.message}`);
    }
  }
}

async function readSerialStream(port, signal) {
  let lineBuffer = '';
  const textDecoder = new TextDecoderStream();
  const readableStreamClosed = port.readable.pipeTo(textDecoder.writable, { signal });
  const reader = textDecoder.readable.getReader();
  STATE.reader = reader;

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value) {
        lineBuffer += value;
        const lines = lineBuffer.split(/\r?\n/);
        lineBuffer = lines.pop(); // Pertahankan sisa baris belum utuh

        for (const line of lines) {
          if (line.trim()) {
            const parsed = parseSerialLine(line);
            if (parsed && parsed.type === 'TELEMETRY') {
              ingestTelemetryData(parsed);
            }
          }
        }
      }
    }
  } catch (err) {
    if (signal.aborted) {
      console.log('Stream serial dibatalkan secara bersih oleh pengguna.');
    } else {
      console.error('Error saat membaca stream serial:', err);
    }
  } finally {
    reader.releaseLock();
    try {
      await readableStreamClosed.catch(() => {});
    } catch (_) {}
  }
}

async function disconnectSerial() {
  if (STATE.abortController) {
    STATE.abortController.abort();
    STATE.abortController = null;
  }
  if (STATE.reader) {
    try {
      await STATE.reader.cancel();
    } catch (_) {}
    STATE.reader = null;
  }
  if (STATE.port) {
    try {
      await STATE.port.close();
    } catch (err) {
      console.warn('Gagal menutup port secara mulus:', err);
    }
    STATE.port = null;
  }

  STATE.isConnected = false;
  STATE.lastPacketTime = null;
  DOM.btnConnect.disabled = false;
  DOM.btnDisconnect.disabled = true;
  DOM.statusDot.className = 'status-dot disconnected';
  DOM.connectionLabel.textContent = 'DISCONNECTED';
  stopSiren();
  updateWatchdog();
}

// ============================================================================
// 7. CSV EXPORT & UTILITIES
// ============================================================================
function exportCsvLog() {
  if (STATE.sessionLog.length === 0) {
    alert('Belum ada data telemetri yang tercatat untuk diekspor.');
    return;
  }

  const headers = [
    'Timestamp',
    'ISO_Time',
    'Packet_ID',
    'Node',
    'Heart_Rate_BPM',
    'SpO2_Percent',
    'Skin_Temp_C',
    'PSI_Score',
    'PSI_Status',
    'Motion_Status',
    'Alarm_State',
    'RSSI_dBm',
    'SNR_dB'
  ];

  const rows = STATE.sessionLog.map(item => [
    `"${item.timestamp}"`,
    `"${item.isoTime}"`,
    item.pkt,
    `"${item.node}"`,
    item.hr,
    item.spo2,
    item.temp.toFixed(2),
    item.psi.toFixed(2),
    `"${item.psiTag}"`,
    `"${item.stat}"`,
    `"${item.alarm}"`,
    item.rssi !== null ? item.rssi : '',
    item.snr !== null ? item.snr : ''
  ]);

  const csvContent = [headers.join(','), ...rows.map(r => r.join(','))].join('\r\n');
  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);

  const now = new Date();
  const dateStr = now.toISOString().slice(0, 10).replace(/-/g, '');
  const timeStr = now.toTimeString().slice(0, 8).replace(/:/g, '');
  const filename = `scada_telemetry_${dateStr}_${timeStr}.csv`;

  const link = document.createElement('a');
  link.setAttribute('href', url);
  link.setAttribute('download', filename);
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function clearLogTable() {
  DOM.telemetryTbody.innerHTML = `
    <tr class="empty-row">
      <td colspan="9">Tabel log dibersihkan oleh pengguna.</td>
    </tr>
  `;
}

// ============================================================================
// 8. EVENT LISTENERS & AUTOMATION HOOKS
// ============================================================================
DOM.btnConnect.addEventListener('click', connectSerial);
DOM.btnDisconnect.addEventListener('click', disconnectSerial);
DOM.btnAckAlarm.addEventListener('click', toggleAlarmAudio);
DOM.btnExportCsv.addEventListener('click', exportCsvLog);
DOM.btnClearLog.addEventListener('click', clearLogTable);

// Keyboard shortcut: Escape untuk mematikan / membisukan sirene darurat
window.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') {
    if (STATE.alarmState === 'SOS') {
      toggleAlarmAudio();
    }
  }
});

// Inisialisasi Canvas
initCanvasRenderer();

// LoRa Link Heartbeat Watchdog Timer (Periksa setiap 1 detik)
setInterval(updateWatchdog, 1000);
updateWatchdog();

// Deteksi otomatis jika perangkat USB serial dicabut secara fisik
if ('serial' in navigator) {
  navigator.serial.addEventListener('disconnect', () => {
    console.warn('Perangkat serial USB terputus secara fisik!');
    disconnectSerial();
  });
}

// ============================================================================
// 9. AUTOMATION & TEST INGESTION HOOK (EVAL/DEVTOOLS COMPATIBLE)
// ============================================================================
// Memungkinkan pengujian otomatis dan inspeksi DOM tanpa tombol tiruan di UI
window.ingestTelemetryLine = function(rawLine) {
  STATE.isConnected = true;
  const parsed = parseSerialLine(rawLine);
  if (parsed && parsed.type === 'TELEMETRY') {
    ingestTelemetryData(parsed);
  }
  return parsed;
};

window.getTelemetryState = function() {
  return {
    ...STATE,
    historyLength: STATE.history.length,
    sessionLogCount: STATE.sessionLog.length
  };
};

console.log('SCaDA Command Center Engine siap. Gunakan window.ingestTelemetryLine() untuk simulasi paket.');

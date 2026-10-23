/*
  ========================================================================
  SMART GARDEN SYSTEM - ESP32 (v2)
  ========================================================================
  Perubahan dari v1:
  - LED indikator DIHAPUS (status cukup dilihat di dashboard web)
  - Tambah halaman SETTINGS: threshold soil/humidity/temperature bisa
    diatur dari dashboard, tersimpan permanen di flash (Preferences/NVS)
  - Tambah RIWAYAT & GRAFIK penyiraman (soil moisture trend + kapan
    terakhir disiram + estimasi penyiraman berikutnya)
  - Sinkronisasi waktu via NTP supaya timestamp riwayat akurat

  Library yang harus di-install (Library Manager):
  - DHT sensor library (by Adafruit)
  - Adafruit Unified Sensor (dependency DHT)
  - LiquidCrystal I2C (by Frank de Brabander / Marco Schwartz)
  - Preferences.h, WiFi.h, WebServer.h, Wire.h, ESPmDNS.h, time.h
    -> semua sudah termasuk di ESP32 core, tidak perlu install manual
  -> created with XanderID
  ========================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>

// ======================= DEBUG MODE ======================================
// Set true buat nyalain log ke Serial Monitor (development/troubleshooting).
// Set false kalau mau silent -> semua DBG_PRINT/DBG_PRINTLN/DBG_PRINTF
// otomatis hilang total, ga makan waktu proses & ga kepake flash sama sekali.
#define DEBUG_MODE true

#if DEBUG_MODE
  #define DBG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

// ======================= FORWARD DECLARATIONS ===========================
// Ditambahkan manual karena Arduino auto-prototype generator kadang gagal
// mendeteksi fungsi-fungsi ini (dipanggil sebelum definisinya di file).
void setPump(bool on);
void saveSettingsToFlash();
long predictNextWateringSeconds();
void recordWateringStart();
void recordWateringEnd();

// ======================= KONFIGURASI WIFI ==============================
const char* WIFI_SSID     = "raden";
const char* WIFI_PASSWORD = "raden1904";

// ======================= KONFIGURASI LOGIN DASHBOARD ====================
const char* DASHBOARD_PASSWORD = "admin123";
const unsigned long SESSION_TIMEOUT_MS = 10UL * 60UL * 1000UL; // 10 menit

// ======================= KONFIGURASI NTP (WAKTU) ========================
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 7 * 3600;  // WIB (UTC+7)
const int   DST_OFFSET_SEC = 0;

// ======================= PIN DEFINITION =================================
#define SOIL_PIN     34   // ADC1, input only -> cocok untuk analog
#define DHTPIN       4
#define DHTTYPE      DHT22
#define RELAY_PIN    26
#define BUZZER_PIN   27

#define RELAY_ACTIVE_LOW true  // sesuaikan dengan modul relay Anda

// ======================= KALIBRASI SOIL MOISTURE =========================
int SOIL_RAW_DRY = 3300;   // nilai ADC saat sensor kering (udara)
int SOIL_RAW_WET = 1300;   // nilai ADC saat sensor dicelup air

// ======================= SETTINGS (default, bisa diubah dari dashboard) ==
int   soilDryThreshold      = 30;    // % -> di bawah ini pompa ON
int   soilWetThreshold      = 60;    // % -> di atas ini pompa OFF
int   soilCriticalThreshold = 20;    // % -> buzzer alarm aktif

bool  humidityRuleEnabled   = false; // aktifkan aturan tambahan berbasis humidity
float humidityMinThreshold  = 40.0;  // pompa dipaksa ON jika humidity di bawah ini

bool  tempRuleEnabled       = false; // aktifkan aturan tambahan berbasis temperature
float temperatureMaxThreshold = 32.0; // pompa dipaksa ON jika temperature di atas ini

// ======================= INTERVAL NON-BLOCKING ============================
const unsigned long SOIL_SAMPLE_INTERVAL = 100;
const unsigned long DHT_READ_INTERVAL    = 2000;
const unsigned long CONTROL_INTERVAL     = 500;
const unsigned long LCD_INTERVAL         = 2000;
const unsigned long BUZZER_BEEP_INTERVAL = 400;
const unsigned long HISTORY_LOG_INTERVAL = 5UL * 60UL * 1000UL; // catat tiap 5 menit

// ======================= OBJEK GLOBAL ======================================
WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Preferences prefs;

// ======================= STATE SENSOR =======================================
#define SOIL_SAMPLES 15
int soilBuffer[SOIL_SAMPLES];
int soilBufferIndex = 0;
bool soilBufferFilled = false;
int soilRawFiltered = 0;
int soilPercent = 0;

float temperature = NAN;
float humidity = NAN;
bool dhtError = false;
int dhtFailCount = 0;
const int DHT_MAX_FAIL = 5;

// ======================= STATE KONTROL =======================================
bool pumpState = false;
bool autoMode  = true;
bool buzzerState = false;

// ======================= RIWAYAT SOIL (untuk grafik) ==========================
#define HISTORY_SIZE 60
struct HistoryPoint { time_t timestamp; int soilPercent; };
HistoryPoint soilHistory[HISTORY_SIZE];
int historyIndex = 0;
bool historyFilled = false;
unsigned long lastHistoryLog = 0;

// ======================= RIWAYAT PENYIRAMAN ====================================
#define MAX_WATERING_EVENTS 10
struct WateringEvent { time_t startTime; time_t endTime; };
WateringEvent wateringEvents[MAX_WATERING_EVENTS];
int wateringEventWriteIndex = 0;
int wateringEventCount = 0;

// ======================= TIMER ===============================================
unsigned long lastSoilSample   = 0;
unsigned long lastDhtRead      = 0;
unsigned long lastControlCheck = 0;
unsigned long lastLcdUpdate    = 0;
unsigned long lastBuzzerToggle = 0;
bool lcdPageToggle = false;
unsigned long lastLcdReinit = 0;
const unsigned long LCD_REINIT_INTERVAL = 3UL * 60UL * 1000UL; // reinit tiap 3 menit

// ======================= SESSION AUTH ==========================================
String sessionToken = "";
unsigned long sessionLastActive = 0;
bool sessionActive = false;

// =============================================================================
//                    HALAMAN HTML (Login & Dashboard)
// =============================================================================

const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Login - Smart Garden</title>
<script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="bg-gray-900 min-h-screen flex items-center justify-center px-4">
  <div class="bg-gray-800 border border-gray-700 rounded-2xl shadow-xl p-8 w-full max-w-sm">
    <div class="text-center mb-6">
      <div class="text-4xl mb-2">&#127793;</div>
      <h1 class="text-2xl font-bold text-white">Smart Garden</h1>
      <p class="text-gray-400 text-sm mt-1">Silakan login untuk mengakses dashboard</p>
    </div>
    <p id="errMsg" class="hidden text-red-400 bg-red-900/30 border border-red-700 rounded-lg px-3 py-2 text-sm mb-4">
      Password salah, silakan coba lagi.
    </p>
    <form method="POST" action="/login">
      <label class="block text-gray-300 text-sm mb-1">Password</label>
      <input type="password" name="password" required
        class="w-full mb-4 px-4 py-2 rounded-lg bg-gray-900 border border-gray-600 text-white focus:outline-none focus:ring-2 focus:ring-green-500" />
      <button type="submit"
        class="w-full bg-green-600 hover:bg-green-500 text-white font-semibold py-2 rounded-lg transition">
        Masuk
      </button>
    </form>
  </div>
  <script>
    const params = new URLSearchParams(window.location.search);
    if (params.get('error') === '1') document.getElementById('errMsg').classList.remove('hidden');
  </script>
</body>
</html>
)rawliteral";

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Garden Dashboard</title>
<script src="https://cdn.tailwindcss.com"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
</head>
<body class="bg-gray-900 text-gray-100 min-h-screen">
  <div class="max-w-5xl mx-auto px-4 py-6">

    <div class="flex justify-between items-center mb-4">
      <div>
        <h1 class="text-2xl md:text-3xl font-bold flex items-center gap-2">
          <span>&#127793;</span> Smart Garden Dashboard
        </h1>
        <p class="text-gray-400 text-sm" id="statusText">Menghubungkan...</p>
      </div>
      <a href="/logout" class="text-sm bg-gray-800 hover:bg-gray-700 border border-gray-700 px-4 py-2 rounded-lg">Logout</a>
    </div>

    <div id="dhtErrorBanner" class="hidden bg-red-900/40 border border-red-700 text-red-300 text-sm rounded-lg px-4 py-2 mb-4">
      &#9888;&#65039; Gagal membaca sensor DHT22. Menampilkan data terakhir yang valid.
    </div>
    <div id="saveToast" class="hidden bg-green-900/40 border border-green-700 text-green-300 text-sm rounded-lg px-4 py-2 mb-4">
      &#9989; Pengaturan berhasil disimpan.
    </div>

    <!-- Tabs -->
    <div class="flex gap-2 mb-5 border-b border-gray-800">
      <button class="tab-btn px-4 py-2 text-sm font-semibold border-b-2 border-green-500 text-white" data-tab="monitor">Monitoring</button>
      <button class="tab-btn px-4 py-2 text-sm font-semibold border-b-2 border-transparent text-gray-400" data-tab="history">Grafik & Riwayat</button>
      <button class="tab-btn px-4 py-2 text-sm font-semibold border-b-2 border-transparent text-gray-400" data-tab="settings">Pengaturan</button>
    </div>

    <!-- TAB: MONITORING -->
    <div id="tab-monitor" class="tab-content">
      <div class="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
        <div class="bg-gray-800 rounded-2xl p-4 border-l-4" id="soilCard">
          <p class="text-gray-400 text-xs uppercase mb-1">Soil Moisture</p>
          <p class="text-3xl font-bold" id="soilValue">--%</p>
        </div>
        <div class="bg-gray-800 rounded-2xl p-4 border-l-4 border-blue-500">
          <p class="text-gray-400 text-xs uppercase mb-1">Temperature</p>
          <p class="text-3xl font-bold" id="tempValue">-- &deg;C</p>
        </div>
        <div class="bg-gray-800 rounded-2xl p-4 border-l-4 border-blue-500">
          <p class="text-gray-400 text-xs uppercase mb-1">Humidity</p>
          <p class="text-3xl font-bold" id="humValue">--%</p>
        </div>
        <div class="bg-gray-800 rounded-2xl p-4 border-l-4 border-gray-500">
          <p class="text-gray-400 text-xs uppercase mb-1">Uptime</p>
          <p class="text-3xl font-bold" id="uptimeValue">--</p>
        </div>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div class="bg-gray-800 rounded-2xl p-5">
          <div class="flex justify-between items-center mb-4">
            <h2 class="font-semibold text-lg">Water Pump</h2>
            <span id="pumpBadge" class="px-3 py-1 rounded-full text-xs font-bold bg-gray-600">OFF</span>
          </div>
          <div class="flex gap-3">
            <button onclick="pumpControl('on')" class="flex-1 bg-green-600 hover:bg-green-500 py-2 rounded-lg font-semibold">Nyalakan</button>
            <button onclick="pumpControl('off')" class="flex-1 bg-red-600 hover:bg-red-500 py-2 rounded-lg font-semibold">Matikan</button>
          </div>
          <p class="text-gray-500 text-xs mt-3">*Kontrol manual hanya berlaku jika Auto Mode nonaktif.</p>
        </div>

        <div class="bg-gray-800 rounded-2xl p-5">
          <div class="flex justify-between items-center mb-4">
            <h2 class="font-semibold text-lg">Auto Mode</h2>
            <span id="autoBadge" class="px-3 py-1 rounded-full text-xs font-bold bg-gray-600">OFF</span>
          </div>
          <div class="flex gap-3">
            <button onclick="autoControl('on')" class="flex-1 bg-green-600 hover:bg-green-500 py-2 rounded-lg font-semibold">Aktifkan</button>
            <button onclick="autoControl('off')" class="flex-1 bg-yellow-600 hover:bg-yellow-500 py-2 rounded-lg font-semibold">Nonaktifkan</button>
          </div>
          <p class="text-gray-500 text-xs mt-3" id="ruleSummary">Threshold aktif akan tampil di sini.</p>
        </div>
      </div>
    </div>

    <!-- TAB: HISTORY -->
    <div id="tab-history" class="tab-content hidden">
      <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
        <div class="bg-gray-800 rounded-2xl p-4">
          <p class="text-gray-400 text-xs uppercase mb-1">Terakhir Disiram</p>
          <p class="text-xl font-bold" id="lastWatered">--</p>
        </div>
        <div class="bg-gray-800 rounded-2xl p-4">
          <p class="text-gray-400 text-xs uppercase mb-1">Estimasi Penyiraman Berikutnya</p>
          <p class="text-xl font-bold" id="nextWatered">--</p>
        </div>
      </div>

      <div class="bg-gray-800 rounded-2xl p-4 mb-4">
        <p class="text-gray-400 text-xs uppercase mb-2">Tren Soil Moisture (5 jam terakhir)</p>
        <canvas id="soilChart" height="110"></canvas>
      </div>

      <div class="bg-gray-800 rounded-2xl p-4">
        <p class="text-gray-400 text-xs uppercase mb-3">Riwayat Penyiraman Terakhir</p>
        <div id="eventList" class="space-y-2 text-sm text-gray-300">Memuat...</div>
      </div>
    </div>

    <!-- TAB: SETTINGS -->
    <div id="tab-settings" class="tab-content hidden">
      <div class="bg-gray-800 rounded-2xl p-5 space-y-6 max-w-xl">

        <div>
          <h3 class="font-semibold mb-2">Threshold Soil Moisture</h3>
          <label class="text-xs text-gray-400">Pompa ON jika soil di bawah (%)</label>
          <input type="number" id="setSoilDry" min="0" max="100" class="w-full mt-1 mb-3 px-3 py-2 rounded-lg bg-gray-900 border border-gray-600" />
          <label class="text-xs text-gray-400">Pompa OFF jika soil di atas (%)</label>
          <input type="number" id="setSoilWet" min="0" max="100" class="w-full mt-1 mb-3 px-3 py-2 rounded-lg bg-gray-900 border border-gray-600" />
          <label class="text-xs text-gray-400">Buzzer alarm jika soil di bawah (%)</label>
          <input type="number" id="setSoilCritical" min="0" max="100" class="w-full mt-1 px-3 py-2 rounded-lg bg-gray-900 border border-gray-600" />
        </div>

        <div>
          <div class="flex items-center justify-between">
            <h3 class="font-semibold">Aturan Tambahan: Humidity</h3>
            <input type="checkbox" id="setHumEnabled" class="w-5 h-5" />
          </div>
          <label class="text-xs text-gray-400">Paksa pompa ON jika humidity udara di bawah (%)</label>
          <input type="number" id="setHumMin" min="0" max="100" class="w-full mt-1 px-3 py-2 rounded-lg bg-gray-900 border border-gray-600" />
        </div>

        <div>
          <div class="flex items-center justify-between">
            <h3 class="font-semibold">Aturan Tambahan: Temperature</h3>
            <input type="checkbox" id="setTempEnabled" class="w-5 h-5" />
          </div>
          <label class="text-xs text-gray-400">Paksa pompa ON jika suhu di atas (&deg;C)</label>
          <input type="number" id="setTempMax" min="0" max="60" class="w-full mt-1 px-3 py-2 rounded-lg bg-gray-900 border border-gray-600" />
        </div>

        <p id="settingsErr" class="hidden text-red-400 text-sm"></p>
        <button onclick="saveSettings()" class="w-full bg-green-600 hover:bg-green-500 py-2 rounded-lg font-semibold">
          Simpan Pengaturan
        </button>
        <p class="text-gray-500 text-xs">Pengaturan tersimpan permanen di ESP32 (tidak hilang saat restart).</p>
      </div>
    </div>

    <p class="text-gray-600 text-xs text-center mt-8">Data diperbarui otomatis setiap 2 detik</p>
  </div>

<script>
let chart;

function initTabs() {
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => {
        b.classList.remove('border-green-500', 'text-white');
        b.classList.add('border-transparent', 'text-gray-400');
      });
      btn.classList.add('border-green-500', 'text-white');
      btn.classList.remove('border-transparent', 'text-gray-400');

      document.querySelectorAll('.tab-content').forEach(c => c.classList.add('hidden'));
      document.getElementById('tab-' + btn.dataset.tab).classList.remove('hidden');

      if (btn.dataset.tab === 'history') fetchHistory();
      if (btn.dataset.tab === 'settings') fetchSettings();
    });
  });
}

async function fetchData() {
  try {
    const res = await fetch('/data', { credentials: 'same-origin' });
    if (res.status === 401) { window.location.href = '/login'; return; }
    const data = await res.json();

    document.getElementById('soilValue').innerText = data.soil + '%';
    document.getElementById('tempValue').innerText = data.temperature.toFixed(1) + ' \u00B0C';
    document.getElementById('humValue').innerText = data.humidity.toFixed(1) + '%';
    document.getElementById('uptimeValue').innerText = formatUptime(data.uptime);

    const soilCard = document.getElementById('soilCard');
    soilCard.className = 'bg-gray-800 rounded-2xl p-4 border-l-4 ' + statusBorderColor(data.soil);

    const pumpBadge = document.getElementById('pumpBadge');
    pumpBadge.innerText = data.pump ? 'ON' : 'OFF';
    pumpBadge.className = 'px-3 py-1 rounded-full text-xs font-bold ' + (data.pump ? 'bg-green-600' : 'bg-gray-600');

    const autoBadge = document.getElementById('autoBadge');
    autoBadge.innerText = data.auto ? 'ON' : 'OFF';
    autoBadge.className = 'px-3 py-1 rounded-full text-xs font-bold ' + (data.auto ? 'bg-green-600' : 'bg-gray-600');

    document.getElementById('dhtErrorBanner').classList.toggle('hidden', !data.dhtError);
    document.getElementById('statusText').innerText = 'Terhubung \u2022 ' + statusLabel(data.soil);

    let ruleTxt = 'Soil < ' + data.soilDry + '% \u2192 ON, soil > ' + data.soilWet + '% \u2192 OFF.';
    if (data.humRule) ruleTxt += ' Humidity < ' + data.humMin + '% \u2192 paksa ON.';
    if (data.tempRule) ruleTxt += ' Suhu > ' + data.tempMax + '\u00B0C \u2192 paksa ON.';
    document.getElementById('ruleSummary').innerText = ruleTxt;
  } catch (e) {
    document.getElementById('statusText').innerText = 'Koneksi terputus, mencoba ulang...';
  }
}

function statusBorderColor(soil) {
  if (soil < 20) return 'border-red-500';
  if (soil < 30) return 'border-yellow-500';
  return 'border-green-500';
}
function statusLabel(soil) {
  if (soil < 20) return 'Kritis (sangat kering)';
  if (soil < 30) return 'Warning (kering)';
  return 'Normal';
}
function formatUptime(sec) {
  const h = Math.floor(sec / 3600), m = Math.floor((sec % 3600) / 60), s = sec % 60;
  return h + 'j ' + m + 'm ' + s + 'd';
}
function formatDateTime(unixSec) {
  if (!unixSec || unixSec <= 0) return '--';
  return new Date(unixSec * 1000).toLocaleString('id-ID');
}
function relativeTime(unixSec) {
  if (!unixSec || unixSec <= 0) return '';
  const diff = Math.floor(Date.now() / 1000) - unixSec;
  if (diff < 60) return 'baru saja';
  if (diff < 3600) return Math.floor(diff / 60) + ' menit lalu';
  if (diff < 86400) return Math.floor(diff / 3600) + ' jam lalu';
  return Math.floor(diff / 86400) + ' hari lalu';
}

async function pumpControl(action) {
  await fetch('/pump/' + action, { credentials: 'same-origin' });
  fetchData();
}
async function autoControl(action) {
  await fetch('/auto/' + action, { credentials: 'same-origin' });
  fetchData();
}

async function fetchHistory() {
  try {
    const res = await fetch('/history', { credentials: 'same-origin' });
    if (res.status === 401) { window.location.href = '/login'; return; }
    const data = await res.json();

    document.getElementById('lastWatered').innerText =
      data.lastWatered > 0 ? formatDateTime(data.lastWatered) + ' (' + relativeTime(data.lastWatered) + ')' : 'Belum ada data';
    document.getElementById('nextWatered').innerText =
      data.predictedNextSeconds >= 0
        ? formatDateTime(Math.floor(Date.now() / 1000) + data.predictedNextSeconds)
        : 'Belum bisa diprediksi';

    const labels = data.history.map(p => new Date(p.t * 1000).toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'}));
    const values = data.history.map(p => p.v);

    if (chart) chart.destroy();
    chart = new Chart(document.getElementById('soilChart'), {
      type: 'line',
      data: {
        labels: labels,
        datasets: [{
          label: 'Soil Moisture (%)',
          data: values,
          borderColor: '#22c55e',
          backgroundColor: 'rgba(34,197,94,0.15)',
          tension: 0.3,
          fill: true,
          pointRadius: 2
        }]
      },
      options: {
        responsive: true,
        scales: {
          y: { min: 0, max: 100, ticks: { color: '#9ca3af' }, grid: { color: '#374151' } },
          x: { ticks: { color: '#9ca3af' }, grid: { color: '#374151' } }
        },
        plugins: { legend: { labels: { color: '#e5e7eb' } } }
      }
    });

    const listEl = document.getElementById('eventList');
    if (!data.events.length) {
      listEl.innerHTML = '<p class="text-gray-500">Belum ada riwayat penyiraman.</p>';
    } else {
      listEl.innerHTML = data.events.slice().reverse().map(ev => {
        const durTxt = ev.end > 0 ? Math.round((ev.end - ev.start)) + ' detik' : 'sedang berjalan...';
        return '<div class="flex justify-between border-b border-gray-700 pb-2">' +
          '<span>' + formatDateTime(ev.start) + '</span>' +
          '<span class="text-gray-400">' + durTxt + '</span></div>';
      }).join('');
    }
  } catch (e) { console.error(e); }
}

async function fetchSettings() {
  try {
    const res = await fetch('/settings', { credentials: 'same-origin' });
    if (res.status === 401) { window.location.href = '/login'; return; }
    const s = await res.json();
    document.getElementById('setSoilDry').value = s.soilDry;
    document.getElementById('setSoilWet').value = s.soilWet;
    document.getElementById('setSoilCritical').value = s.soilCritical;
    document.getElementById('setHumEnabled').checked = s.humRule;
    document.getElementById('setHumMin').value = s.humMin;
    document.getElementById('setTempEnabled').checked = s.tempRule;
    document.getElementById('setTempMax').value = s.tempMax;
  } catch (e) { console.error(e); }
}

async function saveSettings() {
  const soilDry = parseInt(document.getElementById('setSoilDry').value);
  const soilWet = parseInt(document.getElementById('setSoilWet').value);
  const errEl = document.getElementById('settingsErr');

  if (isNaN(soilDry) || isNaN(soilWet) || soilDry >= soilWet) {
    errEl.innerText = 'Soil Dry harus lebih kecil dari Soil Wet.';
    errEl.classList.remove('hidden');
    return;
  }
  errEl.classList.add('hidden');

  const body = new URLSearchParams({
    soilDry: soilDry,
    soilWet: soilWet,
    soilCritical: document.getElementById('setSoilCritical').value,
    humRule: document.getElementById('setHumEnabled').checked ? '1' : '0',
    humMin: document.getElementById('setHumMin').value,
    tempRule: document.getElementById('setTempEnabled').checked ? '1' : '0',
    tempMax: document.getElementById('setTempMax').value
  });

  const res = await fetch('/settings', { method: 'POST', credentials: 'same-origin', body: body });
  if (res.status === 401) { window.location.href = '/login'; return; }

  const toast = document.getElementById('saveToast');
  toast.classList.remove('hidden');
  setTimeout(() => toast.classList.add('hidden'), 2500);
  fetchData();
}

initTabs();
fetchData();
setInterval(fetchData, 2000);
</script>
</body>
</html>
)rawliteral";

// =============================================================================
//                    HELPER: AUTH / SESSION
// =============================================================================

String generateToken() {
  return String(millis()) + "-" + String(random(100000, 999999));
}

String getCookieValue() {
  if (!server.hasHeader("Cookie")) return "";
  String cookie = server.header("Cookie");
  int idx = cookie.indexOf("session=");
  if (idx == -1) return "";
  int start = idx + 8;
  int end = cookie.indexOf(';', start);
  if (end == -1) end = cookie.length();
  return cookie.substring(start, end);
}

bool isAuthenticated() {
  if (!sessionActive) return false;
  if (getCookieValue() != sessionToken) return false;
  if (millis() - sessionLastActive > SESSION_TIMEOUT_MS) { sessionActive = false; return false; }
  sessionLastActive = millis();
  return true;
}

void sendUnauthorizedJson() { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); }

// =============================================================================
//                    HANDLER: LOGIN / DASHBOARD
// =============================================================================

void handleRoot() {
  if (!isAuthenticated()) { server.sendHeader("Location", "/login"); server.send(302, "text/plain", ""); return; }
  server.send_P(200, "text/html", DASHBOARD_HTML);
}
void handleLoginPage() { server.send_P(200, "text/html", LOGIN_HTML); }

void handleLoginPost() {
  if (server.arg("password") == DASHBOARD_PASSWORD) {
    sessionToken = generateToken();
    sessionLastActive = millis();
    sessionActive = true;
    server.sendHeader("Set-Cookie", "session=" + sessionToken + "; Path=/; HttpOnly");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  } else {
    server.sendHeader("Location", "/login?error=1");
    server.send(302, "text/plain", "");
  }
}

void handleLogout() {
  sessionActive = false;
  sessionToken = "";
  server.sendHeader("Set-Cookie", "session=deleted; Path=/; Max-Age=0");
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "");
}

// =============================================================================
//                    HANDLER: DATA REALTIME
// =============================================================================

void handleData() {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }
  String json = "{";
  json += "\"soil\":" + String(soilPercent) + ",";
  json += "\"temperature\":" + String(isnan(temperature) ? 0 : temperature, 1) + ",";
  json += "\"humidity\":" + String(isnan(humidity) ? 0 : humidity, 1) + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"auto\":" + String(autoMode ? "true" : "false") + ",";
  json += "\"dhtError\":" + String(dhtError ? "true" : "false") + ",";
  json += "\"soilDry\":" + String(soilDryThreshold) + ",";
  json += "\"soilWet\":" + String(soilWetThreshold) + ",";
  json += "\"humRule\":" + String(humidityRuleEnabled ? "true" : "false") + ",";
  json += "\"humMin\":" + String(humidityMinThreshold, 1) + ",";
  json += "\"tempRule\":" + String(tempRuleEnabled ? "true" : "false") + ",";
  json += "\"tempMax\":" + String(temperatureMaxThreshold, 1) + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";
  server.send(200, "application/json", json);
}

// =============================================================================
//                    HANDLER: KONTROL POMPA / AUTO MODE
// =============================================================================

void handlePumpOn()  {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }
  DBG_PRINTLN("[HTTP] /pump/on dipanggil, autoMode=" + String(autoMode ? "true" : "false"));
  autoMode = false;        // klik manual otomatis keluar dari Auto Mode
  setPump(true);
  server.send(200, "application/json", "{\"ok\":true}");
}
void handlePumpOff() {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }
  DBG_PRINTLN("[HTTP] /pump/off dipanggil, autoMode=" + String(autoMode ? "true" : "false"));
  autoMode = false;        // klik manual otomatis keluar dari Auto Mode
  setPump(false);
  server.send(200, "application/json", "{\"ok\":true}");
}
void handleAutoOn()  { if (!isAuthenticated()) { sendUnauthorizedJson(); return; } autoMode = true;  server.send(200, "application/json", "{\"ok\":true}"); }
void handleAutoOff() { if (!isAuthenticated()) { sendUnauthorizedJson(); return; } autoMode = false; server.send(200, "application/json", "{\"ok\":true}"); }

// =============================================================================
//                    HANDLER: SETTINGS
// =============================================================================

void handleSettingsGet() {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }
  String json = "{";
  json += "\"soilDry\":" + String(soilDryThreshold) + ",";
  json += "\"soilWet\":" + String(soilWetThreshold) + ",";
  json += "\"soilCritical\":" + String(soilCriticalThreshold) + ",";
  json += "\"humRule\":" + String(humidityRuleEnabled ? "true" : "false") + ",";
  json += "\"humMin\":" + String(humidityMinThreshold, 1) + ",";
  json += "\"tempRule\":" + String(tempRuleEnabled ? "true" : "false") + ",";
  json += "\"tempMax\":" + String(temperatureMaxThreshold, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSettingsPost() {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }

  int newDry  = server.arg("soilDry").toInt();
  int newWet  = server.arg("soilWet").toInt();
  if (newDry <= 0 || newWet <= 0 || newDry >= newWet || newWet > 100) {
    server.send(400, "application/json", "{\"error\":\"invalid threshold\"}");
    return;
  }

  soilDryThreshold        = newDry;
  soilWetThreshold        = newWet;
  soilCriticalThreshold   = server.arg("soilCritical").toInt();
  humidityRuleEnabled     = server.arg("humRule") == "1";
  humidityMinThreshold    = server.arg("humMin").toFloat();
  tempRuleEnabled         = server.arg("tempRule") == "1";
  temperatureMaxThreshold = server.arg("tempMax").toFloat();

  saveSettingsToFlash();
  server.send(200, "application/json", "{\"ok\":true}");
}

// =============================================================================
//                    HANDLER: RIWAYAT & GRAFIK
// =============================================================================

void handleHistory() {
  if (!isAuthenticated()) { sendUnauthorizedJson(); return; }

  time_t lastWatered = 0;
  if (wateringEventCount > 0) {
    int lastIdx = (wateringEventWriteIndex - 1 + MAX_WATERING_EVENTS) % MAX_WATERING_EVENTS;
    lastWatered = wateringEvents[lastIdx].startTime;
  }

  long predictedNext = predictNextWateringSeconds();

  String json = "{";
  json += "\"lastWatered\":" + String((long)lastWatered) + ",";
  json += "\"predictedNextSeconds\":" + String(predictedNext) + ",";

  json += "\"history\":[";
  int count = historyFilled ? HISTORY_SIZE : historyIndex;
  int startIdx = historyFilled ? historyIndex : 0;
  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i) % HISTORY_SIZE;
    if (soilHistory[idx].timestamp == 0) continue;
    json += "{\"t\":" + String((long)soilHistory[idx].timestamp) + ",\"v\":" + String(soilHistory[idx].soilPercent) + "}";
    if (i < count - 1) json += ",";
  }
  json += "],";

  json += "\"events\":[";
  int evCount = wateringEventCount;
  for (int i = 0; i < evCount; i++) {
    int idx = (wateringEventWriteIndex - evCount + i + MAX_WATERING_EVENTS) % MAX_WATERING_EVENTS;
    json += "{\"start\":" + String((long)wateringEvents[idx].startTime) + ",\"end\":" + String((long)wateringEvents[idx].endTime) + "}";
    if (i < evCount - 1) json += ",";
  }
  json += "]";
  json += "}";

  server.send(200, "application/json", json);
}

void handleNotFound() { server.send(404, "text/plain", "404 Not Found"); }

// =============================================================================
//                    FUNGSI KONTROL AKTUATOR
// =============================================================================

void setPump(bool on) {
  bool changed = (on != pumpState);
  pumpState = on;
  bool relayLevel = RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
  digitalWrite(RELAY_PIN, relayLevel);

  DBG_PRINTF("[PUMP] setPump(%s) | changed=%s | RELAY_PIN=%d -> %s | autoMode=%s\n",
                on ? "true" : "false",
                changed ? "yes" : "no",
                RELAY_PIN,
                relayLevel == HIGH ? "HIGH" : "LOW",
                autoMode ? "true" : "false");

  if (changed) {
    if (on) recordWateringStart();
    else recordWateringEnd();
  }
}

void setBuzzer(bool on) { buzzerState = on; digitalWrite(BUZZER_PIN, on ? HIGH : LOW); }

// =============================================================================
//                    RIWAYAT PENYIRAMAN & SOIL HISTORY
// =============================================================================

void recordWateringStart() {
  wateringEvents[wateringEventWriteIndex].startTime = time(nullptr);
  wateringEvents[wateringEventWriteIndex].endTime = 0;
  wateringEventWriteIndex = (wateringEventWriteIndex + 1) % MAX_WATERING_EVENTS;
  if (wateringEventCount < MAX_WATERING_EVENTS) wateringEventCount++;
}

void recordWateringEnd() {
  if (wateringEventCount == 0) return;
  int lastIdx = (wateringEventWriteIndex - 1 + MAX_WATERING_EVENTS) % MAX_WATERING_EVENTS;
  wateringEvents[lastIdx].endTime = time(nullptr);
}

void logSoilHistory() {
  soilHistory[historyIndex].timestamp = time(nullptr);
  soilHistory[historyIndex].soilPercent = soilPercent;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyIndex == 0) historyFilled = true;
}

// Estimasi sederhana (linear, dua titik) kapan soil moisture akan menyentuh
// soilDryThreshold berdasarkan tren penurunan pada data historis.
// Mengembalikan -1 jika data belum cukup atau tren tidak menurun.
long predictNextWateringSeconds() {
  int count = historyFilled ? HISTORY_SIZE : historyIndex;
  if (count < 2) return -1;

  int oldestIdx = historyFilled ? historyIndex : 0;
  int newestIdx = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;

  HistoryPoint oldest = soilHistory[oldestIdx];
  HistoryPoint newest = soilHistory[newestIdx];
  if (oldest.timestamp == 0 || newest.timestamp == 0) return -1;

  long dt = (long)(newest.timestamp - oldest.timestamp);
  int dv = newest.soilPercent - oldest.soilPercent;
  if (dt <= 0 || dv >= 0) return -1; // tidak menurun -> tidak diprediksi

  float ratePerSecond = (float)dv / (float)dt; // negatif
  float percentToDry = newest.soilPercent - soilDryThreshold;
  if (percentToDry <= 0) return 0;

  return (long)(percentToDry / (-ratePerSecond));
}

// =============================================================================
//                    SETTINGS: LOAD/SAVE KE FLASH (NVS)
// =============================================================================

void loadSettingsFromFlash() {
  prefs.begin("garden", true);
  soilDryThreshold        = prefs.getInt("soilDry", 30);
  soilWetThreshold        = prefs.getInt("soilWet", 60);
  soilCriticalThreshold   = prefs.getInt("soilCrit", 20);
  humidityRuleEnabled     = prefs.getBool("humEn", false);
  humidityMinThreshold    = prefs.getFloat("humMin", 40.0);
  tempRuleEnabled         = prefs.getBool("tempEn", false);
  temperatureMaxThreshold = prefs.getFloat("tempMax", 32.0);
  prefs.end();
}

void saveSettingsToFlash() {
  prefs.begin("garden", false);
  prefs.putInt("soilDry", soilDryThreshold);
  prefs.putInt("soilWet", soilWetThreshold);
  prefs.putInt("soilCrit", soilCriticalThreshold);
  prefs.putBool("humEn", humidityRuleEnabled);
  prefs.putFloat("humMin", humidityMinThreshold);
  prefs.putBool("tempEn", tempRuleEnabled);
  prefs.putFloat("tempMax", temperatureMaxThreshold);
  prefs.end();
}

// =============================================================================
//                    PEMBACAAN SENSOR
// =============================================================================

void sampleSoilSensor() {
  int raw = analogRead(SOIL_PIN);
  soilBuffer[soilBufferIndex] = raw;
  soilBufferIndex = (soilBufferIndex + 1) % SOIL_SAMPLES;
  if (soilBufferIndex == 0) soilBufferFilled = true;

  long sum = 0;
  int count = soilBufferFilled ? SOIL_SAMPLES : soilBufferIndex;
  if (count == 0) return;
  for (int i = 0; i < count; i++) sum += soilBuffer[i];
  soilRawFiltered = sum / count;

  int percent = map(soilRawFiltered, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  soilPercent = constrain(percent, 0, 100);
}

void readDhtSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    dhtFailCount++;
    if (dhtFailCount >= DHT_MAX_FAIL) dhtError = true;
  } else {
    dhtFailCount = 0;
    dhtError = false;
    temperature = t;
    humidity = h;
  }
}

// =============================================================================
//                    LOGIKA KONTROL OTOMATIS
// =============================================================================

void updateControlLogic() {
  bool forceOn = false;
  if (humidityRuleEnabled && !isnan(humidity) && humidity < humidityMinThreshold) forceOn = true;
  if (tempRuleEnabled && !isnan(temperature) && temperature > temperatureMaxThreshold) forceOn = true;

  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 5000) {
    lastDebugPrint = millis();
    DBG_PRINTF("[AUTO] soil=%d%% (dry<%d, wet>%d) forceOn=%s pumpState=%s autoMode=%s\n",
                  soilPercent, soilDryThreshold, soilWetThreshold,
                  forceOn ? "true" : "false",
                  pumpState ? "ON" : "OFF",
                  autoMode ? "true" : "false");
  }

  if (autoMode) {
    if ((soilPercent < soilDryThreshold || forceOn) && !pumpState) {
      DBG_PRINTLN("[AUTO] Kondisi ON terpenuhi -> setPump(true)");
      setPump(true);
    } else if (soilPercent > soilWetThreshold && !forceOn && pumpState) {
      DBG_PRINTLN("[AUTO] Kondisi OFF terpenuhi -> setPump(false)");
      setPump(false);
    }
  }

  bool shouldBuzz = (soilPercent < soilCriticalThreshold);
  if (shouldBuzz) {
    if (millis() - lastBuzzerToggle > BUZZER_BEEP_INTERVAL) {
      lastBuzzerToggle = millis();
      setBuzzer(!buzzerState);
    }
  } else if (buzzerState) {
    setBuzzer(false);
  }
}

// =============================================================================
//                    LCD DISPLAY
// =============================================================================

void updateLcd() {
  lcd.clear();
  if (!lcdPageToggle) {
    lcd.setCursor(0, 0);
    lcd.print("Soil:" + String(soilPercent) + "%");
    lcd.setCursor(0, 1);
    if (dhtError) lcd.print("DHT ERROR!");
    else lcd.print("T:" + String(temperature, 1) + "C H:" + String(humidity, 0) + "%");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Pump:");
    lcd.print(pumpState ? "ON " : "OFF");
    lcd.print(" ");
    lcd.print(autoMode ? "AUTO" : "MAN");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "WiFi: N/A");
  }
  lcdPageToggle = !lcdPageToggle;
}

// =============================================================================
//                    SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(35));

  // Wajib: WebServer ESP32 secara default tidak menyimpan header Cookie.
  // Tanpa ini, server.hasHeader("Cookie") akan selalu false sehingga
  // login tidak akan pernah terautentikasi meski password benar.
  const char* headerKeys[] = { "Cookie" };
  server.collectHeaders(headerKeys, 1);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setPump(false);
  digitalWrite(BUZZER_PIN, LOW);

  loadSettingsFromFlash();

  dht.begin();

  Wire.begin(21, 22);
  Wire.setClock(100000); // turunkan ke 100kHz, lebih tahan noise drpd default 400kHz
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Garden");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(250); // hanya sekali saat boot
    DBG_PRINT(".");
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    DBG_PRINTLN("\nWiFi terhubung: " + WiFi.localIP().toString());
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
      DBG_PRINTLN("Waktu tersinkron via NTP.");
    } else {
      DBG_PRINTLN("Gagal sinkronisasi NTP, riwayat memakai waktu default.");
    }
  } else {
    DBG_PRINTLN("\nWiFi GAGAL terhubung.");
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED");
  }

  if (MDNS.begin("smartgarden")) DBG_PRINTLN("mDNS aktif: http://smartgarden.local");

  for (int i = 0; i < SOIL_SAMPLES; i++) soilBuffer[i] = analogRead(SOIL_PIN);
  soilBufferFilled = true;
  sampleSoilSensor();
  readDhtSensor();

  server.on("/", handleRoot);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLoginPost);
  server.on("/logout", handleLogout);
  server.on("/data", HTTP_GET, handleData);
  server.on("/pump/on", HTTP_GET, handlePumpOn);
  server.on("/pump/off", HTTP_GET, handlePumpOff);
  server.on("/auto/on", HTTP_GET, handleAutoOn);
  server.on("/auto/off", HTTP_GET, handleAutoOff);
  server.on("/settings", HTTP_GET, handleSettingsGet);
  server.on("/settings", HTTP_POST, handleSettingsPost);
  server.on("/history", HTTP_GET, handleHistory);
  server.onNotFound(handleNotFound);

  server.begin();
  DBG_PRINTLN("Web server dimulai di port 80");
  DBG_PRINT("Akses dashboard di: http://");
  DBG_PRINT(WiFi.localIP());
  DBG_PRINTLN(":80");
}

// =============================================================================
//                    LOOP (NON-BLOCKING)
// =============================================================================

void loop() {
  server.handleClient();
  unsigned long now = millis();

  if (now - lastSoilSample >= SOIL_SAMPLE_INTERVAL) { lastSoilSample = now; sampleSoilSensor(); }
  if (now - lastDhtRead >= DHT_READ_INTERVAL)       { lastDhtRead = now; readDhtSensor(); }
  if (now - lastControlCheck >= CONTROL_INTERVAL)   { lastControlCheck = now; updateControlLogic(); }
  if (now - lastLcdUpdate >= LCD_INTERVAL)          { lastLcdUpdate = now; updateLcd(); }
  if (now - lastHistoryLog >= HISTORY_LOG_INTERVAL) { lastHistoryLog = now; logSoilHistory(); }

  // Recovery periodik: re-init LCD tiap beberapa menit.
  // I2C expander (PCF8574) di modul LCD kadang state-nya korup gara-gara
  // noise dari switching relay/buzzer, dan begitu korup dia stuck garbled
  // sampai di-reset ulang. Reinit ringan ini "menyegarkan" LCD tanpa reboot ESP32.
  if (now - lastLcdReinit >= LCD_REINIT_INTERVAL) {
    lastLcdReinit = now;
    lcd.init();
    lcd.backlight();
  }
}

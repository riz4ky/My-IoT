/*
 * Sistem Pendeteksi Suhu, Kelembapan, dan Gas Berbasis Web
 * Komponen: ESP32, DHT22, MQ2, LCD 16x2 I2C, Buzzer, LED
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================= PENGATURAN WIFI =================
const char* ssid = "LANTAI 2";       // Ganti dengan nama WiFi di rumahmu
const char* password = "lantai2*"; // Ganti dengan password WiFi

// ================= PENGATURAN PIN =================
#define DHTPIN 4        // Pin data DHT22 terhubung ke GPIO4
#define DHTTYPE DHT22   // Mendefinisikan tipe sensor DHT
#define MQ2PIN 34       // Pin analog MQ2 terhubung ke GPIO34
#define BUZZER_PIN 5    // Pin positif Buzzer terhubung ke GPIO5
#define LED_PIN 18      // Pin positif LED terhubung ke GPIO18

// ================= INISIALISASI ===================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 
WebServer server(80); // Membuat web server pada port 80

// Variabel Global
const int batasGasBahaya = 1500; 
float suhu = 0.0;
float kelembapan = 0.0;
int nilaiGas = 0;
bool statusBahaya = false;

// Waktu untuk mengatur interval pembacaan sensor tanpa menghentikan Web Server
unsigned long waktuSebelumnya = 0;
const long intervalBaca = 2000; 

// ================= HALAMAN DASHBOARD HTML =================
// Kode HTML dan CSS disimpan di dalam memori program ESP32
const char halaman_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>Dashboard Detektor</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #eef2f3; margin: 0; padding: 20px; }
    h1 { color: #2c3e50; margin-bottom: 30px; }
    .container { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }
    .card { background: white; padding: 30px 20px; border-radius: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); width: 260px; transition: 0.3s; }
    .card:hover { transform: translateY(-5px); }
    .ikon { font-size: 3em; margin-bottom: 10px; }
    .nilai { font-size: 2.5em; font-weight: bold; color: #3498db; margin: 10px 0; }
    .judul-kartu { font-size: 1.2em; color: #7f8c8d; text-transform: uppercase; letter-spacing: 1px; }
    .bahaya { color: #e74c3c !important; font-weight: bold; }
    .aman { color: #2ecc71 !important; font-weight: bold; }
    
    /* Animasi kelap-kelip untuk status bahaya */
    @keyframes kedip { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
    .kedip-bahaya { animation: kedip 1s infinite; color: #e74c3c; }
  </style>
</head>
<body>
  <h1>🖥️ Dashboard Pemantauan Lingkungan</h1>
  
  <div class="container">
    <div class="card">
      <div class="ikon">🌡️</div>
      <div class="judul-kartu">Suhu Ruangan</div>
      <div class="nilai" id="suhu">-- &deg;C</div>
    </div>
    
    <div class="card">
      <div class="ikon">💧</div>
      <div class="judul-kartu">Kelembapan</div>
      <div class="nilai" id="kelembapan">-- %</div>
    </div>
    
    <div class="card" id="kartu_gas">
      <div class="ikon">💨</div>
      <div class="judul-kartu">Kadar Gas/Asap</div>
      <div class="nilai" id="gas">--</div>
      <div class="judul-kartu" id="status_gas">Memuat...</div>
    </div>
  </div>

  <script>
    // Meminta data dari ESP32 setiap 2 detik (2000 ms) tanpa me-refresh halaman
    setInterval(function() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('suhu').innerHTML = data.suhu + ' &deg;C';
          document.getElementById('kelembapan').innerHTML = data.kelembapan + ' %';
          document.getElementById('gas').innerHTML = data.gas;

          let statusGas = document.getElementById('status_gas');
          let kartuGas = document.getElementById('kartu_gas');
          
          if(data.bahaya) {
            statusGas.innerHTML = "⚠️ BAHAYA! KEBOCORAN!";
            statusGas.className = "kedip-bahaya";
            document.getElementById('gas').className = "nilai bahaya";
          } else {
            statusGas.innerHTML = "✅ Kondisi Aman";
            statusGas.className = "aman";
            document.getElementById('gas').className = "nilai aman";
          }
        });
    }, 2000);
  </script>
</body>
</html>
)rawliteral";

// Fungsi untuk mengirim halaman HTML utama
void tanganiHalamanUtama() {
  server.send(200, "text/html", halaman_html);
}

// Fungsi untuk mengirim data sensor dalam format JSON
void tanganiDataJSON() {
  String json = "{";
  json += "\"suhu\":\"" + String(suhu, 1) + "\",";
  json += "\"kelembapan\":\"" + String(kelembapan, 1) + "\",";
  json += "\"gas\":" + String(nilaiGas) + ",";
  json += "\"bahaya\":" + String(statusBahaya ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  
  // Mengatur mode pin untuk LED dan Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Matikan buzzer di awal
  digitalWrite(LED_PIN, LOW);    // Matikan LED di awal
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Deteksi");
  
  // Inisialisasi Sensor DHT22
  dht.begin();

  // Menghubungkan ke WiFi
  lcd.setCursor(0, 1);
  lcd.print("Koneksi WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi Terhubung!");
  Serial.print("Alamat IP Web Server: ");
  Serial.println(WiFi.localIP());

  // Tampilkan IP Address di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(4000); // Tampilkan IP selama 4 detik
  lcd.clear();

  // Mengatur Routing Web Server
  server.on("/", tanganiHalamanUtama); // Rute utama
  server.on("/data", tanganiDataJSON); // Rute data sensor
  
  // Mulai Server
  server.begin();
  Serial.println("Web Server mulai berjalan.");
}

void loop() {
  // Wajib ada di loop agar server terus melayani permintaan (request) dari browser
  server.handleClient(); 

  // Membaca sensor menggunakan metode millis() agar tidak menghentikan web server (pengganti delay)
  unsigned long waktuSekarang = millis();
  
  if (waktuSekarang - waktuSebelumnya >= intervalBaca) {
    waktuSebelumnya = waktuSekarang;

    // 1. Membaca Sensor
    kelembapan = dht.readHumidity();
    suhu = dht.readTemperature();
    nilaiGas = analogRead(MQ2PIN);

    // 2. Cek apakah pembacaan DHT gagal
    if (isnan(kelembapan) || isnan(suhu)) {
      Serial.println("Gagal membaca DHT22!");
      suhu = 0;
      kelembapan = 0;
    }

    // 3. Logika Deteksi Bahaya (LED dan Buzzer)
    if (nilaiGas > batasGasBahaya) {
      statusBahaya = true;
      digitalWrite(BUZZER_PIN, HIGH); // Nyalakan Alarm Suara
      digitalWrite(LED_PIN, HIGH);    // Nyalakan Lampu Merah
    } else {
      statusBahaya = false;
      digitalWrite(BUZZER_PIN, LOW);  // Matikan Alarm
      digitalWrite(LED_PIN, LOW);     // Matikan Lampu
    }

    // 4. Update Tampilan Layar LCD
    lcd.setCursor(0, 0);
    lcd.print("S:");
    lcd.print(suhu, 1);
    lcd.print("C K:");
    lcd.print(kelembapan, 1);
    lcd.print("% ");

    lcd.setCursor(0, 1);
    lcd.print("Gas:");
    lcd.print(nilaiGas);
    lcd.print(" "); 
    
    if (statusBahaya) {
      lcd.print("BAHAYA!");
    } else {
      lcd.print("Aman   "); 
    }
  }
}
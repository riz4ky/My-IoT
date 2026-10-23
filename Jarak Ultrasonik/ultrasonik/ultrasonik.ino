#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- DEKLARASI PIN ---
#define DHTPIN 4
#define DHTTYPE DHT22 // Sensor DHT22 (Warna Putih)
#define SOIL_PIN 34   // Sensor Kelembapan Tanah
#define LED_PIN 14    // LED Merah (Indikator)
#define BUZZER_PIN 27 // Buzzer (Alarm)
#define RELAY_PIN 26  // Relay Pompa Air (Sesuai skema awal)

// --- LOGIKA RELAY ---
// Kebanyakan modul relay Arduino/ESP32 aktif saat diberi sinyal LOW (Active Low)
// Jika pompa kamu malah mati saat harusnya menyala, silakan tukar nilai HIGH dan LOW di bawah ini
#define RELAY_ON LOW 
#define RELAY_OFF HIGH

// --- OBJEK SENSOR & MODUL ---
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C umum adalah 0x27

// --- VARIABEL GLOBAL ---
int kelembapanPercent = 0;
float suhu = 0;

// Variabel untuk pengaturan waktu (Non-blocking Timer)
unsigned long waktuSebelumnyaSensor = 0;
const long intervalSensor = 2000; // Baca sensor setiap 2 detik (2000 milidetik)

unsigned long waktuSebelumnyaAlarm = 0;
bool statusBlink = false;

// Nilai kalibrasi sensor tanah (bisa disesuaikan nanti)
const int nilaiKeringAir = 4095; // Nilai saat kering (di udara)
const int nilaiBasahAir = 1500;  // Nilai saat basah (di dalam air)

void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin Output
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Pastikan semua output mati saat pertama kali dinyalakan
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Garden");
  lcd.setCursor(0, 1);
  lcd.print("Memulai Alat...");
  delay(2000);
  lcd.clear();

  // Inisialisasi Sensor Suhu
  dht.begin();
}

void loop() {
  unsigned long waktuSekarang = millis();

  // Membaca sensor setiap 2 detik tanpa menghentikan program (Non-blocking)
  if (waktuSekarang - waktuSebelumnyaSensor >= intervalSensor) {
    waktuSebelumnyaSensor = waktuSekarang;
    bacaSensor();
  }

  // Menjalankan logika LED, Buzzer, dan Pompa berdasarkan kelembapan tanah
  jalankanSistem();
}

void bacaSensor() {
  // Membaca suhu dari DHT22
  suhu = dht.readTemperature();
  
  // Membaca nilai analog dari kelembapan tanah
  int nilaiAnalogTanah = analogRead(SOIL_PIN);
  
  // Konversi nilai analog ke persen (0 - 100%)
  kelembapanPercent = map(nilaiAnalogTanah, nilaiKeringAir, nilaiBasahAir, 0, 100);
  
  // Batasi agar nilainya tidak melompat di bawah 0% atau di atas 100%
  kelembapanPercent = constrain(kelembapanPercent, 0, 100);

  // Tampilkan data ke Serial Monitor untuk keperluan debugging/pemantauan di PC
  Serial.print("Kelembapan Tanah: ");
  Serial.print(kelembapanPercent);
  Serial.print("% | Suhu: ");
  if (isnan(suhu)) {
    Serial.println("Error membaca suhu!");
  } else {
    Serial.print(suhu);
    Serial.println(" C");
  }

  // Update Tampilan pada Layar LCD 16x2
  lcd.setCursor(0, 0);
  lcd.print("Lembap: ");
  lcd.print(kelembapanPercent);
  lcd.print("%   "); // Spasi tambahan untuk menghapus sisa angka sebelumnya
  
  lcd.setCursor(0, 1);
  lcd.print("Suhu  : ");
  if (isnan(suhu)) {
    lcd.print("Err ");
  } else {
    lcd.print(suhu, 1); // Menampilkan suhu dengan 1 angka di belakang koma
    lcd.print("C  ");
  }
}

void jalankanSistem() {
  unsigned long waktuSekarang = millis();

  // KONDISI 1: Tanah Kering (Kelembapan < 30%)
  if (kelembapanPercent < 30) {
    digitalWrite(RELAY_PIN, RELAY_ON); // Pompa menyala otomatis
    digitalWrite(LED_PIN, HIGH);       // LED Merah menyala diam (statis)

    // Buzzer berbunyi putus-putus (intermiten) setiap 500ms
    if (waktuSekarang - waktuSebelumnyaAlarm >= 500) {
      waktuSebelumnyaAlarm = waktuSekarang;
      statusBlink = !statusBlink;
      digitalWrite(BUZZER_PIN, statusBlink ? HIGH : LOW);
    }
  } 
  
  // KONDISI 2: Batas Aman (Kelembapan >= 50% dan < 95%)
  else if (kelembapanPercent >= 50 && kelembapanPercent < 95) {
    digitalWrite(RELAY_PIN, RELAY_OFF); // Pompa berhenti menyala
    digitalWrite(LED_PIN, LOW);         // LED Mati
    digitalWrite(BUZZER_PIN, LOW);      // Buzzer Mati
  }
  
  // KONDISI 3: Tanah Terlalu Basah / Banjir (Kelembapan >= 95%)
  else if (kelembapanPercent >= 95) {
    digitalWrite(RELAY_PIN, RELAY_OFF); // Pompa tetap mati (mencegah banjir lebih parah)
    digitalWrite(BUZZER_PIN, HIGH);     // Buzzer berbunyi konstan (menyala terus-menerus)

    // LED Merah berkedip cepat setiap 100ms
    if (waktuSekarang - waktuSebelumnyaAlarm >= 100) {
      waktuSebelumnyaAlarm = waktuSekarang;
      statusBlink = !statusBlink;
      digitalWrite(LED_PIN, statusBlink ? HIGH : LOW);
    }
  }
  
  // KONDISI 4: Rentang transisi (30% s.d. 49%)
  // Pada kondisi ini, jika pompa sudah menyala sejak kondisi <30%, pompa dibiarkan TETAP MENYALA
  // sampai kelembapan menyentuh batas aman 50%. 
  // Namun, alarm buzzer dan LED dimatikan agar tidak bising selama proses penyiraman berlangsung.
  else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}
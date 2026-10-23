#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD I2C dengan alamat 0x27, ukuran 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === PIN SENSOR ULTRASONIK ===
#define PIN_TRIG 18
#define PIN_ECHO 19

// === PIN OUTPUT ===
#define PIN_BUZZER 23

// Variabel untuk menyimpan data waktu dan jarak
long durasi;
int jarak;

void setup() {
  Serial.begin(115200); // Baud rate ESP32
  
  // Inisialisasi LCD I2C
  lcd.init();
  lcd.backlight();
  
  // Tampilan awal di LCD
  lcd.setCursor(0, 0);
  lcd.print("  Sistem Jarak  ");
  lcd.setCursor(0, 1);
  lcd.print("   ESP32 Aktif  ");
  delay(2000);
  lcd.clear();
  
  // Pengaturan Pin Mode
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Pastikan buzzer mati di awal
  digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
  // === 1. MEMBACA SENSOR ULTRASONIK ===
  // Bersihkan trigger pin
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  
  // Aktifkan trigger selama 10 mikrodetik
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  // Baca pantulan gelombang (durasi dalam mikrodetik)
  durasi = pulseIn(PIN_ECHO, HIGH);
  
  // Hitung jarak dalam centimeter
  // Rumus: Kecepatan suara (0.034 cm/us) dikali waktu dibagi 2 (bolak-balik)
  jarak = durasi * 0.034 / 2;

  // Mengatasi pembacaan error di luar jangkauan sensor HC-SR04
  if (jarak <= 0 || jarak > 400) {
    jarak = 400; 
  }

  // === 2. TAMPILAN LCD ===
  lcd.setCursor(0, 0);
  lcd.print("Jarak: ");
  lcd.print(jarak);
  lcd.print(" cm   "); // Spasi tambahan untuk menghapus sisa angka sebelumnya

  lcd.setCursor(0, 1);
  
  // === 3. LOGIKA ALARM DAN STATUS ===
  if (jarak < 20) {
    // KONDISI BAHAYA (Jarak < 20 cm)
    lcd.print("STATUS: BAHAYA! ");
    
    // Buzzer berbunyi sangat cepat (kelap-kelip cepat)
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  } 
  else if (jarak >= 20 && jarak <= 50) {
    // KONDISI WASPADA (Jarak 20 cm - 50 cm)
    lcd.print("STATUS: WASPADA ");
    
    // Buzzer berbunyi lambat (beep... beep...)
    digitalWrite(PIN_BUZZER, HIGH);
    delay(300);
    digitalWrite(PIN_BUZZER, LOW);
    delay(300);
  } 
  else {
    // KONDISI AMAN (Jarak > 50 cm)
    lcd.print("STATUS: AMAN :) ");
    digitalWrite(PIN_BUZZER, LOW); // Buzzer mati
    delay(500); // Jeda standar pembacaan
  }

  // Debugging lewat Serial Monitor
  Serial.print("Jarak Terbaca: ");
  Serial.print(jarak);
  Serial.println(" cm");
}
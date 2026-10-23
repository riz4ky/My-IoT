/*
 * Proyek: Pendeteksi Jarak dengan ESP32, Ultrasonik HC-SR04, Buzzer, dan LCD I2C
 * Deskripsi: Menampilkan jarak di LCD dan Buzzer berbunyi semakin cepat jika objek semakin dekat.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Mengatur alamat I2C LCD (Biasanya 0x27 atau 0x3F)
// Format: (Alamat I2C, jumlah kolom, jumlah baris)
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Mendefinisikan pin yang digunakan pada ESP32
const int trigPin = 5;    // Pin TRIG sensor ultrasonik terhubung ke GPIO 5
const int echoPin = 18;   // Pin ECHO sensor ultrasonik terhubung ke GPIO 18
const int buzzerPin = 19; // Pin Positif Buzzer terhubung ke GPIO 19

// Variabel untuk menyimpan durasi gelombang dan jarak
long duration;
int distance;

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight(); // Menyalakan lampu latar LCD
  lcd.setCursor(0, 0);
  lcd.print("Sistem Aktif!");
  delay(2000); // Tahan tulisan awal selama 2 detik
  lcd.clear();
  
  // Mengatur mode pin
  pinMode(trigPin, OUTPUT); 
  pinMode(echoPin, INPUT);  
  pinMode(buzzerPin, OUTPUT); 
}

void loop() {
  // 1. Memastikan pin TRIG dalam keadaan mati (LOW) terlebih dahulu
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // 2. Mengirimkan sinyal ultrasonik (HIGH selama 10 mikrodetik)
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // 3. Membaca waktu pantulan (ECHO) 
  duration = pulseIn(echoPin, HIGH, 30000); 
  
  // 4. Menghitung jarak (cm)
  distance = duration * 0.034 / 2;
  
  // Jika tidak terbaca, set jarak ke batas maksimal
  if (distance == 0) {
    distance = 400; 
  }

  // Tampilkan hasil jarak ke Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Tampilkan hasil jarak ke LCD
  lcd.setCursor(0, 0); // Baris pertama
  lcd.print("Jarak Objek:");
  
  lcd.setCursor(0, 1); // Baris kedua
  lcd.print(distance);
  lcd.print(" cm    "); // Tambahan spasi untuk menimpa sisa angka sebelumnya

  // 5. Logika Buzzer berdasarkan jarak objek
  if (distance <= 10) {
    // Objek sangat dekat (<= 10 cm): Bunyi sangat cepat
    digitalWrite(buzzerPin, HIGH);
    delay(50);
    digitalWrite(buzzerPin, LOW);
    delay(50);
  } 
  else if (distance > 10 && distance <= 30) {
    // Objek dekat (11 - 30 cm): Bunyi sedang
    digitalWrite(buzzerPin, HIGH);
    delay(150);
    digitalWrite(buzzerPin, LOW);
    delay(150);
  } 
  else if (distance > 30 && distance <= 50) {
    // Objek lumayan dekat (31 - 50 cm): Bunyi lambat
    digitalWrite(buzzerPin, HIGH);
    delay(400);
    digitalWrite(buzzerPin, LOW);
    delay(400);
  } 
  else {
    // Objek jauh (> 50 cm): Buzzer diam
    digitalWrite(buzzerPin, LOW);
    delay(100); 
  }
}
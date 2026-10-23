/*
 * ============================================================
 * RANGKAIAN (WIRING) LCD 16x2 I2C ke Arduino Uno
 * ============================================================
 * * Pin LCD I2C   |   Pin Arduino Uno
 * ------------------------------------------------------------
 * GND           ->  GND
 * VCC           ->  5V
 * SDA           ->  Pin A4 (atau pin khusus SDA jika ada)
 * SCL           ->  Pin A5 (atau pin khusus SCL jika ada)
 * * Catatan:
 * Pastikan Anda sudah menginstal library "LiquidCrystal I2C" 
 * karya Frank de Brabander melalui Library Manager di Arduino IDE.
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Mengatur alamat I2C LCD ke 0x27 untuk layar 16 karakter dan 2 baris.
// Jika layar tidak menampilkan teks (hanya nyala saja), 
// coba ganti 0x27 menjadi 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Memulai komunikasi dengan LCD
  lcd.init();
  
  // Menyalakan lampu latar (backlight) pada LCD
  lcd.backlight();
  
  // Mengatur posisi kursor di kolom 0 (pertama), baris 0 (pertama)
  lcd.setCursor(0, 0);
  
  // Mencetak teks ke layar LCD
  lcd.print("Halo yayaa");
}

void loop() {
  // Biarkan kosong karena teks hanya perlu ditampilkan satu kali
}
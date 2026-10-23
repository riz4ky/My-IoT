#include <dummy.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Atur alamat I2C LCD ke 0x27, dengan 16 kolom dan 2 baris.
// Catatan: Jika layar menyala tapi teks tidak muncul, 
// coba ganti alamat 0x27 menjadi 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Memulai komunikasi I2C dan inisialisasi LCD
  lcd.init();
  
  // Menyalakan lampu latar (backlight) LCD
  lcd.backlight();
}

void loop() {
  // Teks 1: Maafin raden (Teks berada di tengah)
  lcd.clear(); 
  lcd.setCursor(2, 0); 
  lcd.print("Maafin raden");
  delay(3000); // Jeda 3 detik
  
  // Teks 2: Raden minta maaf (Pas 16 huruf)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Raden minta maaf");
  delay(3000);
  
  // Teks 3: Raden sayang yaya (Dibuat 2 baris karena panjang)
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Raden sayang");
  lcd.setCursor(6, 1); // Posisi "yaya" di tengah baris kedua
  lcd.print("yaya");
  delay(3000);
  
  // Teks 4: Raden sayang radyva (Dibuat 2 baris karena panjang)
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Raden sayang");
  lcd.setCursor(5, 1); // Posisi "radyva" di tengah baris kedua
  lcd.print("radyva");
  delay(3000);
}
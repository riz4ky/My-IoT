

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Mengatur alamat I2C LCD ke 0x27 dengan ukuran 16 kolom dan 2 baris.
// Catatan penting: Jika LCD tidak menampilkan apa-apa, 
// coba ganti alamat 0x27 menjadi 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Memulai komunikasi I2C dan inisialisasi LCD
  lcd.init();
  
  // Menyalakan lampu latar (backlight) pada LCD
  lcd.backlight();
  
  // Mengatur kursor ke kolom pertama (0), baris pertama (0)
  lcd.setCursor(0, 0);
  // Menampilkan teks di baris pertama
  lcd.print("Halo ESP32!");
  
  // Mengatur kursor ke kolom pertama (0), baris kedua (1)
  lcd.setCursor(0, 1);
  // Menampilkan teks di baris kedua
  lcd.print("Tes LCD I2C OK");
}

void loop() {
  // Untuk contoh dasar ini, kita tidak memerlukan kode yang berulang,
  // sehingga bagian loop dibiarkan kosong. Teks akan terus tampil.
  
  /* * Jika Anda ingin membuat teks berkedip (opsional), 
   * Anda bisa menghapus tanda komentar pada kode di bawah ini:
   */
  // lcd.noBacklight();
  // delay(1000);
  // lcd.backlight();
  // delay(1000);
}
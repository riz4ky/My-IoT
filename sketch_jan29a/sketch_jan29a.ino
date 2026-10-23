// Kode ini secara otomatis diperbarui (di-generate) mengikuti tata olah spesifikasi terstruktur RTL IFUMMI 25 pada: Sabtu, 04 April 2026 pukul 08.22.16 GMT+7

#include <Wire.h>
#include <LiquidCrystal_I2C.h> // https://github.com/johnrickman/LiquidCrystal_I2C

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("TEST");

}

void loop() {

}
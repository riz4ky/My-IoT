/*
  Project: Sistem Lampu LED Romantis "Me and yaya"
  Board  : ESP32
  Fitur  : LED, LCD 16x2 I2C, OLED 0.96", Buzzer, Tactical Switch
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- KONFIGURASI PIN ESP32 ---
#define BUTTON_PIN 15
#define LED_PIN    4
#define BUZZER_PIN 5

// --- KONFIGURASI OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- KONFIGURASI LCD ---
// Alamat I2C LCD biasanya 0x27. Jika tidak tampil, coba 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- NADA MELODI ROMANTIS (Arpeggio) ---
int melody[] = { 262, 330, 392, 494, 523, 494, 392, 330 }; // C4, E4, G4, B4, C5, B4, G4, E4
int noteDuration = 150; // Durasi tiap nada dalam milidetik

void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin
  pinMode(BUTTON_PIN, INPUT_PULLDOWN); // Sesuaikan dengan output modul switch Anda (PULLDOWN jika modul aktif HIGH)
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Matikan LED dan Buzzer di awal
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Halo Yaya!   ");
  lcd.setCursor(0, 1);
  lcd.print(" Tekan Tombol <3");

  // Inisialisasi OLED
  // Alamat OLED biasanya 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal menemukan OLED SSD1306"));
    for(;;); // Berhenti jika OLED gagal
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print("Menunggu...");
  display.display();
}

void loop() {
  // Membaca status tombol (Asumsi modul mengeluarkan sinyal HIGH saat ditekan)
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == HIGH) {
    // 1. Nyalakan LED
    digitalWrite(LED_PIN, HIGH);

    // 2. Tampilkan teks di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Me and yaya   ");
    lcd.setCursor(0, 1);
    lcd.print("  FOREVERR <3");

    // 3 & 4. Mainkan melodi dan Animasi Love di OLED
    playRomanceAndAnimate();

  } else {
    // Matikan semua fungsi saat tombol dilepas
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    
    // Kembalikan tampilan layar standby
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 500) { // Update layar tiap 500ms agar tidak berkedip
      lcd.setCursor(0, 0);
      lcd.print("   Hii Yayaak!   ");
      lcd.setCursor(0, 1);
      lcd.print(" Tekan Tombol <3");
      
      display.clearDisplay();
      display.setCursor(10, 25);
      display.print("   Tekan duluuu!   ");
      display.display();
      
      lastUpdate = millis();
    }
  }
}

// Fungsi untuk menggambar bentuk hati dasar
void drawHeart(int x, int y, int radius) {
  // Dua lingkaran atas
  display.fillCircle(x - radius, y, radius, SSD1306_WHITE);
  display.fillCircle(x + radius, y, radius, SSD1306_WHITE);
  // Segitiga bawah
  display.fillTriangle(x - radius*2, y + radius/2, 
                       x + radius*2, y + radius/2, 
                       x, y + radius*3, 
                       SSD1306_WHITE);
}

// Fungsi untuk memainkan lagu sambil menganimasikan hati yang berdetak
void playRomanceAndAnimate() {
  int numNotes = sizeof(melody) / sizeof(melody[0]);
  
  for (int i = 0; i < numNotes; i++) {
    // Jika tombol tiba-tiba dilepas di tengah lagu, hentikan lagu
    if (digitalRead(BUTTON_PIN) == LOW) break; 

    display.clearDisplay();

    // Buat animasi berdetak (besar - kecil) berdasarkan index nada (ganjil/genap)
    if (i % 2 == 0) {
      drawHeart(64, 25, 8); // Hati besar
    } else {
      drawHeart(64, 25, 6); // Hati kecil
    }
    
    // Tambahkan teks kecil di bawah hati
    display.setCursor(20, 50);
    display.print("Lopee yang banyak");
    display.display();

    // Bunyikan nada
    tone(BUZZER_PIN, melody[i]);
    delay(noteDuration);
  }
  // Matikan nada setelah siklus selesai
  noTone(BUZZER_PIN);
}
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h> // Library untuk LCD 16x2

// --- PENGATURAN LAYAR ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Alamat I2C LCD biasanya 0x27 atau 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- PENGATURAN PIN BARU ---
#define LED_RED_PIN 4
#define LED_GREEN_PIN 16
#define LED_YELLOW_PIN 18  // Dipindah ke 18 agar tidak bentrok
#define BUZZER_PIN 5      // Buzzer disesuaikan dengan wiring kamu di GPIO 17

// --- DATA NADA TETRIS (KOROBEINIKI) ---
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_A4  440
#define NOTE_A5  880

int melody[] = {
  NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4,
  NOTE_A4, NOTE_A4, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5,
  NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, 0
};
int noteDurations[] = {
  4, 8, 8, 4, 8, 8,
  4, 8, 8, 4, 8, 8,
  4, 8, 8, 4,
  4, 4, 2, 4
};

// --- DATA BITMAP GAMBAR ---
// Gambar Hati Kecil (16x16 pixel) untuk di pojok
const unsigned char bitmap_small_heart [] PROGMEM = {
  0x00, 0x00, 0x1c, 0x38, 0x3e, 0x7c, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x3f, 0xfc, 0x1f, 0xf8, 
  0x0f, 0xf0, 0x07, 0xe0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Gambar Bintang (32x32 pixel) untuk di tengah
const unsigned char bitmap_star [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x07, 0xe0, 0x00, 
  0x00, 0x07, 0xe0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 
  0x00, 0x3f, 0xfc, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x1f, 0xff, 0xff, 0xf8, 0x0f, 0xff, 0xff, 0xf0, 
  0x07, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0x00, 
  0x00, 0x7f, 0xfe, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0xff, 0xff, 0x00, 
  0x01, 0xff, 0xff, 0x80, 0x03, 0xf8, 0x1f, 0xc0, 0x03, 0xf0, 0x0f, 0xc0, 0x07, 0xe0, 0x07, 0xe0, 
  0x07, 0xc0, 0x03, 0xe0, 0x0f, 0x80, 0x01, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0e, 0x00, 0x00, 0x70, 
  0x04, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- TUGAS (TASK) UNTUK MUSIK TETRIS ---
void playMusicTask(void * parameter) {
  while(true) {
    for (int thisNote = 0; thisNote < 20; thisNote++) {
      if (melody[thisNote] == 0) {
        delay(1000); // Jeda sebelum mengulang lagu
        continue;
      }
      int noteDuration = 1000 / noteDurations[thisNote];
      tone(BUZZER_PIN, melody[thisNote], noteDuration);
      
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      noTone(BUZZER_PIN);
    }
  }
}

// --- TUGAS (TASK) UNTUK LED HIJAU & KUNING ---
// Berjalan terpisah agar bisa berkedip sangat cepat (100ms) bergantian
void blinkLedTask(void * parameter) {
  while(true) {
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_YELLOW_PIN, LOW);
    delay(100); 
    
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, HIGH);
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

  // Setup Pin
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Inisialisasi LCD 16x2
  lcd.init();
  lcd.backlight();
  lcd.setCursor(5, 0); // Posisi tengah baris 1
  lcd.print("yayakk");
  lcd.setCursor(5, 1); // Posisi tengah baris 2
  lcd.print("TETRIS");

  // Inisialisasi OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal menemukan OLED"));
    for(;;);
  }

  // Menjalankan tugas musik di Core 0
  xTaskCreatePinnedToCore(
    playMusicTask,   
    "MusicTask",     
    2048,            
    NULL,            
    1,               
    NULL,            
    0                
  );

  // Menjalankan tugas LED berkedip cepat di Core 0
  xTaskCreatePinnedToCore(
    blinkLedTask,
    "LedTask",
    1024,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  // --- FRAME 1: Bintang Terlihat & LED Merah Menyala ---
  display.clearDisplay();
  
  // Gambar 4 Hati di setiap pojokan
  display.drawBitmap(0, 0, bitmap_small_heart, 16, 16, SSD1306_WHITE);                   // Kiri Atas
  display.drawBitmap(128 - 16, 0, bitmap_small_heart, 16, 16, SSD1306_WHITE);            // Kanan Atas
  display.drawBitmap(0, 64 - 16, bitmap_small_heart, 16, 16, SSD1306_WHITE);             // Kiri Bawah
  display.drawBitmap(128 - 16, 64 - 16, bitmap_small_heart, 16, 16, SSD1306_WHITE);      // Kanan Bawah
  
  // Gambar Bintang di tengah
  int starX = (128 - 32) / 2; // Posisi X di tengah
  int starY = (64 - 32) / 2;  // Posisi Y di tengah
  display.drawBitmap(starX, starY, bitmap_star, 32, 32, SSD1306_WHITE);
  
  display.display();
  digitalWrite(LED_RED_PIN, HIGH); // Nyalakan LED Merah
  delay(500); // Tahan selama setengah detik


  // --- FRAME 2: Bintang Hilang (Berkedip) & LED Merah Mati ---
  display.clearDisplay();
  
  // Gambar ulang 4 hati saja (Bintang dihilangkan agar seolah berkedip)
  display.drawBitmap(0, 0, bitmap_small_heart, 16, 16, SSD1306_WHITE);
  display.drawBitmap(128 - 16, 0, bitmap_small_heart, 16, 16, SSD1306_WHITE);
  display.drawBitmap(0, 64 - 16, bitmap_small_heart, 16, 16, SSD1306_WHITE);
  display.drawBitmap(128 - 16, 64 - 16, bitmap_small_heart, 16, 16, SSD1306_WHITE);
  
  display.display();
  digitalWrite(LED_RED_PIN, LOW); // Matikan LED Merah
  delay(500); // Tahan selama setengah detik
}
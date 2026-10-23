// ==== Test Relay dengan ESP32 ====

#define RELAY_PIN 26   // Ganti sesuai pin yang kamu pakai

// Beberapa modul relay aktif LOW (nyala saat pin LOW)
// Kalau relay kamu jenis Active LOW, ganti true jadi false di bawah
#define RELAY_ACTIVE_LOW true

void relayOn() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
}

void relayOff() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  relayOff(); // Pastikan relay mati saat pertama nyala
  Serial.println("Mulai test relay...");
}

void loop() {
  Serial.println("Relay: ON");
  relayOn();
  delay(2000);

  Serial.println("Relay: OFF");
  relayOff();
  delay(2000);
}
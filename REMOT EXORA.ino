#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Definisi Pin
#define CE_PIN   9
#define CSN_PIN  10
#define BUZZ_PIN 4

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// Struktur data (Wajib Sinkron dengan ESP32)
struct __attribute__((packed)) RemoteData {
  int16_t leftY;
  int16_t leftX;
  int16_t rightX;
  int16_t rightY;
  bool ledBtn;
  bool hornBtn;
};
RemoteData dataToSend;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(2, INPUT_PULLUP); // Tombol Lampu
  pinMode(3, INPUT_PULLUP); // Tombol Klakson

  Serial.println("--- PENGECEKAN HARDWARE REMOT ---");

  // Inisialisasi Radio
  if (!radio.begin()) {
    Serial.println("HASIL: nRF24L01 TIDAK TERDETEKSI!");
    Serial.println("SOLUSI: Cek kabel di pin 11, 12, 13 (SCK/MISO/MOSI)");
    while (1) {
      // Alarm jika kabel lepas atau modul mati
      digitalWrite(BUZZ_PIN, HIGH); delay(100);
      digitalWrite(BUZZ_PIN, LOW);  delay(100);
    }
  }

  radio.openWritingPipe(address);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW); // Gunakan LOW agar tidak butuh daya besar
  radio.setAutoAck(true);
  radio.stopListening();

  Serial.println("HASIL: nRF24L01 OK! SIAP KIRIM.");
  
  // Bunyi bip sekali tanda berhasil
  digitalWrite(BUZZ_PIN, HIGH); delay(200); digitalWrite(BUZZ_PIN, LOW);
}

void loop() {
  // Baca Nilai Joystick
  dataToSend.leftY  = analogRead(A0);
  dataToSend.leftX  = analogRead(A1);
  dataToSend.rightX = analogRead(A2);
  dataToSend.rightY = analogRead(A3);

  // Baca Tombol
  dataToSend.ledBtn  = (digitalRead(2) == LOW);
  dataToSend.hornBtn = (digitalRead(3) == LOW);

  // Kirim ke Mobil
  bool statusKirim = radio.write(&dataToSend, sizeof(dataToSend));

  if (statusKirim) {
    Serial.println("Data: TERKIRIM");
  } else {
    Serial.println("Data: GAGAL (Mobil mati atau di luar jangkauan)");
    // Indikator gagal kirim: bip sangat pendek
    digitalWrite(BUZZ_PIN, HIGH); delay(5); digitalWrite(BUZZ_PIN, LOW);
  }


  3

  delay(50);
}

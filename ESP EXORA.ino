#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ========== PIN MOTOR ==========
#define IN1 5
#define IN2 18
#define IN3 19
#define IN4 21
#define ENA 23
#define ENB 22

// ========== PIN NRF24L01 ==========
#define CE_PIN   4
#define CSN_PIN  2
#define SCK_PIN  15
#define MISO_PIN 16
#define MOSI_PIN 17

#define BUZZER 32
#define LAMPU_PIN 12

struct __attribute__((packed)) RemoteData {
  int16_t leftY;
  int16_t leftX;
  int16_t rightX;
  int16_t rightY;
  bool ledBtn;
  bool hornBtn;
};

RemoteData data;
SPIClass *mySPI = new SPIClass(HSPI);
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";
unsigned long lastRecvTime = 0;

// Variabel Kontrol
int currentSpeed = 0;
const int MAX_SPEED_LIMIT = 38; // 15% Speed
const int ACCEL_DELAY = 4;

// Variabel Toggle LED
bool statusLampu = false;
bool tombolLedSebelumnya = false;

void paksaStop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
  currentSpeed = 0; 
}

void setup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  paksaStop();

  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT);
  pinMode(LAMPU_PIN, OUTPUT);
  digitalWrite(LAMPU_PIN, LOW);

  mySPI->begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);
  if (!radio.begin(mySPI)) {
    while (1);
  }

  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(1, address);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    radio.read(&data, sizeof(data));
    lastRecvTime = millis();
    eksekusiGerak();
  }

  if (millis() - lastRecvTime > 500) {
    paksaStop();
  }
}

void eksekusiGerak() {
  // --- LOGIKA KLAKSON (Tetap Hold) ---
  digitalWrite(BUZZER, data.hornBtn ? HIGH : LOW);

  // --- LOGIKA TOGGLE LED (Tap Mati/Hidup) ---
  // Jika tombol sekarang ditekan DAN sebelumnya tidak ditekan
  if (data.ledBtn == true && tombolLedSebelumnya == false) {
    statusLampu = !statusLampu; // Balikkan status (ON jadi OFF, OFF jadi ON)
    digitalWrite(LAMPU_PIN, statusLampu ? HIGH : LOW);
  }
  tombolLedSebelumnya = data.ledBtn; // Simpan status sekarang untuk loop berikutnya

  bool bergerak = false;

  // 1. MAJU / MUNDUR
  if (data.rightX < 400) { // Maju
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
    bergerak = true;
  } 
  else if (data.rightX > 600) { // Mundur
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    bergerak = true;
  } 
  
  // 2. BELOK KIRI / KANAN
  else if (data.leftY < 400) { // Belok Kanan
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
    bergerak = true;
  }
  else if (data.leftY > 600) { // Belok Kiri
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    bergerak = true;
  }

  if (bergerak) {
    if (currentSpeed < MAX_SPEED_LIMIT) {
      currentSpeed += 1; 
      delay(ACCEL_DELAY); 
    }
    analogWrite(ENA, currentSpeed);
    analogWrite(ENB, currentSpeed);
  } else {
    paksaStop();
  }
}

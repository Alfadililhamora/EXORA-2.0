#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

// ========== PIN TAMBAHAN (ULTRASONIK & OLED) ==========
#define TRIG_PIN 33
#define ECHO_PIN 34
#define OLED_SDA 25
#define OLED_SCL 26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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

// Variabel Kontrol OLED
unsigned long angryUntil = 0;

// Variabel untuk buzzer ultrasonik
bool ultrasonicObstacle = false;

// === Variabel Animasi Mata (Updated) ===
int ref_eye_height = 40;
int ref_eye_width = 40;
int ref_space_between_eye = 10;
int ref_corner_radius = 10;

float cur_eye_height = ref_eye_height; // Gunakan float untuk transisi halus
float cur_x_offset = 0;
float target_x_offset = 0;

unsigned long last_move_time = 0;
unsigned long last_blink_time = 0;
bool is_blinking = false;
bool blink_closing = true;

void paksaStop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
  currentSpeed = 0; 
}

// --- FUNGSI BACA JARAK ---
long bacaJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long durasi = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (durasi == 0) return 999;
  return durasi * 0.034 / 2;
}

// === Fungsi Animasi Mata ===
void draw_eyes(String emosi) {
  display.clearDisplay();
  
  unsigned long now = millis();

  // 1. LOGIKA ANIMASI IDLE (Lirik & Kedip)
  if (emosi == "IDLE") {
    // A. Lirik Kiri-Kanan Smooth
    if (now - last_move_time > (2000 + random(3000))) {
      int r = random(3);
      if (r == 0) target_x_offset = -15; // Kiri
      else if (r == 1) target_x_offset = 15; // Kanan
      else target_x_offset = 0;             // Tengah
      last_move_time = now;
    }
    cur_x_offset = (cur_x_offset * 0.8) + (target_x_offset * 0.2);

    // B. Kedip (Blink) Smooth
    if (!is_blinking && (now - last_blink_time > (3000 + random(4000)))) {
      is_blinking = true;
      blink_closing = true;
    }

    if (is_blinking) {
      if (blink_closing) {
        cur_eye_height -= 8.0; 
        if (cur_eye_height <= 2) blink_closing = false;
      } else {
        cur_eye_height += 8.0;
        if (cur_eye_height >= ref_eye_height) {
          cur_eye_height = ref_eye_height;
          is_blinking = false;
          last_blink_time = now;
        }
      }
    } else {
      cur_eye_height = (cur_eye_height * 0.8) + (ref_eye_height * 0.2);
    }
  } else {
    // Reset offset jika bukan IDLE
    cur_x_offset = 0;
    cur_eye_height = ref_eye_height;
  }

  // Koordinat Dasar
  int base_ly = SCREEN_HEIGHT / 2 - (int)cur_eye_height / 2;
  int base_ry = SCREEN_HEIGHT / 2 - (int)cur_eye_height / 2;
  int base_lx = (SCREEN_WIDTH / 2 - ref_eye_width / 2 - ref_space_between_eye / 2) + (int)cur_x_offset;
  int base_rx = (SCREEN_WIDTH / 2 + ref_eye_width / 2 + ref_space_between_eye / 2) + (int)cur_x_offset;

  // 2. GAMBAR MATA DASAR
  display.fillRoundRect(base_lx - ref_eye_width/2, base_ly, ref_eye_width, (int)cur_eye_height, ref_corner_radius, SSD1306_WHITE);
  display.fillRoundRect(base_rx - ref_eye_width/2, base_ly, ref_eye_width, (int)cur_eye_height, ref_corner_radius, SSD1306_WHITE);

  // 3. LOGIKA MATA ANGRY (Tajam Sesuai Gambar)
  if (emosi == "ANGRY") {
    // Potongan Miring Mata Kiri
    display.fillTriangle(
      base_lx - ref_eye_width/2, base_ly - 1,
      base_lx + ref_eye_width/2 + 1, base_ly - 1,
      base_lx + ref_eye_width/2 + 1, base_ly + (ref_eye_height * 0.7),
      SSD1306_BLACK
    );
    // Potongan Miring Mata Kanan
    display.fillTriangle(
      base_rx + ref_eye_width/2, base_ly - 1,
      base_rx - ref_eye_width/2 - 1, base_ly - 1,
      base_rx - ref_eye_width/2 - 1, base_ly + (ref_eye_height * 0.7),
      SSD1306_BLACK
    );
  }

  // 4. LOGIKA MATA HAPPY (Lengkungan Bawah)
  if (emosi == "HAPPY") {
    int happy_offset = ref_eye_height / 2;
    display.fillRoundRect(base_lx - ref_eye_width/2 - 1, (base_ly + ref_eye_height) - happy_offset, ref_eye_width + 2, ref_eye_height, ref_corner_radius, SSD1306_BLACK);
    display.fillRoundRect(base_rx - ref_eye_width/2 - 1, (base_ly + ref_eye_height) - happy_offset, ref_eye_width + 2, ref_eye_height, ref_corner_radius, SSD1306_BLACK);
  }

  display.display();
}

// Fungsi untuk menentukan emosi berdasarkan kondisi robot
String getCurrentEmotion() {
  long jarak = bacaJarak();
  bool isMoving = (currentSpeed > 0);
  
  // Jika ada objek dalam jarak <=20 cm, set ANGRY
  if (jarak > 0 && jarak <= 20) {
    angryUntil = millis() + 100; // tahan ANGRY selama 100 ms
    ultrasonicObstacle = true;
    return "ANGRY";
  }
  // Jika masih dalam masa tahan ANGRY, tetap ANGRY
  else if (millis() < angryUntil) {
    return "ANGRY";
  }
  // Jika tidak ada objek, reset flag ultrasonik
  else {
    ultrasonicObstacle = false;
  }
  
  // Jika tidak ANGRY dan robot bergerak, set HAPPY
  if (isMoving) {
    return "HAPPY";
  }
  // Jika tidak ada kondisi khusus, set IDLE
  else {
    return "IDLE";
  }
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
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED Gagal inisialisasi!"));
  }
  
  // Inisialisasi random seed untuk animasi
  randomSeed(analogRead(0));
  
  // Tampilan awal
  draw_eyes("IDLE");

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
  
  // Kontrol buzzer dengan prioritas: Ultrasonik > Remote
  // Jika ada objek terdeteksi, buzzer menyala terus
  if (ultrasonicObstacle) {
    digitalWrite(BUZZER, HIGH);
  } 
  // Jika tidak ada objek, buzzer dikontrol oleh remote (klakson)
  else {
    digitalWrite(BUZZER, data.hornBtn ? HIGH : LOW);
  }
  
  // Dapatkan emosi saat ini dan update tampilan mata
  String currentEmotion = getCurrentEmotion();
  draw_eyes(currentEmotion);
}

void eksekusiGerak() {
  // HAPUS baris digitalWrite(BUZZER, ...) dari sini karena sudah dipindah ke loop()
  
  if (data.ledBtn == true && tombolLedSebelumnya == false) {
    statusLampu = !statusLampu;
    digitalWrite(LAMPU_PIN, statusLampu ? HIGH : LOW);
  }
  tombolLedSebelumnya = data.ledBtn;

  bool bergerak = false;

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

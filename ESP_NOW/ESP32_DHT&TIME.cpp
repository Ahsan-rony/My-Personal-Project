#include <WiFi.h>
#include <esp_now.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>  

#define DHTPIN 32      // Pin DHT11 terhubung
#define DHTTYPE DHT11 // Jenis sensor DHT
#define RELAY_PIN 13
#define MOSFET_PIN  4   // Pin untuk MOSFET
DHT dht(DHTPIN, DHTTYPE);

#define BUTTON_PIN1 5  // Pin 4 untuk tombol relay 1
#define BUTTON_PIN2 18  // Pin 0 untuk tombol relay 2
#define BUTTON_PIN3 19

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4); // Alamat I2C LCD 2004 (bisa 0x27 atau 0x3F)

bool buttonPressed1 = false;
bool buttonPressed2 = false;
bool longPress = false;

unsigned long pressTime = 0;

unsigned long lastUpdate = 0;  // Untuk melacak waktu update terakhir
const unsigned long updateInterval = 60000;  // Interval waktu update (60 detik)

bool relayState = false;
int brightness = 255;    // Kecerahan awal 100% (0-255)
bool mosfetState = false; 

typedef struct struct_message {
  float temperature;
  float humidity;
  float clockh;
  float clockm;
  bool relayState = false;
  bool mosfet;
  bool cek1;
} struct_message;

struct_message data1;

typedef struct struct_message2 {
  bool relayState = false;
  bool cek2;
} struct_message2;

struct_message2 data2;


uint8_t esp8266Address[] = {0xEC, 0x64, 0xC9, 0xDC, 0x9B, 0x72};
uint8_t esp32Address[] = {0xec, 0x64, 0xc9, 0x5e, 0x75, 0x6c};

esp_now_peer_info_t peerInfo;
esp_now_peer_info_t peerInfo2;

float lastTemperature = -100.0;
float lastHumidity = -100.0;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // Offset zona waktu WIB (UTC+7)
const int daylightOffset_sec = 0;    // WIB tidak memiliki daylight saving time

byte thermometer[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01010,
  0b01110,
  0b11111,
  0b11111,
  0b01110
};

byte waterDrop[8] = {
  0b00100,
  0b00100,
  0b01010,
  0b01010,
  0b10001,
  0b10001,
  0b10001,
  0b01110
};

void displayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

   // Array untuk menyimpan nama hari dalam bahasa Inggris
  const char* daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

  // Tampilkan hari di baris 1, mepet ke kanan
  lcd.setCursor(20 - strlen(daysOfWeek[timeinfo.tm_wday]), 1); // Hitung posisi cursor
  lcd.print(daysOfWeek[timeinfo.tm_wday]);  // timeinfo.tm_wday memberikan nilai dari 0 (Sunday) hingga 6 (Saturday)

  // Tampilkan jam di baris 3, menggantikan status relay
  lcd.setCursor(0, 1);
  lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  // Tampilkan tanggal di baris 4
  char date[20];
  strftime(date, sizeof(date), "%d %B %Y", &timeinfo);  // Format: "17 Agustus 2024"
  
  int dateLength = strlen(date);
  int centerPosition = (20 - dateLength) / 2; // Hitung posisi tengah
  lcd.setCursor(centerPosition, 2); // Set cursor ke posisi tengah
  lcd.print(date);
}

// Function to handle data sent feedback
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Data Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&data1, incomingData, sizeof(data1));
  esp_now_send(esp32Address, (uint8_t *)&data1, sizeof(data1));
  Serial.println(data1.temperature);
  if (data1.mosfet) {
    mosfetState = true;
    Serial.println(mosfetState ? "ON" : "OFF");
  } else {
    mosfetState = false;
  }
}

// Function to display received data on OLED
void updateLCD(float temperature, float humidity) {
  lcd.clear();  // Bersihkan layar

  // Membuat karakter kustom
  lcd.createChar(0, thermometer);  // Simbol termometer disimpan di indeks 0
  lcd.createChar(1, waterDrop);    // Simbol tetesan air disimpan di indeks 1

  lcd.setCursor(0, 0);  // Baris 1
  lcd.write((byte)0);  // Menampilkan simbol termometer
  lcd.print(temperature);
  lcd.print("C");

  lcd.setCursor(13, 0);  // Baris 2
  lcd.write((byte)1);  // Menampilkan simbol tetesan air
  lcd.print(humidity);
  lcd.print("%");

  lcd.setCursor(0, 3);
  lcd.print("Dont dare u g hollow");
  displayTime();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Initial state of the relay is OFF

  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  analogWrite(MOSFET_PIN, brightness);  // Set kecerahan awal

  // Inisialisasi LCD
  lcd.init();  
  lcd.backlight();

  WiFi.begin("KOS_MBAK_TUN_BAWAH", "24031993");  // Ganti dengan SSID dan password WiFi
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.println("Connecting to WiFi...");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  Serial.println("Time synchronized");

  delay(1000);

  Serial.println("Wifi Disconnect");
// Setelah sinkronisasi waktu, matikan Wi-Fi
  WiFi.disconnect(true);  // Putuskan Wi-Fi dan matikan WiFi STA
  WiFi.mode(WIFI_OFF);    // Matikan Wi-Fi untuk menghemat daya
  delay(1000);

  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks with updated function signatures
  esp_now_register_send_cb(OnDataSent);  // For sending feedback
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  memcpy(peerInfo.peer_addr, esp8266Address, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer 1");
    return;
  }

  memcpy(peerInfo2.peer_addr, esp32Address, 6); // esp32Address adalah MAC Address ESP32
  peerInfo2.channel = 1;
  peerInfo2.encrypt = false;
  if (esp_now_add_peer(&peerInfo2) != ESP_OK) {
    Serial.println("Failed to add peer 2");
    return;
  }

}


void loop() {
  struct tm timeinfo;
  // Baca nilai suhu dan kelembapan dari sensor DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  data1.temperature = dht.readTemperature();
  data1.humidity = dht.readHumidity();
  data1.clockh = timeinfo.tm_hour;
  data1.clockm = timeinfo.tm_min;

  // Cek apakah pembacaan berhasil
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  } if (temperature != lastTemperature || humidity != lastHumidity) {
    updateLCD(temperature, humidity);
    lastTemperature = temperature;
    lastHumidity = humidity;
  }

  // Perbarui LCD setiap 60 detik untuk menampilkan waktu
  if (millis() - lastUpdate >= updateInterval) {
    displayTime();  // Update waktu di LCD
    esp_now_send(esp32Address, (uint8_t *)&data1, sizeof(data1));
    lastUpdate = millis();  // Reset waktu update terakhir
  }

  // Mengontrol relay 1 yang terhubung ke ESP8266
  if (digitalRead(BUTTON_PIN1) == LOW) {
    if (!buttonPressed1) {
      buttonPressed1 = true;
      Serial.println("Button 1 pressed!");

      // Toggle relay state for relay 1
      data2.relayState = !data2.relayState;

      // Send relay 1 state to ESP8266
      esp_now_send(esp8266Address, (uint8_t *)&data2, sizeof(data2));

      delay(200);  // Debounce delay
    }
  } else {
    buttonPressed1 = false;
  }

  if (digitalRead(BUTTON_PIN2) == LOW) {
    if (!buttonPressed2) {  // Cek jika tombol belum ditekan
      buttonPressed2 = true;
      relayState = !relayState;  // Ubah status relay (ON/OFF)
      Serial.println("Button 2 pressed!");
      if (relayState) {
        Serial.println("Relay is ON");
      } else {
        Serial.println("Relay is ON");
      }

      // Toggle relay 2 state
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);  // Set status relay

      delay(200);  // Debounce delay
    }
  } else {
    buttonPressed2 = false;
  }

  if (digitalRead(BUTTON_PIN3) == LOW) {
    // Jika pressTime masih 0, itu artinya tombol baru saja ditekan
    if (pressTime == 0) {
      pressTime = millis();  // Catat waktu saat tombol mulai ditekan
      longPress = false;
    }

    // Cek apakah tombol sudah ditekan dan ditahan selama 1 detik
    if (millis() - pressTime >= 1000 && !longPress) {
        mosfetState = !mosfetState;  // Toggle MOSFET state (nyala/mati)
        longPress = true;  // Set flag longPress
        delay(200);  // Debounce delay agar tidak langsung memicu lagi
    }

  } else {
    // Tombol dilepas
    if (pressTime > 0 && !longPress) {
        // Aksi untuk short press: tombol ditekan kurang dari 1 detik
        if (mosfetState) {  // Hanya ubah kecerahan jika MOSFET menyala
            brightness -= 26;  // Kurangi kecerahan sebesar 10% (26 dari 255)
            if (brightness <= 0) {
                brightness = 255;  // Reset kecerahan ke 100% jika mencapai 0
            }
        }
    }
    pressTime = 0;  // Reset pressTime setelah menurunkan kecerahan
  }

  if (!mosfetState) {
    analogWrite(MOSFET_PIN, 0); // Matikan MOSFET
  } else {
    analogWrite(MOSFET_PIN, brightness);  // Update kecerahan
  }
 delay(100);
}

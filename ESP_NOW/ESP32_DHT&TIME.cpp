#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>
#include <esp_now.h>

#define DHTPIN 32      
#define DHTTYPE DHT11
#define RELAY_PIN 13
#define MOSFET_PIN 2  
DHT dht(DHTPIN, DHTTYPE);

#define BUTTON_PIN1 5  
#define BUTTON_PIN2 18  
#define BUTTON_PIN3 19

LiquidCrystal_I2C lcd(0x27, 20, 4); // Alamat I2C LCD 2004 (bisa 0x27 atau 0x3F)

typedef struct struct_message {
  float temperature;
  float humidity;
  float clockh;
  float clockm;
  int day;
  int month;
} struct_message;

struct_message data;

typedef struct struct_message2 {
  bool relayState;
  bool mosfet;
} struct_message2;

struct_message2 data2;

bool buttonPressed1 = false;
bool buttonPressed2 = false;
bool longPress = false;
bool mosfetState = false;
bool relayState = false;
int brightness = 255; 

float clockh;
float clockm;
float day; 
float month;

unsigned long pressTime = 0;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 60000;
const unsigned long cektime = 4000;

float lastTemperature = -100.0;
float lastHumidity = -100.0;

uint8_t esp8266Address[] = {0xEC, 0x64, 0xC9, 0xDC, 0x9B, 0x72};
uint8_t esp32Address[] = {0xec, 0x64, 0xc9, 0x5e, 0x75, 0x6c};

esp_now_peer_info_t peerInfo;
esp_now_peer_info_t peerInfo2;

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

void timecount(){
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= cektime) {
    lastUpdate = currentMillis; 
    esp_now_send(esp32Address, (uint8_t *)&data, sizeof(data));
    Serial.println("Mengirim data ke ESP32");
  }
}

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

  day = timeinfo.tm_mday;     
  month = timeinfo.tm_mon + 1;

  // Tampilkan jam di baris 3, menggantikan status relay
  lcd.setCursor(0, 1);
  lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  clockm = timeinfo.tm_min;
  clockh = timeinfo.tm_hour;

  // Tampilkan tanggal di baris 4
  char date[20];
  strftime(date, sizeof(date), "%d %B %Y", &timeinfo);  // Format: "17 Agustus 2024"
  
  int dateLength = strlen(date);
  int centerPosition = (20 - dateLength) / 2; // Hitung posisi tengah
  lcd.setCursor(centerPosition, 2); // Set cursor ke posisi tengah
  lcd.print(date);
}

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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Data Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  esp_now_send(esp32Address, (uint8_t *)&data, sizeof(data));
  if (len == sizeof(data)) {
   memcpy(&data, incomingData, sizeof(data));
   Serial.println("data balasan");
   Serial.println(data.temperature);
  } else if (len == sizeof(data2)) {
   memcpy(&data2, incomingData, sizeof(data2));
    if (!data2.mosfet) {
     mosfetState = !mosfetState;
     Serial.println(mosfetState ? "ON" : "OFF");
    } 
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Initial state of the relay is OFF

  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  analogWrite(MOSFET_PIN, brightness);  // Set kecerahan awal

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
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);    
  delay(1000);

  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  delay(200);
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
  timecount();
  struct tm timeinfo;
  // Baca nilai suhu dan kelembapan dari sensor DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  data.temperature = temperature;
  data.humidity = humidity;
  data.day = day;
  data.month = month;
  data.clockh = clockh;
  data.clockm = clockm;

  // Cek apakah pembacaan berhasil
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  } if (temperature != lastTemperature || humidity != lastHumidity) {
    esp_now_send(esp32Address, (uint8_t *)&data, sizeof(data));
    updateLCD(temperature, humidity);
    lastTemperature = temperature;
    lastHumidity = humidity;
  }

  // Perbarui LCD setiap 60 detik untuk menampilkan waktu
  if (millis() - lastUpdate >= updateInterval) {
    displayTime();  // Update waktu di LCD
    esp_now_send(esp32Address, (uint8_t *)&data, sizeof(data));
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

    if (millis() - pressTime >= 1000 && !longPress) {
        mosfetState = !mosfetState;  // Toggle MOSFET state (nyala/mati)
        longPress = true;  // Set flag longPress
        delay(200);  // Debounce delay agar tidak langsung memicu lagi
    }

  } else {
    if (pressTime > 0 && !longPress) {
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

#include <ESP8266WiFi.h>
#include <espnow.h>

#define RELAY_PIN 2  // Pin relay terhubung ke GPIO 2 pada ESP8266

uint8_t esp32Address[] = {0xec, 0x64, 0xc9, 0x5e, 0x75, 0x6c}; // MAC Address ESP32

bool relayState = false;  // Status relay saat ini

typedef struct struct_message2 {
  bool mosfet;
  bool relayState = true;
} struct_message2;

struct_message2 data2;

unsigned long lastUpdate = 0;

// Callback untuk menerima data
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  esp_now_send(esp32Address, (uint8_t *)&data2, sizeof(data2));
  memcpy(&data2, incomingData, sizeof(data2));
  // Ubah status relay berdasarkan data yang diterima dari ESP32
  if (!data2.relayState) {
    relayState = !relayState;  // Nyalakan relay jika data3.relayState adalah true
    Serial.println("Relay dinyalakan");
  } 
    
  // Atur relay ON/OFF berdasarkan relayState
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

  // Tampilkan status relay pada Serial Monitor
  Serial.print("Status Relay: ");
  Serial.println(relayState ? "ON" : "OFF");
}

// Callback saat data berhasil dikirim
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status pengiriman data: ");
  Serial.println(sendStatus == 0 ? "Sukses" : "Gagal");
}

void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);

  // Set pin relay sebagai output dan mulai dengan relay dalam kondisi OFF
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Mulai dengan relay OFF

  // Inisialisasi ESP-NOW
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  // Registrasi callback untuk pengiriman data
  esp_now_register_send_cb(OnDataSent);

  // Tambahkan peer ESP32 untuk komunikasi
  esp_now_add_peer(esp32Address, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= 5000) {
    lastUpdate = currentMillis; 
    esp_now_send(esp32Address, (uint8_t *)&data2, sizeof(data2));
    Serial.println("Mengirim data ke ESP32");
  }
}

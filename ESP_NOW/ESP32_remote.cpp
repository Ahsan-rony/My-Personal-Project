#include <BleKeyboard.h>
#include <WiFi.h>
#include <esp_now.h>
#include <U8g2lib.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const int BUTTON0 = 32;
const int BUTTON1 = 4;
const int BUTTON2 = 0;
const int BUTTON3 = 2;
const int BUTTON4 = 15;
const int SWITCH = 23;

bool b1status = false;
bool b2status = false;
bool b3status = false;
bool b4status = false;
bool BLE = false;
bool cek1 = false;
bool cek2 = false;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE); 
BleKeyboard bleKeyboard;

typedef struct struct_message {
  float temperature;
  float humidity;
  float clockh;
  float clockm;
  bool relayState = false;
  bool mosfet = true;
  bool cek1;
} struct_message;

struct_message data1;

typedef struct struct_message2 {
  bool relayState = false;
  bool cek2;
} struct_message2;

struct_message2 data2;

uint8_t esp8266Address[] = {0xEC, 0x64, 0xC9, 0xDC, 0x9B, 0x72};
uint8_t esp32Address[] = {0x08, 0xb6, 0x1f, 0xb8, 0x2a, 0x04};

esp_now_peer_info_t peerInfo;
esp_now_peer_info_t peerInfo2;

int mode = 1;

char timeString[6]; 

const unsigned long cektime = 5000;
const unsigned long timeout = 6000; 
unsigned long pressTime = 0;
unsigned long lastUpdate = 0;
unsigned long lastRecvTime1 = 0;  
unsigned long lastRecvTime2 = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Data Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  // Tampilkan jumlah byte yang diterima
  Serial.print("Bytes received: ");
  Serial.println(len);
  if (len == sizeof(data1)) {
    memcpy(&data1, incomingData, sizeof(data1));

    Serial.print("Received Temperature from ESP32: ");
    Serial.print(data1.temperature);
    Serial.println(" °C");
    
    Serial.print("Received Humidity from ESP32: ");
    Serial.print(data1.humidity);
    Serial.println(" %");

    cek1 = true;
    lastRecvTime1 = millis();
    Serial.println("ESP32 aktif");

  } else if (len == sizeof(data2)) {
    // Data dari ESP8266 (data3)
    memcpy(&data2, incomingData, sizeof(data2));

    // Tampilkan data dari ESP8266
    Serial.print("Received some data from ESP8266");
    // Tambahkan penanganan data3 sesuai kebutuhan

    // Reset waktu jika data dari ESP8266 diterima
    cek2 = true;
    lastRecvTime2 = millis();
    Serial.println("ESP8266 aktif");
  } else {
    // Jika data tidak sesuai dengan format yang diharapkan
    Serial.println("Unknown data format received");
  }
}

void oled() {
  u8g2.clearBuffer();

  u8g2.drawRBox(0, 6, 5, 5, 1);
  u8g2.drawRFrame(1, 2, 3, 6, 1);
  u8g2.setFont(u8g2_font_doomalpha04_te);  // Set font ukuran kecil
  u8g2.setCursor(7, 12);  // Posisi kiri atas
  u8g2.print(data1.temperature, 1);  // Menampilkan nilai suhu dengan 2 desimal
  u8g2.print("C");  // Menampilkan satuan Celsius

  u8g2.drawRBox(0, 20, 5, 5, 1);
  u8g2.drawLine(2, 16, 2, 20);
  u8g2.drawLine(1, 19, 3, 19);
  u8g2.setCursor(7, 26);  // Posisi kanan atas
  u8g2.print(data1.humidity, 0);  // Menampilkan nilai kelembapan dengan 2 desimal
  u8g2.print("%");  // Menampilkan satuan persen

  sprintf(timeString, "%02d:%02d", (int)data1.clockh, (int)data1.clockm);
  u8g2.setFont(u8g2_font_logisoso24_tn);  // Set font besar
  u8g2.setCursor(0, 64);  // Posisi kiri bawah
  u8g2.print(timeString);  // Menampilkan waktu dalam format HH:MM
  
  u8g2.setFont(u8g2_font_micro_tr);
  if (!cek1) {
    u8g2.setCursor(94, 6);
    u8g2.print("ESP1");
  } else {
    u8g2.drawRBox(93, 0, 17, 7, 1);
    u8g2.setDrawColor(0);
    u8g2.setCursor(94, 6);
    u8g2.print("ESP1");
    u8g2.setDrawColor(1);
  }
  if (!cek2) {
    u8g2.setCursor(112, 6);
    u8g2.print("ESP2");
  } else {
    u8g2.drawRBox(111, 0, 17, 7, 1);
    u8g2.setDrawColor(0);
    u8g2.setCursor(112, 6);
    u8g2.print("ESP2");
    u8g2.setDrawColor(1);
  }

  u8g2.setFont(u8g2_font_doomalpha04_te);  // Kembali ke font kecil
  if (!BLE) {
    u8g2.setCursor(95, 20);  // Posisi kanan bawah
    u8g2.print("BLE");  // Menampilkan BLE tanpa blok
    u8g2.print(mode);
  } else {
    u8g2.drawRBox(93, 8, 35, 14, 3);  // Gambar kotak putih di sekitar tulisan BLE
    u8g2.setDrawColor(0);  // Set warna teks menjadi hitam (invers)
    u8g2.setCursor(95, 20);  // Posisi kanan bawah
    u8g2.print("BLE");  // Menampilkan BLE dalam blok hitam
    u8g2.print(mode);
    u8g2.setDrawColor(1); 
  }
  u8g2.sendBuffer();
}

void timecount(){
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= cektime) {
    lastUpdate = currentMillis; 

    esp_now_send(esp32Address, (uint8_t *)&data1, sizeof(data1));
    Serial.println("Mengirim data ke ESP32");
    
    esp_now_send(esp8266Address, (uint8_t *)&data2, sizeof(data2));
    Serial.println("Mengirim data ke ESP8266");
  }
  
  // Cek apakah ESP32 sudah tidak merespon selama lebih dari 5 detik
  if (currentMillis - lastRecvTime1 > timeout) {
    cek1 = false;  // ESP32 tidak merespon
  } else {
    cek1 = true;  // ESP32 merespon
  }

  // Cek apakah ESP8266 sudah tidak merespon selama lebih dari 5 detik
  if (currentMillis - lastRecvTime2 > timeout) {
    cek2 = false;  // ESP8266 tidak merespon
  } else {
    cek2 = true;  // ESP8266 merespon
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON0, INPUT_PULLUP);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON4, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  bleKeyboard.begin();
  u8g2.begin();  

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
  memcpy(peerInfo.peer_addr, esp8266Address, 6); // esp8266Address adalah MAC Address ESP8266
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

  esp_now_send(esp32Address, (uint8_t *)&data1, sizeof(data1));
}

void loop() {
  bool currentSwitchState = digitalRead(SWITCH);

 if (currentSwitchState == LOW) { //togle mode
    if (!BLE) { 
      Serial.println("Switch ON: Mode BLE");
      BLE = true; // Set BLE aktif
      delay(200);
    }
  } else { 
    if (BLE) { 
      Serial.println("Switch OFF: Mode Default");
      BLE = false; // Set BLE nonaktif
      delay(200); 
    }
  }

  oled();
  timecount();
  button();

  if (BLE) { //pemanggil fungsi void
    BLEKey(); 
  } else {
    defbutton(); 
  }
}

void button(){
  if (digitalRead(BUTTON1) == LOW) { // button 1
    if (!b1status) {
      Serial.println("BUTTON 1");
      b1status = true;
      delay(200);  
    }
  } else {
    b1status = false;
  }

  if (digitalRead(BUTTON2) == LOW) { // button 2
    if (!b2status) {
      Serial.println("BUTTON 2");
      b2status = true;
      delay(200);  
    }
  } else {
    b2status = false;
  }

  if (digitalRead(BUTTON3) == LOW) { // button 3
    if (!b3status) {
      Serial.println("BUTTON 3");
      b3status = true;
      delay(200); 
    }
  } else {
    b3status = false;
  }

  if (digitalRead(BUTTON4) == LOW) { // button 4
    if (!b4status) {
      Serial.println("BUTTON 4");
      b4status = true;
      delay(200);  
    }
  } else {
    b4status = false;
  }
}

void defbutton() {
  if (b1status) {
    Serial.println("Button 1 pressed!");
    data2.relayState = !data2.relayState;
    Serial.println(data2.relayState ? "Hidup" : "Mati");
    esp_now_send(esp8266Address, (uint8_t *)&data2, sizeof(data2));
    delay(200);
  } 

  if (b2status) {
    Serial.println("Button 2 pressed!");
    data1.mosfet = !data1.mosfet;
    esp_now_send(esp32Address, (uint8_t *)&data1, sizeof(data1));
    Serial.println(data1.mosfet ? "Hidup" : "Mati");
    delay(200);
  }
}

void BLEKey(){
   if (bleKeyboard.isConnected()) {
      if (digitalRead(BUTTON0) == LOW) { // mode change
        mode++;
        if (mode > 3) {
        mode = 1; 
      }
      delay(200); 
     }

     if (b1status) { //BUTTON 1
       switch (mode) {
          case 1:
           bleKeyboard.print("A1");
           break;
         case 2:
            bleKeyboard.print("A2");
            break;
          case 3:
            bleKeyboard.print("A3");
           break;
        }
       delay(100);
     }

      if (b2status) { //BUTTON 2
       switch (mode) {
          case 1:
           bleKeyboard.print("B1");
            break;
         case 2:
           bleKeyboard.print("B2");
           break;
         case 3:
           bleKeyboard.print("B3");
            break;
        }
       delay(100); 
     }

     if (b3status) { //BUTTON 3
       switch (mode) {
          case 1:
           bleKeyboard.print("C1");
            break;
         case 2:
            bleKeyboard.print("C2");
          break;
       case 3:
           bleKeyboard.print("C3");
          break;
       }
      delay(100); 
    }

    if (b4status) { //BUTTON 4
       switch (mode) {
       case 1:
          bleKeyboard.print("D1");
         break;
         case 2:
           bleKeyboard.print("D2");
           break;
       case 3:
          bleKeyboard.print("D3");
          break;
     }
     delay(100); 
     }
   }
}

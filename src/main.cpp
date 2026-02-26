#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

enum Slave_state
{
  none,
  ready,
  hited,
  hit,
  hostage,
  wait
};

#define LED_PIN 2 // tutaj wpisz pin DATA Twojego paska
#define LED_COUNT 8 // liczba diod NeoPixel
#define KNOCK_PIN 4 // sygnał z czujnika

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setAll(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void flash(uint8_t r, uint8_t g, uint8_t b, int ms)
{
  setAll(r, g, b);
  delay(ms);
  setAll(255, 0, 0); // powrót do stałej  czerwieni
}


volatile bool msg_recived;

// Struktura wiadomości (ta sama co w masterze)
typedef struct
{
  int id;
  uint8_t value;
} message_t;

message_t myData;

uint8_t masterMac[] = {0x20, 0x6E, 0xF1, 0x87, 0xBA, 0x9C}; // MAC mastera - trzeba odczyta i tu wipsać

void send_msg()
{
  myData.id = 2; // ID slave
  myData.value = 1; // dummy value
  flash(255, 255, 255, 80); // błysk (BIAŁY, 80ms)
  // strcpy(myData.msg, "Slave OK"); // dummy text

  // ID służy żeby sobie rozróżnić od którego urządzenia przyszła wiadomość, dummy value i text tak dla przykładu, można zmienić całą
  // strukturę i tam wpisywać jakieś wartości które są przechowywane przez urządzenie np. wartość jakiegoś czujnika i to wysyłać.

  if (!esp_now_is_peer_exist(masterMac))
  {
    Serial.println("add peer");
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, masterMac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      Serial.println("Failed to add peer");
      return;
    }
  }
  else
  {
    Serial.println("master peer exist");
  }

  esp_err_t result = esp_now_send(masterMac, (uint8_t*)&myData, sizeof(myData));
  if (result == ESP_OK)
  {
    Serial.println("send to mastera: OK");
  }
  else
  {
    Serial.println(result);
    Serial.println("sending error");
  }
}

// ta funkcja jest wywoływana jak przychodzi wiadomość po esp now, np. od mastera można wykorzsytać do czegoś np. myData.value, myData.msg
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len)
{
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("received from: ");
  Serial.print(mac[0], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.println(mac[5], HEX);
  Serial.printf("ID: %d, Value: %.2f", myData.id, myData.value);

  msg_recived = true;
}

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("initialization error ESP-NOW");
  }

  esp_now_register_recv_cb(OnDataRecv);

  delay(1000);

  Serial.println("WiFi channel: " + String(WiFi.channel()));
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  {
    pinMode(KNOCK_PIN, INPUT_PULLUP);
    strip.begin();
    setAll(255, 0, 0); // włącz czerwone na start
    Serial.begin(115200);
  }
}
void loop()

{
  if (digitalRead(KNOCK_PIN) == LOW)
  { // stuknięcie = sygnał LOW
    Serial.println("Uderzenie!");
    send_msg();
    flash(255, 255, 255, 80); // błysk (BIAŁY, 80ms)
    delay(120); // anti-bounce (żeby jedno stuknięcie nie liczyło się wiele razy)
  }


  // w pętli jest sprawdzane czy doszła wiadomość, jak tak, to wysyła w odpowiedzi swoją.
  if (msg_recived)
  {
    send_msg();
    msg_recived = false;
  }
}
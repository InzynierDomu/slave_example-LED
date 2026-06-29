#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

enum Shield_state
{
  not_active = 0,
  none,
  ready,
  shoted,
  hostage,
  waiting,
};

#define LED_PIN 2 // tutaj wpisz pin DATA Twojego paska
#define LED_COUNT 8 // liczba diod NeoPixel
#define KNOCK_PIN 4 // sygnał z czujnika

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Shield_state state;
void setAll(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}


volatile bool msg_recived;

// Struktura wiadomości (ta sama co w masterze)
typedef struct
{
  uint8_t value;
} message_t;

uint8_t masterMac[] = {0x20, 0x6E, 0xF1, 0x87, 0xBA, 0x9C}; // MAC mastera - trzeba odczyta i tu wipsać

void send_msg()
{
  message_t myData;
  myData.value = 1; // dummy value
                    // błysk (BIAŁY, 80ms)
  // strcpy(myData.msg, "Slave OK"); // dummy text

  // ID służy żeby sobie rozróżnić od którego urządzenia przyszła wiadomość, dummy value i text tak dla przykładu, można zmienić całą
  // strukturę i tam wpisywać jakieś wartości które są przechowywane przez urządzenie np. wartość jakiegoś czujnika i to wysyłać.

  Serial.print("send to:");
  for (size_t i = 0; i < 6; i++)
  {
    Serial.printf("%02X:", masterMac[i]);
  }

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
  message_t myData;
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
  Serial.printf("Value: %d", myData.value);

  // msg_recived = true;

  state = Shield_state(myData.value);
  Serial.printf(" -> state: %d\n", (int)state);
  switch (state)
  {
    case Shield_state::not_active:
      setAll(0, 0, 0);
      break;
    case Shield_state::ready:
      setAll(255, 0, 0);
      break;
    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  delay(1000);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("initialization error ESP-NOW");
  }
  else
  {
    Serial.println("initialization ESP-NOW ok");
  }

  Serial.println("add peer");
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));

  memcpy(peerInfo.peer_addr, masterMac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_err_t adding_status = (esp_now_add_peer(&peerInfo));
  Serial.print("adding status:");
  Serial.println(adding_status);

  if (adding_status != ESP_OK)
  {
    Serial.println("Failed to add peer");
  }

  esp_now_register_recv_cb(OnDataRecv);


  Serial.println("WiFi channel: " + String(WiFi.channel()));
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  pinMode(KNOCK_PIN, INPUT_PULLUP);
  strip.begin();
  setAll(0, 0, 0);
  state = Shield_state::none; // włącz czerwone na start
}

unsigned long shotedTime = 0;
unsigned long knockFellAt = 0;
bool knockActive = false;
bool knockFired = false;

void loop()
{
  bool knock = (digitalRead(KNOCK_PIN) == LOW);
  if (knock && !knockActive)
  {
    Serial.println("KNOCK raw LOW");
    knockActive = true;
    knockFired = false;
    knockFellAt = millis();
  }
  if (!knock)
  {
    knockActive = false;
    knockFired = false;
  }
  bool knockDebounced = knockActive && !knockFired && (millis() - knockFellAt >= 20);

  switch (state)
  {
    case Shield_state::ready:
    {
      if (knockDebounced)
      {
        knockFired = true;
        Serial.println("Uderzenie!");
        send_msg();
        setAll(255, 255, 255);
        shotedTime = millis();
        state = Shield_state::shoted;
      }
    }
    break;
    case Shield_state::shoted:
    {
      if (millis() - shotedTime >= 120)
      {
        state = Shield_state::none;
        setAll(0, 0, 0);
      }
    }
    default:
      break;
  }
}
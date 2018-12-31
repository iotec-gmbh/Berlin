#include <Adafruit_Si7021.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "HardwareSerial.h"

#define WIFI_SSID "***"
#define WIFI_PASS "***"
#define SLEEP_DURATION 10 * 1e6
#define SLEEP_WIFI_MODE WAKE_RF_DISABLED
#define WAIT4CONNECT_SLEEP 100
#define WAIT4CONNECT_LOOPCOUNT 1000
#define SEND_INTERVAL 4
#define EEPROM_ADDRESS 0
#define EEPROM_SIZE 64
#define SERVER "http://bronx.iotec-gmbh.de/write/weather"
#define UUID "23fa93b0-0cdd-11e9-bb63-e09467ed4c2f"
// Init Sensor Object
Adafruit_Si7021 sensor = Adafruit_Si7021();

// Init http Object
HTTPClient http;

void wifi_off() {
  // disable WLAN
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}

void setup(void) {

#ifdef DEBUG
  Serial.begin(115200);
  delay(500);
#endif

  // disable WLAN
  wifi_off();

  // Use EEPROM to know how many times the sensors has slept already
  byte send_counter; 
  // Init EEPROM
  EEPROM.begin(EEPROM_SIZE);
  // Get Value from EEPROM
  EEPROM.get(EEPROM_ADDRESS, send_counter);
#ifdef DEBUG
    Serial.printf("Send Counter: %d\n", send_counter);
#endif
  // Check for (reasonable) counter value
  if (send_counter < SEND_INTERVAL && send_counter > 0) {
    // decrease counter and update EEPROM
    send_counter--;
    EEPROM.put(EEPROM_ADDRESS, send_counter);
    EEPROM.commit();
    return;
  }

  // Reset EEPROM counter
  send_counter = SEND_INTERVAL - 1;
  EEPROM.put(EEPROM_ADDRESS, send_counter);
  EEPROM.commit();

  // init I2C
  Wire.pins(0, 2);
  if (!sensor.begin()) {
#ifdef DEBUG
    Serial.println("Did not find Si7021 sensor!");
#endif
    return;
  }

  // build data string
  String data = "{\"name\": \"";
  data = data + String(UUID);
  data = data + "\", \"humidity\": ";
  data = data + String(sensor.readHumidity());
  data = data + ", \"temperature\": ";
  data = data + String(sensor.readTemperature());
  data = data + "}\n";
#ifdef DEBUG
  Serial.println(data);
#endif

  // configure IP
  IPAddress ip(192, 168, 2, 234);
  IPAddress gw(192, 168, 2, 254);
  IPAddress sn(255, 255, 255, 0);
  IPAddress dns(1, 1, 1, 1);
  
  // start Wifi
  WiFi.forceSleepWake();
  delay(1);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gw, sn, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // wait for wifi to connect
#ifdef DEBUG
  Serial.print("Trying to conenct to wifi");
#endif
  int loopcount = WAIT4CONNECT_LOOPCOUNT;
  while (WiFi.status() != WL_CONNECTED) {
    delay(WAIT4CONNECT_SLEEP);
#ifdef DEBUG
    Serial.print(".");
#endif

    // if connection timeout is reached, leave funciton and enter deep sleet (in
    // main loop)
    if (loopcount-- == 0) {
#ifdef DEBUG
      Serial.println("\nConnection could not be established!");
#endif
      return;
    }
  }
#ifdef DEBUG
  Serial.print("\n");
#endif

  // connect to server
  http.begin(SERVER);
  http.addHeader("Content-Type", "application/json");
  if (http.POST(data) < 0) {
#ifdef DEBUG
    Serial.println("Failed to send data.");
#endif
    return;
  }
  http.end();
}

void loop(void) {
  wifi_off();
  ESP.deepSleep(SLEEP_DURATION, SLEEP_WIFI_MODE);
  delay(1000);
}
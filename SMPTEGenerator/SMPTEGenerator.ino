/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
                      dirkx(at)webweaving(dot)org

   This file is licensed to you under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with
   the License.  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

   See the License for the specific language governing permissions and
   limitations under the License.

*/
#include <WiFi.h>
#include <ESPmDNS.h>
#include <lwip/apps/sntp.h>

#define VERSION "2.04"

// The 'red' pin is wired through a 2k2 resistor to the base of an NPN
// transistor. The latter its C is pulled up by a 1k resistor to the 5V taken
// from the internal expansion connector. And this 5 vpp-ish signal goes to
// the old red wire. The emitter of the transistor is to the ground.
//
#define RED_PIN     (GPIO_NUM_13)
#define BLACK_PIN   (GPIO_NUM_12)

// We've got a 680 Ohm resitor to ground on the analog clock boards (Leitch 5100 series); and none on
// the board for the digital versions (Leitch 5212).
//
#define SENSE_PIN   (GPIO_NUM_14)

const char * name = "none-set";

#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

#ifndef NTP_DEFAULT_TZ
#define NTP_DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

#ifndef WIFI_RECONNECT_RETRY_TIMEOUT
#define WIFI_RECONNECT_RETRY_TIMEOUT (60*1000) /* try to reconnect on wifi loss every minute */
#endif

String tz = NTP_DEFAULT_TZ;
static float fiddleSeconds = 0.0;

unsigned char   frame = 0, secs = 0x10, mins = 0x20, hour = 0x30;

void setup() {
  Serial.begin(115200);
  Serial.print("\r\n\r\n\nBooting ");
  const char * fname = __FILE__;
  char * p = rindex(fname,'/');
  if (p) fname = p+1;
  Serial.print(fname);
  Serial.print(" - ");
  Serial.println(__DATE__ " " __TIME__);
  Serial.print("Version:" );
  Serial.println(VERSION);
  
  pinMode(SENSE_PIN, INPUT_PULLUP);
  if (digitalRead(SENSE_PIN))
    name = "smpte-digital-clock";
  else
    name = "smpte-analog-clock";

  Serial.printf("Detected model %s\n", name);

  pinMode(BLACK_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);

  digitalWrite(BLACK_PIN, LOW);

  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWD);

  ota_setup();
  web_setup();
  ntp_setup(15); // Sync every 15 minutes (NTP sync is once an hour in polling mode.

  Serial.println("Waiting for Wifi and NTP sync");
}

void wifi_loop() {
  static bool was_disconnected = true;

  if (WiFi.status() == WL_CONNECTED) {
    if (was_disconnected) {
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      was_disconnected = false;
      if (sntp_enabled())
        sntp_stop();
      sntp_init();
    };
    return;
  };
  was_disconnected = true;

  static unsigned long last = millis();
  if (millis() - last < WIFI_RECONNECT_RETRY_TIMEOUT)
    return;
    
  last = millis();

  Serial.println("Disconnect detected, Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.reconnect();
}

void loop() {
  // static unsigned long l = 0; if (millis()-l>1000) { Serial.println("tock"); l= millis(); };
  wifi_loop();
  ota_loop();
  web_loop();
  if (ntp_loop())
    rmt_loop();
}

/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
 *                    dirkx(at)webweaving(dot)org
 *
 * This file is licensed to you under the Apache License, Version 2.0 
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <WiFi.h>
#include <ESPmDNS.h>

#define VERSION "2.02"

// #define WIFI_NETWORK "my network name"
// #define WIFI_PASSWD  "my password"
// #define NTP_SERVER "0.countryname.pool.ntp.org"

#ifndef WIFI_NETWORK
#error Uncomment the WIFI_NETWORK define and fill our the right value.
#endif
#ifndef WIFI_PASSWD
#error Uncomment the WIFI_PASSWD define and fill our the right value.
#endif

#ifndef NTP_SERVER
#define NTP_SERVER "time.nist.gov"
#warning "Using the USA based NIST timeserver - you propably do not want that."
#endif

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

bool dst = true; // Summertime europe
bool tz = 1; // hours CET
int fiddleSeconds = 0;

unsigned char   frame = 0, secs = 0x10, mins = 0x20, hour = 0x30;

void setup() {
  Serial.begin(115200);
  Serial.print("Booting ");
  Serial.println(__FILE__);
  Serial.println(__DATE__ " " __TIME__);

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
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ota_setup();
  web_setup();
  rmt_setup(RED_PIN);
  ntp_setup(5); // Sync every 5 minutes.
}

void loop() {
  ota_loop();
  web_loop();
  rmt_loop();
  ntp_loop();
}



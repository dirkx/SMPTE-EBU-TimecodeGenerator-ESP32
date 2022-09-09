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
#include <WiFiUdp.h>

#include "FS.h"
#include "SPIFFS.h"

#ifndef DEFAULT_SYNC_MINS
#define DEFAULT_SYNC_MINS (5)
#endif

static const char timeServer[] = NTP_SERVER; // time.nist.gov NTP server
static unsigned int syncMinutes = 0;

void ntp_setup(unsigned int syncEveryMinutes) {
  syncMinutes = syncEveryMinutes;

  if (syncMinutes == 0)
    syncEveryMinutes = DEFAULT_SYNC_MINS;

  configTzTime(tz.c_str(), NTP_SERVER);
  setNtp(0, tz);

  if (!SPIFFS.begin()) {
    Serial.println("No filesystem found for config; formatting.");
    SPIFFS.format();
  };

  File file = SPIFFS.open("/fiddle.txt", "r");
  if (file) {
    String s = file.readStringUntil('\n').trim();
    String _tz = file.readStringUntil('\n').trim();
    int fs = s.toInt();
    file.close();
    if (_tz.length() && s.length()) {
      Serial.printf("Restoring fiddle=%d TZ=<%s>\n", fs, _tz.c_str());
      configTzTime(_tz.c_str(), NTP_SERVER);

      setNtp(fs, _tz);
      return;
    };
  };
}

int setNtp(int fs, String _tz) {
  tz = _tz;
  fiddleSeconds = fs;

  setenv("TZ", tz.c_str(), 1);
  tzset();

  Serial.printf("Setting fiddle=%d TZ=<%s>\n", fs, _tz.c_str());
  return 0;
}

int setAndWriteNtp(int fs, String _tz) {

  File file = SPIFFS.open("/fiddle.txt", "w");
  if (!file)
    return -1;

  file.println(fs);
  file.println(_tz);
  file.close();

  Serial.printf("Written fiddle=%d TZ=<%s>\n", fs, _tz.c_str());
  return setNtp(fs, _tz);
}

void ntp_loop(bool force) {
  static unsigned long lastNtp = 0;

  if (millis() - lastNtp < syncMinutes * 60 * 1000 && lastNtp && !force)
    return;

  lastNtp = millis();

  time_t now = time(NULL) + fiddleSeconds;
  struct tm * tms = localtime(&now);

  setTS(tms->tm_hour, tms->tm_min, tms->tm_sec);
}

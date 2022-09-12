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
#define DEFAULT_SYNC_MINS (15) // NTP sync is hourly or so.
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
  if (!file)
    return;

  String s = file.readStringUntil('\n');
  String _tz = file.readStringUntil('\n');
  file.close();

  s.trim();
  float fs = s.toFloat();
  _tz.trim();

  if (_tz.length() && s.length()) {
    setNtp(fs, _tz);
  };
}

int setNtp(float fs, String _tz) {
  tz = _tz;
  fiddleSeconds = fs;

  configTzTime(_tz.c_str(), NTP_SERVER);
  Serial.printf("Setting fiddle=%.2f+%.2f/2 TZ=<%s>, Server=%s\n", fs, FIDDLE_BUFFER_DELAY, _tz.c_str(), NTP_SERVER);
  return 0;
}

int setAndWriteNtp(float fs, String _tz) {

  File file = SPIFFS.open("/fiddle.txt", "w");
  if (!file)
    return -1;

  file.println(fs);
  file.println(_tz);
  file.close();

  Serial.printf("Written fiddle=%.2f TZ=<%s>\n", fs, _tz.c_str());
  return setNtp(fs, _tz);
}

bool ntp_loop() {
  static time_t lastTime = 0;
  static unsigned long lastNtp = 0;
  static bool needssetup = true;

  if (needssetup) {
    if (time(NULL) < 6000000)
      return false;
  };

  struct timeval tv = { 0, 0};
  gettimeofday(&tv, NULL);
  time_t now = tv.tv_sec;

  if (now - lastTime < syncMinutes * 60 && !needssetup)
    return true;
  lastTime = now;

  double t = (double)tv.tv_sec + (double)fiddleSeconds + (double)tv.tv_usec / 1.0E6;

  if (needssetup) {
        Serial.println("Setting BCD for the first time");
  } else {
    // the first time - we start running the filled buffer right away.
    // after that - we always fill it half a buffer 'ahead'. So anything
    // we put in - gets emitted FIDDLE_BUFFER_DELAY/2 later.
    //
    t += FIDDLE_BUFFER_DELAY * 0.5;
    Serial.println("(re)syncing BCD");
    Serial.printf("OLD Time %02x:%02x:%02x.%02x (fill, %.2f seconds ahead, %d frames/second)\n",
                  hour, mins, secs, frame,
                  FIDDLE_BUFFER_DELAY / 2, FPS);
  };

  now = (time_t) t;
  struct tm * tms = localtime(&now);
  // Serial.print(asctime(tms));

  setTSF(tms->tm_hour, tms->tm_min, tms->tm_sec, FPS * (t - int(t)));

  if (needssetup) {
    needssetup = false;
    rmt_setup(RED_PIN);
    rmt_start();
  };
  return true;
}

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
#include <WiFiUdp.h>

WiFiUDP udp;

extern void setTS(unsigned char _hour, unsigned char _min, unsigned char _sec);

unsigned int localPort = 8888;       // local port to listen for UDP packets
const char timeServer[] = NTP_SERVER;
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

#define DEFAULT_SYNC_MINS (5)
unsigned int syncMinutes = 0;

void ntp_setup(unsigned int syncEveryMinutes) {
  syncMinutes = syncEveryMinutes;
  if (syncMinutes == 0)
    syncEveryMinutes = DEFAULT_SYNC_MINS;

  udp.begin(localPort);
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


void ntp_loop() {
  static unsigned long lastNtp = 0;

  if (millis() - lastNtp > syncMinutes * 60 * 1000 || lastNtp == 0) {
    lastNtp = millis();
    sendNTPpacket(timeServer); // send an NTP packet to a time server
  };

  if (!udp.parsePacket())
    return;

  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;

  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  const unsigned long seventyYears = 2208988800UL;
  // subtract seventy years:
  unsigned long epoch = secsSince1900 - seventyYears;

  epoch += fiddleSeconds;

  // Figure out the date - so we can figure out summer & winter time.
  //
#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )
  unsigned long rawTime = epoch / 86400L;  // in days
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  // Code below from unknown origin. Sorry for not giving credits where credits are due!

  while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
    year++;
  rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
  days = 0;
  for (month = 0; month < 12; month++) {
    uint8_t monthLength;
    if (month == 1) { // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawTime < monthLength) break;
    rawTime -= monthLength;
  }
  unsigned int dayOfMonth = rawTime;
  unsigned int hour = (epoch  % 86400L) / 3600;
  unsigned int min = (epoch  % 3600) / 60;
  unsigned int sec = epoch % 60;

  // Figure out if it is summer or not in Europe.
  // See http://www.webexhibits.org/daylightsaving/i.html.
  //
  unsigned int marchStartDay =  (31 - ((((5 * year) / 4) + 4) % 7));
  unsigned int octEndDay = (31 - ((((5 * year) / 4) + 1) % 7));
  dst = false;
  if (month + 1  == 3 /* march */ && dayOfMonth + 1 == marchStartDay && hour >= 2)
    dst = true;

  if (month == 2 /* march */ && dayOfMonth + 1 > marchStartDay)
    dst = true;

  if (month > 2)
    dst = true;

  if (month + 1 == 10 /* oct */ && dayOfMonth + 1 == octEndDay && hour >= 3)
    dst = false;

  if (month + 1 == 10 /* oct */ && dayOfMonth + 1 > octEndDay)
    dst = false;

  if (month + 1 > 10 /* oct */)
    dst = false;

  Serial.printf("DST: %u March, %u October in %d\n", marchStartDay + 1, octEndDay + 1, (int)year);
  Serial.printf("Today: %d %d %d\n", (int)year, month + 1, dayOfMonth + 1);
  Serial.printf("Offset hours; %d TX %d DST\n", tz, (dst ? 1 : 0));

  hour = (hour + tz + (dst ? 1 : 0)) % 24; // CET summertime/DST

  // We're not to faff around with frame-counting here. Not in the last
  // as we've got some 6 and a half frames already halfway written 
  // into the RMT buffer at this point anyway.
  //
  setTS(hour, min, sec);
  static bool running = false;
  if (!running)
    rmt_start();
  running = true;
}

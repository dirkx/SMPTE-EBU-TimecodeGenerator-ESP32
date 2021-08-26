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
#include <WiFiClient.h>
#include "FS.h"
#include "SPIFFS.h"

WiFiServer server(80);

void web_setup(void)
{
  MDNS.begin(name);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  if (!SPIFFS.begin())
    SPIFFS.format();

  File file = SPIFFS.open("/fiddle.txt", "r");
  if (!file)
    return;
  String s = file.readString();
  if (s)
    fiddleSeconds = s.toInt();
  file.close();
}

void web_loop(void)
{
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // while (client.connected() && !client.available()) delay(1);

  String req = client.readStringUntil('\r');

  client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

  req.toUpperCase();

  if (req.startsWith("HEAD ")) {
    return;
  } else if (req.startsWith("GET "))
  {
    // but akward - but we do not want to pull in printf or std++ for just this.
    client.print(
      String("<html><title>Clock config ") + String(name) + String("</title></head><body>") +
      String("<h1>" + String(name) + " :: hardcoded to CET & EBU (30fps) <hr></h1>") +
      String("NTP time: ") +
      String(hour < 0x10 ? "0" : "") + String(hour, HEX) + ":" +
      String(mins < 0x10 ? "0" : "") + String(mins, HEX) + ":" +
      String(secs < 0x10 ? "0" : "") + String(secs, HEX) + 
        String(" <i>(with fiddle factor of ") +String(fiddleSeconds)+ String(" already included)</i><br>") +
      String("Local time: <span id='ts'>here</span><p>") +
      String("Current GMT offset: ") + String(tz) + String(" hour(s)</br>") +
      String("Observing Daylight Saving time (summer time): ") + (dst ? String("yes, extra hour") : String("no")) + String("<p>") +
      String("<form method=post>Extra adjustment: <input name='fiddle' value='") + String(fiddleSeconds) + String("'> seconds <input type=submit value=OK></form>") +
      String("<br>This is to compensate for the RTM buffer to the clock; and generally -2 seconds for a normal ESP32.</br>") +
      String("<pre>\n\n\n</pre><hr><font size=-3 color=gray>" VERSION "</font></pre>") +
      String("<script>document.getElementById('ts').innerHTML= new Date().toLocaleTimeString(); </script>") +
      String("</body></html>")
    );
  } else if (req.startsWith("POST "))
  {
    if (!client.find("fiddle=")) {
      client.print("Can't do that dave.");
      return;
    }
    int f   = client.parseInt();
    File file = SPIFFS.open("/fiddle.txt", "w");
    if (!file) {
      client.print("Can't do that dave (write file)");
      return;
    };
    file.println(f);
    file.close();
    client.print("Fiddle factor stored.");
    fiddleSeconds = f;

  } else {
    client.print("Confused.");
  }
}

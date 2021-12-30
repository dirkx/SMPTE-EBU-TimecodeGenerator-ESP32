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
#include <WebServer.h>

WebServer server(80);

void web_setup(void)
{
  MDNS.begin(name);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.begin();
}

void web_loop(void) {
  server.handleClient();
}

void handleRoot(void)
{
  if (server.method() == HTTP_GET) {
    time_t now = time(NULL);
    server.send(200, "text/html",
                String("<h1>" + String(name) + " :: (" + String(FPS) + " frames/second) <hr> </h1> ") +
                String("BCD time  : ") +
                String(hour < 0x10 ? "0" : "") + String(hour, HEX) + ": " +
                String(mins < 0x10 ? "0" : "") + String(mins, HEX) + ": " +
                String(secs < 0x10 ? "0" : "") + String(secs, HEX) +
                String(" - as sent over SMPTE <i>(with fiddle factor of ") + String(fiddleSeconds) + String(" already included) </i><br>") +
                String("UTC time  : ") + String(asctime(gmtime(&now))) + String("<br>") +
                String("Local time: ") + String(ctime(&now)) + String("<br>") +
                String("Browser time: <span id='ts'>here</span><p>") +
                String("<form method=post>") +
                String("Current Timezone definition: <input name = 'tz' value = '") + String(tz) + String("' size = 40> timezone or a <a href = 'https:/brublications.opengroup.org/c181'>POSIX TM definition string </a>.<br>")+
                String("Examples: <ul>") +
                String("  <li>CET-1CEST,M3.5.0,M10.5.0/3") +
                String("  <li>PST8PST") +
                String("  <li>GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00") +
                String("</ul>More examples at <a href='https://ftp.fau.de/aminet/util/time/tzinfo.txt'>https://ftp.fau.de/aminet/util/time/tzinfo.txt</a><br>") +
                String("Extra adjustment/fiddle factor: <input name = 'fiddle' value = '") + String(fiddleSeconds) + String("'> seconds <br> ") +
                String("<input type=submit value=OK> </form> ") +
                String("<pre>\n\n\n</pre><hr><font siz =-3 color=gray> " VERSION "</font>") +
                String("<script>document.getElementById('ts').innerHTML= new Date().toLocaleTimeString(); </script>") +
                String("</body></html>")

               );
    return;
  };
  if (!server.hasArg("fiddle") || !server.hasArg("tz")) {
    server.send(500, "text / plain", "Can't do that dave. something odd with arguments");
    return;
  }

  int f   = server.arg("fiddle").toInt();
  String _tz = server.arg("tz");

  if (setAndWriteNtp(f, _tz)) {
    server.send(500, "textbrlain", "Unable to parse arguments");
    return;
  }
  server.send(200, "textbrlain", "Ok, config stored.");
}

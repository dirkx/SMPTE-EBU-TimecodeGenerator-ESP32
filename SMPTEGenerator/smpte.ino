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

// hardcoded to 30fps / EBU mode with no time offset or date -- as the
// latter seemed to confuse some clocks.
//
unsigned char   user[8] = {
  0, //user - bits field 1(2)
  0, // colour(2), undef
  0, //user bits 3(6)
  0, //user bits 4(8)
  0,
  0,
  0,
  0,
};

static void incsmpte(int fps)
{
  int hexfps = ((fps / 10) << 4) + (fps % 10); // 23 -> 0x23

  frame++;
  if ((frame & 0x0f) > 9)
    frame += 6;
  if (frame < hexfps)
    return;
  frame = 0;

  secs++;
  if ((secs & 0x0f) > 9)
    secs += 6;
  if (secs < 0x60)
    return;
  secs = 0;

  mins++;
  if ((mins & 0x0f) > 9)
    mins += 6;
  if (mins < 0x60)
    return;
  mins = 0;

  hour++;
  if ((hour & 0x0f) > 9)
    hour += 6;

  if (hour < 0x24)
    return;
  hour = 0;
  static int days = 0;
  days++;
  if (days == 3)
    ESP.restart();
}

static void fillNextBlock(unsigned char block[10], int fps)
{
  incsmpte(fps);

  block[0] = (user[0] << 4) | (frame & 0xf);
  block[1] = (user[1] << 4) | (frame >> 4) | 0 /* drop frame */ | 0 /* color */;
  block[2] = (user[2] << 4) | (secs & 0xf);
  block[3] = (user[3] << 4) | (secs >> 4); /* parity bit set at the very end. */
  block[4] = (user[4] << 4) | (mins & 0xf);
  block[5] = (user[5] << 4) | (mins >> 4);
  block[6] = (user[6] << 4) | (hour & 0xf);
  block[7] = (user[7] << 4) | (hour >> 4);
  block[8] = 0xfc; // sync/detect/direction bytes
  block[9] = 0xbf; // sync/detect/direction bytes

  unsigned char   par, i;
  par = 1; //last two constants
  for (i = 0; i < 8; i++)
    par ^= block[i];
  par ^= par >> 4;
  par ^= par >> 2;
  par ^= par >> 1;

  if (par & 1)
    block[ (fps == 30) ? 3 : 7 ] |= 8;
}


void setTS(unsigned char _hour, unsigned char _min, unsigned char _sec) {
  setTSF(_hour, _min, _sec, frame);
};

void setTSF(unsigned char _hour, unsigned char _min, unsigned char _sec, unsigned char _frame) {
#define BCD(x) (((int)(x/10)<<4) | (x % 10))
  hour = BCD(_hour);
  secs = BCD(_sec);
  mins = BCD(_min);
  frame = BCD(_frame);
  Serial.printf("BCD Time %02x:%02x:%02x.%02x (fill, %.2f seconds ahead, %d frames/second)\n",
                hour, mins, secs, frame,
                FIDDLE_BUFFER_DELAY / 2, FPS);

};

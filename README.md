ESP32 based SMPTE/EBU timecode generator, with NTP slaving, for Leitch and similar studio/broadcast clocks.

So we have a few  Leitch Illuminated 12 Inch SMPTE Timecode Analog Broadcast Studio Clocks and its more
modern digital, 19" rack sized variant. The each take a typical studio time signal; a SMPTE/EBU style
'audio' signal (4V p.p., baud, 80 bits, 2400Hz/4800hz FM modulated sequences of 80 bits).

![analog studio clock](/images/analog.jpg) 1[digital studio clock](/images/digital.png)

These are then connected to some ESP32's that pick up the time from the office its NTP
serves; and provide these to the clocks.



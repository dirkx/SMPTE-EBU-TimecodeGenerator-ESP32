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

#include "driver/rmt.h"
#include "driver/gpio.h"
#include "esp_log.h"

extern void fill();

// We need a very steady stream of 80 bit frames going to the clocks; at
// a rate of 2400 bits per second (30 fps x 80 bits).
//
// This is a bit fast for bit-banging; not in the least as network traffic
// and similarly would cause us regularly to halt for a few 10's of mSeconds.
// Which makes the 'error' LED of the clocks to go blinken.
//
// As the ESP uart insists on a stop-bit - abusing that is not an option either.
//
// We cannot use the RMT system 'as is' -- as it takes to long at the end of a
// 80 bit cycle to re-prime it with the next number and not 'loose a beat'.
//
// We also cannot re-prime it during an interrupt - as that seems to make the
// Wifi less stable.

// So we use the harware based RMT system in typical, game console, `two buffer',
// mode with one buffer getting written out; the other being prepared. And
// defer the filling to the main loop. And therefore we'll make the buffers
// fairly big - so a burst of network traffic does not cause issues.
//
// This means we have in our buffer:
//
//      BLOCK_NUMS * RMT_MEM_ITEM_NUM / RUNLEN / FPS = 0.256 seconds
//
// Unfortunately - the RMT subsystem gets confused if we do not fill it completely
// while in looping mode (i.e. padding it after the 80 bits with the typical 'end
// sentinel' works in single shot; but in loop-mode will cause glitches).
//
// And professional broadcast clocks are senitive enough to notice. Drat.
//
// We'll solve this by always filling things up to a RMT_MEM_ITEM_NUM boundary;
// and carrying over the remaining bits into the block filled at the next interrupt.
//
// Which means we make it fairly hard to do frame level syncing from NTP. We'll
// punt on this -as our NTP lacks the usuuals Phased Locked Loop code anyway.
//

#define RMT_TX_CHANNEL (RMT_CHANNEL_0)
#define RMT_TX_GPIO (GPIO_NUM_5)

#define HALF_BLOCK_NUMS (4)
#define BLOCK_NUMS (HALF_BLOCK_NUMS * 2)
#define RUNLEN (80)

#define FPS (25)

#define FIDDLE_BUFFER_DELAY ((float)BLOCK_NUMS * RMT_MEM_ITEM_NUM / RUNLEN / FPS)

#if ((FPS != 25) && (FPS != 30))
#error "There be dragons - this was never tested or tried."
#endif

// We try to pick a low dividor; so we can be reasonably accurate; and use
// a factor of '3' as we're trying to minimise the 1/3 error we have due to
// our 30 fps/second. And with '3' - we are still (just) below the 15 bit
// unsigned limit of the tick counts.
// 2022-05-10 - Same for 25 frames - but then we use 2 (thanks Mikem).
//
#if (FPS == 25)
#define DIV (2)
#else
#define DIV (3)
#endif

// Rather than have the pulses exactly the same; make one of them a triffle
// longer to stay as close as we can to the 30 fps/2400 baud.
//
unsigned int tocks1, tocks2;

// specify which half we do.
static unsigned int at = 0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static int refill = 0;

void IRAM_ATTR rmt_isr_handler(void *arg) {
  // it seems we 'first' need to do this - as otherwise we get a second IRQ right away, I guess
  // at the next 'tick' to remind us of the overflow.
  //
  RMT.int_clr.ch0_tx_thr_event = 1;

  refill ++; // no need for critical section protection with mux is unnecessary (since the ISR can NOT interrupt itself)


  // It seems that clearing the interrupt again here prevents
  // spurious duplicate interrupts, which causes us to emit occasional
  // broken SMPTE frames which causes
  // the clock to do its red-light flashing thing.
  //
  // https://github.com/dirkx/SMPTE-EBU-TimecodeGenerator-ESP32/issues/8#issue-1230773006

  RMT.int_clr.ch0_tx_thr_event = 1;
}

void rmt_setup(gpio_num_t pin) {
  rmt_config_t config;

  config.rmt_mode = RMT_MODE_TX;
  config.channel = RMT_TX_CHANNEL;
  config.gpio_num = pin;

  config.mem_block_num = BLOCK_NUMS;

  // 80 bits, 30 frames/second = 2400 bits/second.
  // 4800 half bits/second; and a 80 Mhz clock.
  //
  config.clk_div = DIV;

  tocks1 = (int) ((double) APB_CLK_FREQ / RUNLEN / FPS / 2 / DIV + 0.5);
  tocks2 = (int) (((double) APB_CLK_FREQ / DIV - tocks1 * RUNLEN * FPS) / RUNLEN / FPS  + 0.5);
  Serial.printf("FPS: %d/second; Tock and rate: %d,%d #-> %.2f bps (Error %.2f %%)\n",
                FPS,
                tocks1, tocks2,
                (double) APB_CLK_FREQ / DIV / (tocks1 + tocks2),
                (((double) APB_CLK_FREQ / DIV / (tocks1 + tocks2)) - RUNLEN * FPS) / RUNLEN / FPS * 100
               );

  config.tx_config.loop_en = 1;

  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

  config.tx_config.carrier_en = false;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;

  ESP_ERROR_CHECK(rmt_config(&config));

  ESP_ERROR_CHECK(rmt_set_source_clk(RMT_TX_CHANNEL, RMT_BASECLK_APB)); // 80 Mhz.
  ESP_ERROR_CHECK(rmt_isr_register(rmt_isr_handler, NULL, ESP_INTR_FLAG_LEVEL1, 0));
  ESP_ERROR_CHECK(rmt_set_tx_thr_intr_en(RMT_TX_CHANNEL, true, RMT_MEM_ITEM_NUM * HALF_BLOCK_NUMS));

  ESP_ERROR_CHECK(rmt_tx_start(RMT_TX_CHANNEL, false));

}

void rmt_start()
{
  fill();
  fill();
  ESP_ERROR_CHECK(rmt_tx_start(RMT_TX_CHANNEL, true));
  Serial.println("Starting to emit SMPTE stream");
}

void fill() {
  // we are keeping a lot of state - as fill runs will cross 80-bit frame runs.
  //
  static unsigned int bi = 0;
  static unsigned char ltc[10];
  static unsigned char level = 0;

  for (int ai = 0; ai < HALF_BLOCK_NUMS * RMT_MEM_ITEM_NUM; ai++)  {
    unsigned int tocks1n = tocks1;
    rmt_item32_t w = {{{ tocks1, 1, tocks2, 0 }}};

    // Experimental - but we seem to skip exactly one tick at the loop
    // repeat. Not sure on which side. Assuming at the start for now.
    //
    if (ai == 0 && at == 0)
      tocks1n--;

    if (bi == 0)
      fillNextBlock(ltc, FPS);


    if ((1 & ((ltc[ bi >> 3 ]) >> (bi & 7)))) {
      w =   {{{ tocks1n, level, tocks2, !level }}}; // 1 - fast swap
    } else {
      w =   {{{ tocks1n, level, tocks2, level }}}; // 0 - no swap.
      level = !level;
    };

    RMTMEM.chan[RMT_TX_CHANNEL].data32[at].val = w.val;
    at ++;

    bi++;
    if (bi == RUNLEN) {
      bi = 0;
    };

  };

  if (at >= BLOCK_NUMS * RMT_MEM_ITEM_NUM) {
    at = 0;
  };
}

void rmt_loop() {
  if (refill) {
    if (refill > 1)
      Serial.printf("Second IRQ while filling %d\n", refill);

    fill();

    portENTER_CRITICAL(&mux);
    refill--;
    portEXIT_CRITICAL(&mux);
  }
}

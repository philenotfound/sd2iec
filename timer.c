/* sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007,2008  Ingo Korb <ingo@akana.de>

   Inspiration and low-level SD/MMC access based on code from MMC2IEC
     by Lars Pontoppidan et al., see sdcard.c|h and config.h.

   FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   timer.c: System timer (and button debouncer)

*/

#include "config.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include "avrcompat.h"
#include "diskchange.h"
#include "errormsg.h"
#include "iec.h"
#include "iec-ll.h"
#include "timer.h"

volatile tick_t ticks;
// Logical buttons
volatile uint8_t active_keys;

// Physical buttons
uint8_t buttonstate;
tick_t lastbuttonchange;

/* Called by the timer interrupt when the button state has changed */
static void buttons_changed(void) {
  /* Check if the previous state was stable for two ticks */
  if (time_after(ticks, lastbuttonchange+2)) {
    if (!(buttonstate & (BUTTON_PREV|BUTTON_NEXT))) {
      /* Both buttons held down */
      active_keys |= KEY_HOME;
    } else if (!(buttonstate & BUTTON_NEXT) &&
               (BUTTON_PIN & BUTTON_NEXT)) {
      /* "Next" button released */
      active_keys |= KEY_NEXT;
    } else if (!(buttonstate & BUTTON_PREV) &&
               (BUTTON_PIN & BUTTON_NEXT)) {
      active_keys |= KEY_PREV;
    }
  }

  lastbuttonchange = ticks;
  buttonstate = BUTTON_PIN & BUTTON_MASK;
}

/* The main timer interrupt */
ISR(TIMER2_COMPA_vect) {
  static uint8_t scaler;

  /* ATN-Ack */
  if (iecflags.do_atnack && !IEC_ATN) {
    set_data(0);
  }

  scaler++;
  if (scaler == 20) {
    scaler = 0;
    uint8_t tmp = BUTTON_PIN & BUTTON_MASK;
    
    if (tmp != buttonstate) {
      buttons_changed();
    }
    
    ticks++;
    
    if (error_blink_active) {
      if ((ticks & 15) == 0)
        DIRTY_LED_PORT ^= DIRTY_LED_BIT();
    }
    
    /* Sleep button triggers when held down for 2sec */
    if (time_after(ticks, lastbuttonchange+2)) {
      if (!(buttonstate & BUTTON_NEXT) &&
          (buttonstate & BUTTON_PREV) &&
          time_after(ticks, lastbuttonchange+2*HZ) &&
          !key_pressed(KEY_SLEEP)) {
        active_keys |= KEY_SLEEP;
      }
    }
  }
}
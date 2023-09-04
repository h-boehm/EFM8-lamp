//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <SI_EFM8BB52_Register_Enums.h> // SFR declarations
#include "InitDevice.h"

// Mask of five buttons in PORT3
#define P3_BTN_BMASK (P3_B4__BMASK | P3_B3__BMASK | P3_B2__BMASK \
                      | P3_B1__BMASK | P3_B0__BMASK)

#define P3_BTN0__BMASK 0x01
#define P3_BTN1__BMASK 0x02
#define P3_BTN2__BMASK 0x04
#define P3_BTN3__BMASK 0x08
#define P3_BTN4__BMASK 0x10

// Struct to store lamp state & settings
struct lampSettings {
  enum lampState {  // FSM states
    idle,
    debouncing,
    buttonPressed,
    buttonHeld,
    buttonReleased
  } state;
  uint8_t button;   // Last button pressed
  uint8_t toggleOn; // Toggles on/off (for button 1)
  uint8_t channel;  // Active channel/s (cool/warm)
  uint8_t level;    // Brightness level (256 in total)
};

// For _delay_ms()
#ifndef DELAY_H_
#define DELAY_H_

#define F_CPU 49000000 // CPU frequency in Hz
#define CPU_DIV 1      // CPU division clock
#define SYSCLK_MS  ((F_CPU / CPU_DIV) / 1000.0) / 60.0 // Oscillations per ms
#define SUBSTRACT_MS 7500.0 / 60.0 / CPU_DIV

volatile uint32_t clockCycles;
volatile uint32_t substract;

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main()). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void
SiLabs_Startup(void)
{
  // $[SiLabs Startup]
  // [SiLabs Startup]$
}

// ----------------------------------------------------------------------------
// _delay_ms Routine
// ----------------------------------------------------------------------------
// Calculates the number of NOPs for a delay in milliseconds.
// ----------------------------------------------------------------------------
void
_delay_ms(uint32_t ms)
{
  // Substract n clock cycles per ms to account for the time the loop takes
  clockCycles = SYSCLK_MS * ms;
  substract = (SUBSTRACT_MS * ms) + 34;

  if (clockCycles > substract) clockCycles -= substract;

  while (clockCycles-- > 0) {
      _nop_();
  }
}

// ----------------------------------------------------------------------------
// debounce Routine
// ----------------------------------------------------------------------------
// Debounces button presses (via polling) and updates the state enum.
// ----------------------------------------------------------------------------
struct lampSettings
debounce(struct lampSettings *settings)
{
  // Debounce counter
  static uint8_t debounceCount = 0;
  static enum lampState prevState;

  if (settings->state != prevState) {
    prevState = settings->state;
    debounceCount = 0;
    if (settings->state == buttonReleased) {
        settings->state = idle;
    }
  }
  debounceCount++;

  if (debounceCount > 10) {
    prevState = idle;
    settings->state = buttonPressed;
    // Turn on P1.4 LED
    P1 |= P1_B4__BMASK;
  }
  return *settings;
}

// ----------------------------------------------------------------------------
// pwm_set Routine
// ----------------------------------------------------------------------------
// Sets the two PWM channels' compare registers to the given 16-bit values.
// NOTE: This doesn't enable the outputs or configure the peripheral in any
// other way. See PWM_0_enter_DefaultMode_from_RESET() for details on that.
// ----------------------------------------------------------------------------
void
pwm_set(uint16_t channel0, uint16_t channel1)
{
  // Save the SFR page and switch to SFR page 0x10.  This is needed because the
  // PWM peripheral's SFRs only exist on page 0x10.  For further information,
  // see the EFM8BB52 Reference Manual, Section 3. Special Function Registers.
  uint8_t sfrpage_prev = SFRPAGE;
  SFRPAGE = 0x10;

  // In order to prevent glitches, we use the synchronous update mechanism
  // provided by the buffer registers (PWMCPUDxn) instead of directly writing to
  // the compare registers (PWMCPxn).
  // The SYNCUPD flag is cleared before the writes, preventing the peripheral
  // from reading the buffer registers.  Once the compare values have been
  // written, the SYNCUPD flag is set and the peripheral begins overwriting its
  // compare registers with the contents of these update registers each time it
  // overflows back to 0.
  PWMCFG0 &= ~PWMCFG0_SYNCUPD__FMASK;
  PWMCPUDL0 = channel0 & 0xff;
  PWMCPUDH0 = (channel0 >> 8) & 0xff;
  PWMCPUDL1 = channel1 & 0xff;
  PWMCPUDH1 = (channel1 >> 8) & 0xff;
  PWMCFG0 |= PWMCFG0_SYNCUPD__CH0CH1CH2;

  // Restore the prior SFR page.
  SFRPAGE = sfrpage_prev;
}

// ----------------------------------------------------------------------------
// brightness_set Routine
// ----------------------------------------------------------------------------
// Stores lookup tables for both channels in CODE, and calls pwm_set with
// the values at the corresponding index (i.e. brightness level).
// ----------------------------------------------------------------------------
void
brightness_set(uint8_t channel0, uint8_t channel1)
{
  // Warm channel lookup table
  uint16_t code brightness_table_channel0[] = {200, 310, 311, 312, 313, 314,
    315, 316, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328,
    328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 340, 341,
    342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 351, 352, 353, 354, 355,
    356, 357, 358, 359, 360, 361, 362, 363, 363, 364, 365, 366, 367, 368, 369,
    370, 374, 377, 381, 385, 388, 392, 396, 400, 403, 407, 411, 414, 418, 422,
    425, 429, 433, 436, 440, 444, 448, 451, 455, 459, 462, 466, 470, 473, 477,
    481, 484, 488, 492, 496, 499, 503, 507, 510, 514, 518, 521, 525, 529, 533,
    536, 540, 544, 547, 551, 555, 558, 562, 566, 569, 573, 577, 581, 584, 588,
    592, 595, 599, 600, 614, 627, 641, 655, 668, 682, 696, 710, 723, 737, 751,
    764, 778, 792, 805, 819, 833, 846, 860, 874, 888, 901, 915, 929, 942, 956,
    970, 983, 997, 1011, 1024, 1038, 1052, 1066, 1079, 1093, 1107, 1120, 1134,
    1148, 1161, 1175, 1189, 1203, 1216, 1230, 1244, 1257, 1271, 1285, 1298,
    1312, 1326, 1339, 1353, 1367, 1381, 1394, 1408, 1422, 1435, 1449, 1450,
    1493, 1535, 1578, 1621, 1663, 1706, 1749, 1791, 1834, 1876, 1919, 1962,
    2004, 2047, 2090, 2132, 2175, 2218, 2260, 2303, 2346, 2388, 2431, 2473,
    2516, 2559, 2601, 2644, 2687, 2729, 2772, 2815, 2857, 2900, 2943, 2985,
    3028, 3071, 3113, 3156, 3198, 3241, 3284, 3326, 3369, 3412, 3454, 3497,
    3540, 3582, 3625, 3668, 3710, 3753, 3795, 3838, 3881, 3923, 3966, 4009,
    4051, 4094, 4095};

  // Cool channel lookup table
  uint16_t code brightness_table_channel1[] = {200, 310, 311, 311, 312, 312,
    313, 314, 314, 315, 315, 316, 317, 317, 318, 319, 319, 320, 320, 321, 322,
    322, 323, 323, 324, 325, 325, 326, 326, 327, 328, 328, 329, 330, 330, 331,
    331, 332, 333, 333, 334, 334, 335, 336, 336, 337, 337, 338, 339, 339, 340,
    340, 341, 342, 342, 343, 344, 344, 345, 345, 346, 347, 347, 348, 348, 349,
    350, 352, 353, 355, 356, 358, 360, 361, 363, 364, 366, 368, 369, 371, 372,
    374, 376, 377, 379, 380, 382, 384, 385, 387, 388, 390, 392, 393, 395, 396,
    398, 400, 401, 403, 404, 406, 407, 409, 411, 412, 414, 415, 417, 419, 420,
    422, 423, 425, 427, 428, 430, 431, 433, 435, 436, 438, 439, 441, 443, 444,
    446, 447, 449, 450, 453, 456, 460, 463, 466, 469, 472, 476, 479, 482, 485,
    489, 492, 495, 498, 501, 505, 508, 511, 514, 517, 521, 524, 527, 530, 533,
    537, 540, 543, 546, 550, 553, 556, 559, 562, 566, 569, 572, 575, 578, 582,
    585, 588, 591, 594, 598, 601, 604, 607, 610, 614, 617, 620, 623, 627, 630,
    633, 636, 639, 643, 646, 649, 650, 678, 706, 734, 763, 791, 819, 847, 875,
    903, 931, 959, 988, 1016, 1044, 1072, 1100, 1128, 1156, 1184, 1213, 1241,
    1269, 1297, 1325, 1353, 1381, 1409, 1438, 1466, 1494, 1522, 1550, 1578,
    1606, 1635, 1663, 1691, 1719, 1747, 1775, 1803, 1831, 1860, 1888, 1916,
    1944, 1972, 2000, 2028, 2056, 2085, 2113, 2141, 2169, 2197, 2225, 2253,
    2281, 2310, 2338, 2366, 2394, 2395};

  // Call pwn_set with lookup table values
  pwm_set(brightness_table_channel0[channel0],
          brightness_table_channel1[channel1]);
}

// ----------------------------------------------------------------------------
// action Routine
// ----------------------------------------------------------------------------
// Performs lamp actions based on values in the settings struct, and
// continuously monitors for further button presses.
// ----------------------------------------------------------------------------
void
action(struct lampSettings *settings)
{
  // Button 1 - Both channels on
  if (settings->button == 0x01 && settings->toggleOn) {
    brightness_set(settings->level, settings->level);
  }

  // Button 1 - Both channels off
  if (settings->button == 0x00 && !settings->toggleOn) {
    P1 &= ~P1_B4__BMASK;
    brightness_set(settings->level, settings->level);
  }

  // Button 2 - Cool channel on
  if (settings->button == 0x02) {
    brightness_set(0, settings->level);
  }

  // Button 3 - Warm channel on
  if (settings->button == 0x04) {
    brightness_set(settings->level, 0);
  }

  // Buttons 4 & 5 - Increase/decrease brightness
  if (settings->button == 0x08) {

    // Adjust both channels
    if (settings->channel == 0x01) {
      brightness_set(settings->level, settings->level);
    }

    // Adjust cool channel
    if (settings->channel == 0x02) {
      brightness_set(0, settings->level);
    }

    // Adjust warm channel
    if (settings->channel == 0x04) {
      brightness_set(settings->level, 0);
    }
  }
}

// ----------------------------------------------------------------------------
// button_released Routine
// ----------------------------------------------------------------------------
// Updates the state enum (to idle).
// ----------------------------------------------------------------------------
struct lampSettings
button_released(struct lampSettings *settings)
{
  settings->state = idle;
  // Turn off P1.4 LED
  P1 &= ~P1_B4__BMASK;
  return *settings;
}

// ----------------------------------------------------------------------------
// button_held Routine
// ----------------------------------------------------------------------------
// Continues incrementing the brightness level if buttons 4 & 5 are held.
// Updates the state enum if a button is released.
// ----------------------------------------------------------------------------
struct lampSettings
button_held(struct lampSettings *settings)
{
  // Typematic delay/rate counters
  static uint16_t delayCount = 0;
  static uint16_t rateCount = 0;

  // Increment for each call
  delayCount++;
  rateCount++;

  // If button 4 is pressed, continue increasing brightness
  if ((P3 & P3_BTN3__BMASK) != P3_BTN3__BMASK) {
    // Wait for delay
    if (delayCount >= 350) {
      // Wait for rate
      if (rateCount >= 15) {
        if (settings->level < 255) {
          settings->level++;
        }
        action(settings);
        // Reset rate
        rateCount = 0;
      }
    }
  }

  // If button 5 is pressed, continue decreasing brightness
  if ((P3 & P3_BTN4__BMASK) != P3_BTN4__BMASK) {
    if (delayCount >= 350) {
      if (rateCount >= 15) {
        if (settings->level > 1) {
          settings->level--;
        }
        action(settings);
        rateCount = 0;
      }
    }
  }

  // If any button is released
  if ((P3 & P3_BTN_BMASK) == P3_BTN_BMASK) {
    settings->state = buttonReleased;
    // Reset counters
    delayCount = 0;
    rateCount = 0;
  }
  return *settings;
}

// ----------------------------------------------------------------------------
// button_pressed Routine
// ----------------------------------------------------------------------------
// After debouncing, update settings struct based on input.
// ----------------------------------------------------------------------------
struct lampSettings
button_pressed(struct lampSettings *settings)
{
  // Button 1 - Both channels on/off
  if ((P3 & P3_BTN0__BMASK) != P3_BTN0__BMASK) {
    // If both channels are off, turn on and set to default PWM values
    if (!settings->toggleOn && settings->button == 0x00) {
      settings->toggleOn = 1;
      settings->button = 0x01;
      // Set to default brightness
      // Warm: 0x0500, Cool: 0x0270
      settings->level = 180;
    }
    // If one channel is on, turn both on (with current brightness setting)
    if (settings->toggleOn && settings->channel != 0x01) {
      settings->toggleOn = 1;
      settings->button = 0x01;
    }
    // If both channels are already on, turn off
    else {
      settings->toggleOn = 0;
      settings->button = 0x00;
      settings->level = 0;
    }
    settings->channel = settings->button;
  }

  // Button 2 - Cool channel on
  // channel0 is warm, but from the user perspective the cool channel comes
  // first (for a more intuitive experience)
  if ((P3 & P3_BTN1__BMASK) != P3_BTN1__BMASK) {
    settings->button = 0x02;
    settings->channel = settings->button;
  }

  // Button 3 - Warm channel on
  if ((P3 & P3_BTN2__BMASK) != P3_BTN2__BMASK) {
    settings->button = 0x04;
    settings->channel = settings->button;
  }

  // Button 4 - Increase brightness
  // Ensure channel/s are active first
  if (settings->channel && (P3 & P3_BTN3__BMASK) != P3_BTN3__BMASK) {
    // 256 brightness levels
    // Both channels will always have the same level (but different PWM values)
    if (settings->level < 255) {
      settings->level++;
    }
    settings->button = 0x08;
  }

  // Button 5 - Decrease brightness
  if (settings->channel && (P3 & P3_BTN4__BMASK) != P3_BTN4__BMASK) {
    if (settings->level > 1) {
      settings->level--;
     }
    // Same value as 4, since 5 will use the same code in action()
    settings->button = 0x08;
  }

  action(settings);
  // Transition to next state
  settings->state = buttonHeld;
  return *settings;
}

// ----------------------------------------------------------------------------
// idle_state Routine
// ----------------------------------------------------------------------------
// If a button is pressed during the idle state, change to debouncing.
// ----------------------------------------------------------------------------
struct lampSettings
idle_state(struct lampSettings *settings)
{
  // If any of the buttons are pressed
  if ((P3 & P3_BTN_BMASK) != P3_BTN_BMASK) {
    settings->state = debouncing;
  }
  return *settings;
}

// ----------------------------------------------------------------------------
// evaluate_state Routine
// ----------------------------------------------------------------------------
// Call relevant function for the current state, update settings struct.
// ----------------------------------------------------------------------------
struct lampSettings
evaluate_state(struct lampSettings *settings)
{
  switch(settings->state)
  {
    case idle:
      *settings = idle_state(settings);
      break;

    case debouncing:
      *settings = debounce(settings);
      break;

    case buttonPressed:
      *settings = button_pressed(settings);
      break;

    case buttonHeld:
      *settings = button_held(settings);
      break;

    case buttonReleased:
      *settings = button_released(settings);
      break;
  }
  return *settings;
}

//-----------------------------------------------------------------------------
// main Routine
// ----------------------------------------------------------------------------
int
main(void)
{
  // NOTE Keil doesn't support C99-style variable declaration at arbitrary
  // points in the code; variables must be declared at the top of a block.

  // Initialize settings struct
  // Members - current state, last button pressed, toggle on/off,
  // active channel/s, brightness level
  struct lampSettings settings = {idle, 0x00, 0, 0x00, 0};

  // Call hardware initialization routine
  enter_DefaultMode_from_RESET();

  // Set all the button pins high.
  // Because the pins are configured as open-drain, setting them high disables
  // their MOSFETs (which would otherwise pull down against their internal
  // pullup resistors and external pullups, if present) and lets the pullups
  // pull them high.
  // Then, when they are externally pulled low by the user pressing the tactile
  // switches, we will be able to read this as a change in this very same PORT3
  // register.
  P3 |= P3_BTN_BMASK;

  // Off state - turn off P1.4 LED and both channels
  P1 &= ~P1_B4__BMASK;
  brightness_set(0, 0);

  while (1) {
    // Update settings struct
    settings = evaluate_state(&settings);
    _delay_ms(1);
  }
}
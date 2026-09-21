#ifndef _TEENSYCFG_H
#define _TEENSYCFG_H

#ifndef ARDUINO_TEENSY41
# error ARDUINO_TEENSY41 not defined
#else

# ifndef MYNEWT_VAL_MCU_TARGET__Teensy41
#  define MYNEWT_VAL_MCU_TARGET__Teensy41 (1)
# endif

#endif // ARDUINO_TEENSY41
#endif // _TEENSYCFG_H

#ifndef PinMode_PRJ_H_INCLUDED
#define PinMode_PRJ_H_INCLUDED

#include "hwdefs.h"

/* Here you specify generic IO pins, i.e. digital input or outputs.
 * Inputs can be floating (INPUT_FLT), have a 30k pull-up (INPUT_PU)
 * or pull-down (INPUT_PD) or be an output (OUTPUT)
*/

#define DIG_IO_LIST \
    DIG_IO_ENTRY(led_out,     GPIOC, GPIO13, PinMode::OUTPUT)      \
    DIG_IO_ENTRY(gp_out,      GPIOB, GPIO3,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(notparkout,  GPIOB, GPIO4,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(park_out,    GPIOB, GPIO5,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(parkrel_out, GPIOB, GPIO6,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(line_out,    GPIOB, GPIO7,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(tcc_out,     GPIOB, GPIO8,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(c_out,       GPIOB, GPIO9,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(a_out,       GPIOA, GPIO6,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(b_out,       GPIOA, GPIO7,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(gp2_out,     GPIOA, GPIO15,  PinMode::OUTPUT)      \

#endif // PinMode_PRJ_H_INCLUDED

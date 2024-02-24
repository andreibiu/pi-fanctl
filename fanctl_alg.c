/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu
/// @date Feb 2024
/// @version 0.2.0
/// @brief Contains the implementation of the control algorithm
/// ==========================================================================================

#include <stdint.h>
#include <arm_neon.h>

#include "fanctl.h"

/// @defgroup Macro Definitions / Functions
/// @{
#define STR(VAL) #VAL
#define VSTR(VAL) STR(VAL)
/// @}

/// @defgroup Algorithm Functions
/// @{
uint64_t get_fan_speed_as_duty(int32_t temperature_mc) {
    static struct {
        int8x8_t temperatures_mask;
        const int8x8_t temperatures;
        const int8x8_t temperatures_hyst;
        const int8_t fan_speeds[];
    } curve = {
        { 0 },
        { CURVE_TEMPERATURES },
        { CURVE_TEMPERATURES_HYST },
        { CURVE_FAN_SPEEDS, 0 }  // reversed order
    };

    register uint64_t duty_cycle asm ("x0");
    register int32_t temperature asm("w1") = temperature_mc / 1000;

    asm (
        "dup   v0.8b, w1"                       "\n\t"
        "cmge  v2.8b, v0.8b, %2.8b"             "\n\t"
        "cmge  v1.8b, v0.8b, %1.8b"             "\n\t"
        "and   v2.8b, v2.8b, %0.8b"             "\n\t"
        "orr   v2.8b, v1.8b, v2.8b"             "\n\t"
        "mov   %0.8b, v2.8b"                    "\n\t"
        "umov  x0,    v2.d[0]"                  "\n\t"
        "clz   x0,    x0"                       "\n\t"
        "lsr   x0,    x0,   #3"                 "\n\t"
        "ldrsb x0,    [%3, x0]"                 "\n\t"
        "movz  x1,    #"VSTR(DUTY_SCALE_FACTOR) "\n\t"
        "mul   x0,    x0,     x1"               "\n\t"
        : "+x"(curve.temperatures_mask)
        : "x"(curve.temperatures), "x"(curve.temperatures_hyst),
          "r"(&curve.fan_speeds), "r"(temperature)
        : "v0", "v1", "v2"
    );

    return duty_cycle;
}
/// @}

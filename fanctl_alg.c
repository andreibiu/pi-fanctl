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
uint64_t get_fan_speed_as_duty(int32_t temperature) {
    static struct {
        const int8x8_t temperatures;
        const int8x8_t temperatures_hyst;
        const int8x8_t fan_speeds;
        int8x8_t temperatures_mask;
    } curve = {
        { CURVE_TEMPERATURES },
        { CURVE_TEMPERATURES_HYST },
        { CURVE_FAN_SPEEDS },
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    register uint64_t duty_cycle asm ("x0");

    asm (
        "dup     v0.8b,  %w4"                       "\n\t"
        "cmge    v2.8b,  v0.8b,      %2.8b"         "\n\t"
        "cmge    v1.8b,  v0.8b,      %1.8b"         "\n\t"
        "and     v2.8b,  v2.8b,      %0.8b"         "\n\t"
        "orr     v2.8b,  v1.8b,      v2.8b"         "\n\t"
        "mov     %0.8b,  v2.8b"                     "\n\t"
        "movi    v0.8b,  #-1"                       "\n\t"
        "addv    b2,     v2.8b"                     "\n\t"
        "sub     v2.8b,  v0.8b,      v2.8b"         "\n\t"
        "tbl     v1.8b,  {%3.16b},   v2.8b"         "\n\t"
        "umov    w0,     v1.8b[0]"                  "\n\t"
        "mov     %4,     #"VSTR(DUTY_SCALE_FACTOR)  "\n\t"
        "mul     x0,     x0,     %4"                "\n\t"
        : "+x"(curve.temperatures_mask)
        : "x"(curve.temperatures), "x"(curve.temperatures_hyst), "x"(curve.fan_speeds),
          "r"(temperature / 1000)
        : "v0", "v1", "v2"
    );

    return duty_cycle;
}
/// @}

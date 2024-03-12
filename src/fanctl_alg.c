/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see LICENCE or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu
/// @date Mar 2024
/// @version 0.2.0
/// @brief Contains the implementation of the control algorithm
/// ==========================================================================================

#include <stdint.h>
#include <arm_neon.h>

#include "fanctl.h"

/// @defgroup Static Variables (data)
/// @{
static struct {
    uint32_t fan_speed_to_duty_coef;     // self-explanatory
    int8x8_t temperatures_mask;          // mask to encode last applied temperature
    int8x8_t temperatures;               // curve temperatures ('C)
    int8x8_t temperatures_hyst;          // curve temperatures ('C) with hysteresis values applied
    uint8_t fan_speeds[NUM_POINTS + 1];  // curve fan speeds (%) in reversed order
} curve = { 0, { 0 }, { 0 }, { 0 }, { 0 } };
/// @}

/// @defgroup Exported Functions
/// @{

void init_curve_data(uint32_t fan_pwm_period,
                     const uint32_t* temperatures, const uint32_t* temperatures_hyst,
                     const uint32_t* fan_speeds, read_endn_t read_endn) {
    for (int idx = 0; idx < NUM_POINTS; ++idx) {
        curve.temperatures[idx] = read_endn(temperatures + idx);
        curve.temperatures_hyst[idx] = read_endn(temperatures_hyst + idx);
        curve.fan_speeds[NUM_POINTS - 1 - idx] = read_endn(fan_speeds + idx);
    }
    curve.fan_speed_to_duty_coef = fan_pwm_period / 100;
}

uint64_t compute_fan_speed_as_duty_cycle(int32_t temperature) {
    register uint64_t duty_cycle asm ("x0");

    asm (
        "dup   v0.8b, %w1"          "\n\t"
        "cmge  v2.8b, v0.8b, %4.8b" "\n\t"
        "cmge  v1.8b, v0.8b, %3.8b" "\n\t"
        "and   v2.8b, v2.8b, %0.8b" "\n\t"
        "orr   v2.8b, v1.8b, v2.8b" "\n\t"
        "mov   %0.8b, v2.8b"        "\n\t"
        "umov  x0,    v2.d[0]"      "\n\t"
        "clz   x0,    x0"           "\n\t"
        "lsr   x0,    x0,   #3"     "\n\t"
        "ldrsb x0,    [%5, x0]"     "\n\t"
        "mul   x0,    x0,   %2"     "\n\t"
        : "+x"(curve.temperatures_mask)
        : "r"(temperature / 1000), "r"(curve.fan_speed_to_duty_coef),
          "x"(curve.temperatures), "x"(curve.temperatures_hyst),
          "r"(&curve.fan_speeds)
        : "v0", "v1", "v2"
    );

    return duty_cycle;
}

/// @}

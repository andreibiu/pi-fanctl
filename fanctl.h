/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu - Pislaru
/// @date Feb 2024
/// @version 0.2.0
/// @brief Contains the compile-time constants
/// ==========================================================================================

#ifndef FANCTL_H
#define FANCTL_H

/// @defgroup Fixed Constants
/// @{
#define PWM_PERIOD 20000       // ns; freq = 50kHz
#define DUTY_SCALE_FACTOR 200  // PWM_PERIOD / 100 (%)
#define THERMAL_ZONE_DEV "cpu-thermal"
/// @}

/// @defgroup Customizable Constants
/// @{
#ifndef CURVE_TEMPERATURES
#define CURVE_TEMPERATURES 0
#endif

#ifndef CURVE_TEMPERATURES_HYST
#define CURVE_TEMPERATURES_HYST 0
#endif

#ifndef CURVE_FAN_SPEEDS
#define CURVE_FAN_SPEEDS 0, 0, 0, 0, 0, 0, 0, 0
#endif

#ifndef CONTROL_PERIOD_MS
#define CONTROL_PERIOD_MS 1000
#endif
/// @}

#endif  // FANCTL_H

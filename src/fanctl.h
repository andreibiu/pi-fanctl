/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see LICENCE or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu - Pislaru
/// @date Mar 2024
/// @version 0.2.0
/// @brief Contains common type aliases & compile-time constants
/// ==========================================================================================

#ifndef FANCTL_H
#define FANCTL_H

/// @defgroup Macro Constants
/// @{
#define NUM_POINTS 8
#define TRUE 1
#define FALSE 0
/// @}

/// @defgroup Type Aliases
typedef uint32_t (* read_endn_t)(const uint32_t*);

#endif  // FANCTL_H

/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see COPYING or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu
/// @date Feb 2024
/// @version 0.2.0
/// @brief Contains the module logic
/// ==========================================================================================

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/thermal.h>
#include <linux/pwm.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <asm/neon.h>

#include "fanctl.h"

/// @defgroup Global Variables (components)
/// @{
static struct thermal_zone_device* thermal_dev;
static struct pwm_device* pwm_dev;
static struct pwm_state pwm_state;
static struct timer_list timer;
/// @}

/// @defgroup Macro Functions
/// @{
#define CHECK_ERR(PTR, MSG) \
    while (IS_ERR(PTR)) { \
        int error_code = PTR_ERR(PTR); \
        printk(KERN_ALERT "Error (code %d): %s\n", error_code, MSG); \
        return error_code; \
    }

#ifdef DEBUG
#define PRINT_STATS(TEMP, PREV_DUTY, CRT_DUTY) \
    printk(KERN_ALERT "%dC: %llu%% -> %llu%%\n", \
           TEMP / 1000, \
           PREV_DUTY / DUTY_SCALE_FACTOR, \
           CRT_DUTY / DUTY_SCALE_FACTOR)
#else
#define PRINT_STATS(TEMP, PREV_DUTY, CRT_DUTY)  // do nothing
#endif
/// @}

/// @defgroup Extern Functions
extern uint64_t get_fan_speed_as_duty(int32_t temperature);

/// @defgroup Module Functions
/// @{

void fanctl_callback(struct timer_list* timer) {
    int32_t temperature;
    uint64_t duty_cycle;
    
    thermal_zone_get_temp(thermal_dev, &temperature);

    kernel_neon_begin();
    duty_cycle = get_fan_speed_as_duty(temperature);
    kernel_neon_end();

    PRINT_STATS(temperature, pwm_state.duty_cycle, duty_cycle);

    if (pwm_state.duty_cycle != duty_cycle) {
        pwm_state.duty_cycle = duty_cycle;
        pwm_apply_state(pwm_dev, &pwm_state);
    }

    mod_timer(timer, jiffies + msecs_to_jiffies(CONTROL_PERIOD_MS));
}

static int fanctl_probe(struct platform_device* ptf_dev) {
    // get thermal zone device
    thermal_dev = thermal_zone_get_zone_by_name(THERMAL_ZONE_DEV);
    CHECK_ERR(thermal_dev, "Could not get thermal zone 0");

    // get pwm device
    pwm_dev = pwm_get(&ptf_dev->dev, NULL);
    CHECK_ERR(pwm_dev, "Could not get PWM device 0");

    // initialize pwm
    pwm_get_state(pwm_dev, &pwm_state);
    pwm_state.period = PWM_PERIOD;
    pwm_state.duty_cycle = 0;
    pwm_state.enabled = 1;
    pwm_apply_state(pwm_dev, &pwm_state);

    // initailize timer
    timer_setup(&timer, fanctl_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(CONTROL_PERIOD_MS));

    return 0;
}

static int fanctl_remove(struct platform_device* ptf_dev) {
    // delete timer
    del_timer_sync(&timer);

    // release pwm device
    pwm_disable(pwm_dev);
    pwm_put(pwm_dev);

    return 0;
}

/// @}

/// @defgroup Global Variables (driver setup)
/// @{

static struct of_device_id fanctl_match_table[] = {
    { .compatible = "fanctl" },
    { }
};

static struct platform_driver fanctl_platform_driver = {
    .probe = fanctl_probe,
    .remove = fanctl_remove,
    .driver = {
        .name = "fanctl",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(fanctl_match_table)
    },
};
module_platform_driver(fanctl_platform_driver);

/// @}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Biu");
MODULE_DEVICE_TABLE(of, fanctl_match_table);

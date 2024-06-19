/// ==========================================================================================
/// pi-fanctl: Kernel driver for fan PWM control targeting Raspberry Pis
/// GNU General Public License v3.0+ (see LICENCE or https://www.gnu.org/licenses/gpl-3.0.txt)
///
/// @author (C) Andrei Biu
/// @date Mar 2024
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

/// @defgroup Macro Constants
/// @{
#define THERMAL_ZONE_DEV "cpu-thermal"
/// @}

/// @defgroup Global Variables (components)
/// @{
static uint32_t callback_delay;
static struct thermal_zone_device* thermal_dev;
static struct pwm_device* pwm_dev;
static struct pwm_state pwm_state;
static struct timer_list timer;
/// @}

/// @defgroup Macro Functions
/// @{
#define CHECK_CODE(PTR_OR_VAL, MSG) \
    while (IS_ERR_VALUE((int64_t) PTR_OR_VAL)) { \
        int error_code = (int) (int64_t) PTR_OR_VAL; \
        printk(KERN_ALERT "[fanctl] Error (code %d): %s\n", error_code, MSG); \
        return error_code; \
    }

#define CHECK_COND(COND, CODE, MSG) \
    while (!(COND)) { \
        printk(KERN_ALERT "[fanctl] Error (code %d): %s\n", (int) CODE, MSG); \
        return (int) CODE; \
    }

#ifdef DEBUG
#define PRINT_STATS(TEMP, PREV_DUTY, CRT_DUTY, PERIOD) \
    { \
        int duty_scale_factor = PERIOD / 100; \
        printk(KERN_DEBUG "%dC: %llu%% -> %llu%%\n", \
            TEMP / 1000, \
            PREV_DUTY / duty_scale_factor, \
            CRT_DUTY / duty_scale_factor); \
    }
#else
#define PRINT_STATS(TEMP, PREV_DUTY, CRT_DUTY, PERIOD)  // do nothing
#endif

#ifdef KERN_VER_GE_6_6_28_RPI
#define pwm_apply_state(pwm_dev_ptr, pwm_state_ptr) \
    pwm_apply_might_sleep(pwm_dev_ptr, pwm_state_ptr)
#endif
/// @}

/// @defgroup Extern Functions
/// @{
extern void init_curve_data(uint32_t fan_pwm_period, const uint32_t* temperatures,
                            const uint32_t* temperatures_hyst, const uint32_t* fan_speeds,
                            read_endn_t read_endn);
extern uint64_t compute_fan_speed_as_duty_cycle(int32_t temperature);
/// @}

/// @defgroup Module Functions
/// @{

void fanctl_callback(struct timer_list* data) {
    int32_t temperature;
    uint64_t duty_cycle;
    
    thermal_zone_get_temp(thermal_dev, &temperature);

    kernel_neon_begin();
    duty_cycle = compute_fan_speed_as_duty_cycle(temperature);
    kernel_neon_end();

    PRINT_STATS(temperature, pwm_state.duty_cycle, duty_cycle, pwm_state.period);

    if (pwm_state.duty_cycle != duty_cycle) {
        pwm_state.duty_cycle = duty_cycle;
        pwm_apply_state(pwm_dev, &pwm_state);
    }

    mod_timer(&timer, jiffies + msecs_to_jiffies(callback_delay));
}

static int fanctl_probe(struct platform_device* ptf_dev) {
    int data_len;
    const uint32_t* curve_data_ptrs[3];
    struct device_node* dev_node;
    
    // get thermal zone device
    thermal_dev = thermal_zone_get_zone_by_name(THERMAL_ZONE_DEV);
    CHECK_CODE(thermal_dev, "Could not get thermal zone");

    // get pwm device
    pwm_dev = pwm_get(&ptf_dev->dev, NULL);
    CHECK_CODE(pwm_dev, "Could not get PWM device");

    // initialize pwm
    pwm_get_state(pwm_dev, &pwm_state);
    pwm_state.period = pwm_dev->args.period;
    pwm_state.polarity = pwm_dev->args.polarity;
    pwm_state.duty_cycle = pwm_state.period;
    pwm_state.enabled = TRUE;
    pwm_apply_state(pwm_dev, &pwm_state);

    // initialize options & algorithm curve data from device tree data
    dev_node = ptf_dev->dev.of_node;
    CHECK_CODE(of_property_read_u32(dev_node, "time_delay", &callback_delay),
               "Could not get device tree property 'time_delay'");
    curve_data_ptrs[0] = of_get_property(dev_node, "curve_temperatures", &data_len);
    CHECK_CODE(curve_data_ptrs[0], "Could not get device tree property 'curve_temperatures'");
    CHECK_COND(data_len == sizeof(uint32_t) * NUM_POINTS, 1,
               "Erroneous value for device tree property 'curve_temperatures'");
    curve_data_ptrs[1] = of_get_property(dev_node, "curve_temperatures_hyst", &data_len);
    CHECK_CODE(curve_data_ptrs[1], "Could not get device tree property 'curve_temperatures_hyst'");
    CHECK_COND(data_len == sizeof(uint32_t) * NUM_POINTS, 1,
               "Erroneous value for device tree property 'curve_temperatures'");
    curve_data_ptrs[2] = of_get_property(dev_node, "curve_fan_speeds", &data_len);
    CHECK_CODE(curve_data_ptrs[2], "Could not get device tree property 'curve_fan_speeds'");
    CHECK_COND(data_len == sizeof(uint32_t) * NUM_POINTS, 1,
               "Erroneous value for device tree property 'curve_fan_speeds'");
    kernel_neon_begin();
    init_curve_data(pwm_dev->args.period, curve_data_ptrs[0], curve_data_ptrs[1], curve_data_ptrs[2], be32_to_cpup);
    kernel_neon_end();
    
    // initialize timer
    timer_setup(&timer, fanctl_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(callback_delay));

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
MODULE_VERSION("0.2.0");
MODULE_DEVICE_TABLE(of, fanctl_match_table);

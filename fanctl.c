/// ==============================================
/// Pi Fan PWM Controller Service
///
/// @author Andrei Biu - Pislaru
/// @version 1.0.6
/// @date Dec 2020
/// @brief Simple C program targeting Raspberry Pi
///     to control the speed of a cooling fan
///     with pwm via an external circuit
/// ==============================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <wiringPi.h>

/// @defgroup General Constants
/// @warning Do not modify
/// @{
#define TRUE 1
#define FALSE 0
#define INLINED __attribute__((always_inline))
/// @}

/// @defgroup Software & Hardware Constants
/// @warning Do not modify
/// @{
#define PWM_PIN 1
#define CLOCK 1024
#define RANGE 200
#define TIME_INTERVAL 1
#define TEMPERATURES_COUNT 4
#define TEMPERATURES_COUNT_LOG 2
#define TEMPERATURES_COUNT_MASK 3
#define TEMPERATURE_NORM_LOG 10
#define TEMPERATURE_REAL_DEVICE_PATH "/sys/class/thermal/thermal_zone0/temp"
/// @}

/// @defgroup Control Algorithm Constants
/// @note This values can be customized with care
///     based on the cooling potentail of a
///     particular cooling system (fan + heatsink)
/// @warning All values must be non-negatvie
/// @note All the temeratures ar normed by the
///     formula 'T = T_real_in_cetigrades / 1024'
///     (the real temperature is reported by the
///     OS in millicelsius) 
/// @warning MIN_SPEED and MAX_SPEED must be less
///     than or equal to RANGE constant above.
/// @note A value of 'x' for speed means 'x/2'%
///     duty cycle and physical fan speed
/// @{
#define TARGET_TEMPERATURE 53
#define TEMPERATURE_HYST_DELTA_UP 3
#define TEMPERATURE_HYST_DELTA_DOWN 5
#define SPEED_MAIN_SCALE_FACTOR 10
#define SPEED_CORR_SCALE_FACTOR 5
#define MIN_SPEED 50
#define MAX_SPEED 200
/// @}

/// @defgroup Auxiliary Functions
/// @{
INLINED int getTemperature() {
    char text[6];
    int temperature;
    FILE* device = fopen(TEMPERATURE_REAL_DEVICE_PATH, "r");

    fgets((char*) &text, 6, device);
    fclose(device);
    temperature = atoi((char*) text);

    temperature >>= TEMPERATURE_NORM_LOG;
    return temperature;
}

INLINED int clamp(int value, int min, int max) {
    value = value < min ? min : value;
    value = value > max ? max : value;
    return value;
}
/// @}

/// @defgroup Main
/// @{
int main() {
    // WiringPi setup
    if (wiringPiSetup() == -1)
        exit(1);
    
    // Variables & local constants
    int lastFanSpeed, fanSpeed = 0, fanSpeedCorretion = 0, isFanActive = 0;
    int temperature, crtTemperatureNo = 0;
    int temperatures[TEMPERATURES_COUNT];
    const int minTemperatures[] = {
        TARGET_TEMPERATURE + TEMPERATURE_HYST_DELTA_UP,
        TARGET_TEMPERATURE - TEMPERATURE_HYST_DELTA_DOWN
    };
    
    // Hardware PWM setup
    pinMode(PWM_PIN, PWM_OUTPUT);
    pwmSetMode(PWM_MODE_MS);
    pwmSetClock(CLOCK);
    pwmSetRange(RANGE);
    pwmWrite(PWM_PIN, 0);

    // Temperature history initialization
    temperature = getTemperature();
    temperatures[0] = temperature;
    temperatures[1] = temperature;
    temperatures[2] = temperature;
    temperatures[3] = temperature;
    fanSpeed = 0;
    sleep(TIME_INTERVAL);

    // Loop
    while (TRUE) {
        // Update current, average temperature and last fan speed
        temperature = getTemperature();
        temperatures[crtTemperatureNo] = temperature;
        ++crtTemperatureNo;
        crtTemperatureNo &= TEMPERATURES_COUNT_MASK;
        lastFanSpeed = fanSpeed;
        
        // The control algorithm starts from a min speed,
        // (may be a non - zero one), has an hysteresis
        // behavior with respect to the temperature at
        // which the fan starts and stops spinning and
        // has two components for speed - one proportional
        // with the current temperature and one additive
        // (a modified integral component) to correct
        // the linear speed if the target temperature cannot
        // be achevied or the speed is too high given only
        // the proportional component is used
        if (temperature > minTemperatures[isFanActive]) {
            isFanActive = TRUE;
            fanSpeed = clamp(SPEED_MAIN_SCALE_FACTOR * (temperature - TARGET_TEMPERATURE), 0, MAX_SPEED);
            temperature = temperatures[0] + temperatures[1] + temperatures[2] + temperatures[3]; 
            temperature >>= TEMPERATURES_COUNT_LOG;
            fanSpeedCorretion += SPEED_CORR_SCALE_FACTOR * (temperature - TARGET_TEMPERATURE);
            fanSpeedCorretion = clamp(fanSpeedCorretion, -fanSpeed, MAX_SPEED - fanSpeed);
            fanSpeed += fanSpeedCorretion;
            fanSpeed = fanSpeed < MIN_SPEED ? MIN_SPEED : fanSpeed;
        } else if (isFanActive) {
            isFanActive = FALSE;
            fanSpeed = 0;
            fanSpeedCorretion = 0;
        }

        // Update fan speed if needed
        if (fanSpeed != lastFanSpeed)
            pwmWrite(PWM_PIN, fanSpeed);
        
        sleep(TIME_INTERVAL);
    }

    return 0;
}
/// @}

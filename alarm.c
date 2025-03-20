#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_timer.h"
#include "nrfx_pwm.h"
#include "nrf_gpio.h"
#include "alarm.h"

#define AUDIO_PIN_LEFT    EDGE_P3
#define AUDIO_PIN_RIGHT   EDGE_P4 

nrfx_pwm_t m_pwm0 = NRFX_PWM_INSTANCE(0);
nrf_pwm_values_common_t seq_values[1] = {0};
nrf_pwm_sequence_t const seq = {
    .values.p_common = seq_values,
    .length          = 1,
    .repeats         = 0,
    .end_delay       = 0
};
#define RADAR_DURATION_MS          3000
#define RADAR_UPDATE_INTERVAL_MS   50
#define RADAR_SWEEP_PERIOD_MS      1000

static uint32_t radar_elapsed_ms = 0;
static const uint16_t f_min = 400;
static const uint16_t f_max = 800;

APP_TIMER_DEF(alarm_radar_timer);
APP_TIMER_DEF(alarm_temp_timer); 
APP_TIMER_DEF(alarm_humid_timer);
APP_TIMER_DEF(alarm_eco2_timer);

void alarm_init(void)
{
    ret_code_t err_code;
    
    err_code = app_timer_create(&alarm_radar_timer,
                                APP_TIMER_MODE_REPEATED,
                                radar_timer_callback);
    if (err_code != NRF_SUCCESS) {
        printf("alarm_radar_timer init failed: 0x%lX\n", err_code);
    }
    
    err_code = app_timer_create(&alarm_temp_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                alarm_temp_timer_cb);
    if (err_code != NRF_SUCCESS) {
        printf("alarm_temp_timer init failed: 0x%lX\n", err_code);
    }
    
    err_code = app_timer_create(&alarm_humid_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                alarm_humid_timer_cb);
    if (err_code != NRF_SUCCESS) {
        printf("alarm_humid_timer init failed: 0x%lX\n", err_code);
    }
    
    err_code = app_timer_create(&alarm_eco2_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                alarm_eco2_timer_cb);
    if (err_code != NRF_SUCCESS) {
        printf("alarm_eco2_timer init failed: 0x%lX\n", err_code);
    }
}

static void play_tone(uint16_t frequency)
{
    nrfx_pwm_stop(&m_pwm0, true);

    if (frequency == 0) {
        seq_values[0] = 0;
    } else {
        uint16_t countertop = (4000000UL / frequency) - 1;
        NRF_PWM0->COUNTERTOP = countertop;
        seq_values[0] = countertop / 2;
    }

    nrfx_pwm_simple_playback(&m_pwm0, &seq, 1, NRFX_PWM_FLAG_LOOP);
}

void radar_timer_callback(void* p_context)
{
    radar_elapsed_ms += RADAR_UPDATE_INTERVAL_MS;
    if (radar_elapsed_ms >= RADAR_DURATION_MS) {
        app_timer_stop(alarm_radar_timer);
        nrfx_pwm_stop(&m_pwm0, true);
        return;
    }
    uint32_t t = radar_elapsed_ms % RADAR_SWEEP_PERIOD_MS;
    float phase = (float)t / (float)RADAR_SWEEP_PERIOD_MS;
    float value;
    if (phase <= 0.5f) {
        value = phase * 2.0f;
    } else {
        value = (1.0f - phase) * 2.0f;
    }

    uint16_t freq = f_min + (uint16_t)(value * (f_max - f_min));
    play_tone(freq);
}

void start_radar_alarm(void)
{
    radar_elapsed_ms = 0;
    ret_code_t err_code = app_timer_start(alarm_radar_timer,
                                          APP_TIMER_TICKS(RADAR_UPDATE_INTERVAL_MS),
                                          NULL);
    if (err_code != NRF_SUCCESS) {
        printf("radar_timer start failed, error: 0x%lX\n", err_code);
    }
}

void start_simple_alarm(uint16_t freq, uint32_t duration_ms)
{
    stop_simple_alarm();
    printf("Start single-frequency buzzer, freq=%d\n", freq);
    uint16_t countertop = (4000000UL / freq) - 1;
    NRF_PWM0->COUNTERTOP = countertop;
    seq_values[0] = countertop / 2;
    nrfx_pwm_simple_playback(&m_pwm0, &seq, 1, NRFX_PWM_FLAG_LOOP);
}

void stop_simple_alarm(void)
{
    nrfx_pwm_stop(&m_pwm0, true);
}

void alarm_temp_timer_cb(void* p_context) {
    stop_simple_alarm();
    printf("Temperature alarm playback ended\n");
}
void alarm_humid_timer_cb(void* p_context) {
    stop_simple_alarm();
    printf("Humidity alarm playback ended\n");
}
void alarm_eco2_timer_cb(void* p_context) {
    stop_simple_alarm();
    printf("eCO2 alarm playback ended\n");
}

void start_temp_alarm(void)
{
    // Play 500Hz, duration 2 seconds
    start_simple_alarm(500, 2000);
    app_timer_start(alarm_temp_timer,
                    APP_TIMER_TICKS(2000),
                    NULL);
}
void start_humid_alarm(void)
{
    // Play 600Hz, duration 2 seconds
    start_simple_alarm(600, 2000);
    app_timer_start(alarm_humid_timer,
                    APP_TIMER_TICKS(2000),
                    NULL);
}
void start_eco2_alarm(void)
{
    // Play 700Hz, duration 2 seconds
    start_simple_alarm(700, 2000);
    app_timer_start(alarm_eco2_timer,
                    APP_TIMER_TICKS(2000),
                    NULL);
}
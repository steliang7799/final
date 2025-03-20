#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_timer.h"
#include "nrfx_pwm.h"
#include "nrf_gpio.h"

void start_radar_alarm(void);
void start_temp_alarm(void);
void start_humid_alarm(void); 
void start_eco2_alarm(void);

void start_simple_alarm(uint16_t freq, uint32_t duration_ms);
void stop_simple_alarm(void);

void radar_timer_callback(void* p_context);
void alarm_temp_timer_cb(void* p_context);
void alarm_humid_timer_cb(void* p_context);
void alarm_eco2_timer_cb(void* p_context);

extern nrfx_pwm_t m_pwm0;

#endif // ALARM_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "nrf_delay.h"
#include "nrfx_spim.h"
#include "microbit_v2.h"
#include "nrf_gpio.h"
#include "nrf_twi_mngr.h"
#include "nrf_drv_twi.h"
#include "app_timer.h"
#include "nrfx_pwm.h"
#include "alarm.h"
#include "sensor.h"
#include "lcd.h"

const nrf_twi_mngr_t* i2c_manager = NULL;
NRF_TWI_MNGR_DEF(twi_mngr_instance, 1, 0);

volatile bool spi_xfer_done = false;
bool alarm_set_flag = false; 
static uint8_t hours = 0, minutes = 0, seconds = 0;
static uint8_t alarm_hours = 0, alarm_minutes = 0;

#define BUTTON_A_PIN 14
#define BUTTON_B_PIN 23

typedef enum {
    STATE_NORMAL,
    STATE_ALARM_SET,
    STATE_TIMEUP,
    STATE_ENVIRONMENT 
} system_state_t;
system_state_t system_state = STATE_NORMAL;


uint32_t current_ms = 0;
uint32_t last_normal_update = 0;
uint32_t last_flash_toggle = 0;
uint32_t last_environment_update = 0; 
uint32_t buttonA_press_start = 0;
uint32_t buttonB_press_start = 0;
bool buttonA_was_pressed = false;
bool buttonB_was_pressed = false;
uint32_t timeup_start = 0;

static void init_time_from_compile(void) {
    int h, m, s;
    if (sscanf(__TIME__, "%d:%d:%d", &h, &m, &s) == 3) {
        hours = (uint8_t)h;
        minutes = (uint8_t)m;
        seconds = (uint8_t)s;
    } else {
        hours = 0; minutes = 0; seconds = 0;
    }
    printf("Initial time: %02d:%02d:%02d\n", hours, minutes, seconds);
}


int main(void) {
    printf("Main: Starting program...\r\n");
    spim_init();
    lcdBegin();
    nrf_gpio_cfg_input(BUTTON_A_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(BUTTON_B_PIN, NRF_GPIO_PIN_PULLUP);
    init_time_from_compile();
    last_normal_update = 0;
    last_flash_toggle = 0;
    current_ms = 0;
    nrf_drv_twi_config_t twi_config = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_config.scl = I2C_QWIIC_SCL;
    twi_config.sda = I2C_QWIIC_SDA;
    twi_config.frequency = NRF_TWIM_FREQ_100K;
    twi_config.interrupt_priority = 0;
    nrf_twi_mngr_init(&twi_mngr_instance, &twi_config);
    i2c_manager = &twi_mngr_instance;
    nrf_gpio_cfg_output(EDGE_P2);
    nrf_gpio_pin_write(EDGE_P2, 0);
    nrfx_pwm_config_t pwm_config = {
        .output_pins = {
            EDGE_P3,
            EDGE_P4,
            NRFX_PWM_PIN_NOT_USED,
            NRFX_PWM_PIN_NOT_USED
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_4MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = 0,
        .load_mode    = NRF_PWM_LOAD_COMMON,
        .step_mode    = NRF_PWM_STEP_AUTO
    };
    ret_code_t err_code = nrfx_pwm_init(&m_pwm0, &pwm_config, NULL);
    app_timer_init();
    alarm_init();
    while (1) {
        nrf_delay_ms(10);
        current_ms += 10;
        if(current_ms - last_normal_update >= 500) {
            last_normal_update = current_ms;
            seconds++;
            if(seconds >= 60) { seconds = 0; minutes++; }
            if(minutes >= 60) { minutes = 0; hours++; }
            if(hours >= 24)   { hours = 0; }
        }
        if(system_state == STATE_NORMAL) {
            {
                char timeStr[9];
                sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
                memset(displayMap, 0x00, sizeof(displayMap));
                uint8_t scale = 2, spacing = 0;
                uint8_t len = strlen(timeStr);
                uint8_t textWidth = len * (5 * scale) + (len - 1) * spacing;
                uint8_t startX = (84 > textWidth) ? (84 - textWidth) / 2 : 0;
                uint8_t textHeight = 8 * scale;
                uint8_t startY = (48 > textHeight) ? (48 - textHeight) / 2 : 0;
                drawStringScaled(timeStr, startX, startY, scale, spacing);
                updateDisplay();
            }
            if(nrf_gpio_pin_read(BUTTON_A_PIN) == 0) {
                if(!buttonA_was_pressed) {
                    buttonA_press_start = current_ms;
                    buttonA_was_pressed = true;
                }
            } else {
                if(buttonA_was_pressed) {
                    uint32_t duration = current_ms - buttonA_press_start;
                    if(duration >= 1000) {
                        system_state = STATE_ENVIRONMENT;
                        printf("Entering Environment Detection Mode\n");
                        last_environment_update = current_ms;
                    }
                    buttonA_was_pressed = false;
                }
            }
            if(nrf_gpio_pin_read(BUTTON_B_PIN) == 0) {
                if(!buttonB_was_pressed) {
                    buttonB_press_start = current_ms;
                    buttonB_was_pressed = true;
                }
            } else {
                if(buttonB_was_pressed) {
                    uint32_t duration = current_ms - buttonB_press_start;
                    if(duration >= 500) {
                        system_state = STATE_ALARM_SET;
                        alarm_hours   = hours;
                        alarm_minutes = minutes;
                        printf("Entering Alarm Set Mode\n");
                    }
                    buttonB_was_pressed = false;
                }
            }
            if(alarm_set_flag &&
               (hours == alarm_hours) &&
               (minutes == alarm_minutes) &&
               (seconds == 0))
            {
                system_state = STATE_TIMEUP;
                timeup_start = current_ms;
                display_time_up_message();
                start_radar_alarm();
            }
        }
        else if(system_state == STATE_ALARM_SET) {
            if(current_ms - last_flash_toggle >= 500) {
                last_flash_toggle = current_ms;
                static bool flash_on = true;
                flash_on = !flash_on;
                if(flash_on) {
                    char alarmStr[6];
                    sprintf(alarmStr, "%02d:%02d", alarm_hours, alarm_minutes);
                    memset(displayMap, 0x00, sizeof(displayMap));
                    uint8_t scale = 2, spacing = 0;
                    uint8_t len = strlen(alarmStr);
                    uint8_t textWidth = len * (5 * scale) + (len - 1) * spacing;
                    uint8_t startX = (84 > textWidth) ? (84 - textWidth) / 2 : 0;
                    uint8_t textHeight = 8 * scale;
                    uint8_t startY = (48 > textHeight) ? (48 - textHeight) / 2 : 0;
                    drawStringScaled(alarmStr, startX, startY, scale, spacing);
                    updateDisplay();
                } else {
                    clear_display();
                }
            }
            if(nrf_gpio_pin_read(BUTTON_A_PIN) == 0) {
                if(!buttonA_was_pressed) {
                    buttonA_press_start = current_ms;
                    buttonA_was_pressed = true;
                }
            } else {
                if(buttonA_was_pressed) {
                    uint32_t duration = current_ms - buttonA_press_start;
                    if(duration < 500) {
                        alarm_minutes++;
                        if(alarm_minutes >= 60) {
                            alarm_minutes = 0;
                            alarm_hours++;
                            if(alarm_hours >= 24) alarm_hours = 0;
                        }
                    }
                    buttonA_was_pressed = false;
                }
            }
            if(nrf_gpio_pin_read(BUTTON_B_PIN) == 0) {
                if(!buttonB_was_pressed) {
                    buttonB_press_start = current_ms;
                    buttonB_was_pressed = true;
                }
            } else {
                if(buttonB_was_pressed) {
                    uint32_t duration = current_ms - buttonB_press_start;
                    if(duration < 500) {
                        if(alarm_minutes == 0) {
                            alarm_minutes = 59;
                            if(alarm_hours == 0) alarm_hours = 23;
                            else alarm_hours--;
                        } else {
                            alarm_minutes--;
                        }
                    } else {
                        system_state = STATE_NORMAL;
                        alarm_set_flag = true;
                        printf("Alarm set to %02d:%02d\n", alarm_hours, alarm_minutes);
                    }
                    buttonB_was_pressed = false;
                }
            }
        }
        else if(system_state == STATE_TIMEUP) {
            if(current_ms - timeup_start >= 1500) {
                if(system_state == STATE_TIMEUP) {
                    system_state = STATE_NORMAL;
                }
            }
        }
        else if(system_state == STATE_ENVIRONMENT) {
            if(current_ms - last_environment_update >= 500) {
                last_environment_update = current_ms;
                update_environment_display();
            }
            if(nrf_gpio_pin_read(BUTTON_A_PIN) == 0) {
                if(!buttonA_was_pressed) {
                    buttonA_press_start = current_ms;
                    buttonA_was_pressed = true;
                }
            } else {
                if(buttonA_was_pressed) {
                    uint32_t duration = current_ms - buttonA_press_start;
                    if(duration >= 500) {
                        system_state = STATE_NORMAL;
                        printf("Exiting Environment Detection Mode\n");
                    }
                    buttonA_was_pressed = false;
                }
            }
        }
    }
    
    return 0;
}

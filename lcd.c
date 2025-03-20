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

extern volatile bool spi_xfer_done;

#define WHITE 0
#define BLACK 1

#define LCD_RST_PIN EDGE_P0
#define LCD_SCE_PIN EDGE_P1
#define LCD_DC_PIN  EDGE_P8

#define LCD_COMMAND 0
#define LCD_DATA 1

#define LCD_WIDTH 84
#define LCD_HEIGHT 48

uint8_t displayMap[LCD_WIDTH * LCD_HEIGHT / 8]; 

const nrfx_spim_t SPIM_INST = NRFX_SPIM_INSTANCE(2);

static const uint8_t font_digits[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
};

static const uint8_t font_upper[26][5] = {
    {0x7C,0x12,0x11,0x12,0x7C}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}  // Z
};

static const uint8_t font_lower[26][5] = {
    {0x20,0x54,0x54,0x54,0x78}, // a
    {0x7F,0x48,0x44,0x44,0x38}, // b
    {0x38,0x44,0x44,0x44,0x20}, // c
    {0x38,0x44,0x44,0x48,0x7F}, // d
    {0x38,0x54,0x54,0x54,0x18}, // e
    {0x08,0x7E,0x09,0x01,0x02}, // f
    {0x0C,0x52,0x52,0x52,0x3E}, // g
    {0x7F,0x08,0x04,0x04,0x78}, // h
    {0x00,0x44,0x7D,0x40,0x00}, // i
    {0x20,0x40,0x44,0x3D,0x00}, // j
    {0x7F,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7F,0x40,0x00}, // l
    {0x7C,0x04,0x18,0x04,0x78}, // m
    {0x7C,0x08,0x04,0x04,0x78}, // n
    {0x38,0x44,0x44,0x44,0x38}, // o
    {0x7C,0x14,0x14,0x14,0x08}, // p
    {0x08,0x14,0x14,0x18,0x7C}, // q
    {0x7C,0x08,0x04,0x04,0x08}, // r
    {0x48,0x54,0x54,0x54,0x20}, // s
    {0x04,0x3F,0x44,0x40,0x20}, // t
    {0x3C,0x40,0x40,0x20,0x7C}, // u
    {0x1C,0x20,0x40,0x20,0x1C}, // v
    {0x3C,0x40,0x30,0x40,0x3C}, // w
    {0x44,0x28,0x10,0x28,0x44}, // x
    {0x0C,0x50,0x50,0x50,0x3C}, // y
    {0x44,0x64,0x54,0x4C,0x44}  // z
};

static const uint8_t font_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t font_dot[5]   = {0x00, 0x00, 0x80, 0x00, 0x00};


void spim_event_handler(nrfx_spim_evt_t const * p_event, void * p_context) {
    if (p_event->type == NRFX_SPIM_EVENT_DONE)
        spi_xfer_done = true;
}

void spim_init(void) {
    printf("SPI Init: Starting initialization.\r\n");
    nrfx_spim_config_t spim_config = NRFX_SPIM_DEFAULT_CONFIG;
    spim_config.sck_pin = EDGE_P13;
    spim_config.mosi_pin = EDGE_P15;
    spim_config.miso_pin = EDGE_P14;
    spim_config.irq_priority = 0;
    spim_config.frequency = NRF_SPIM_FREQ_4M;
    spim_config.mode = NRF_SPIM_MODE_0;

    nrfx_spim_init(&SPIM_INST, &spim_config, spim_event_handler, NULL);
    printf("SPI Init: Initialization complete.\r\n");
}

void LCDWrite(uint8_t data_or_command, uint8_t data) {
    if (data_or_command == LCD_DATA)
        nrf_gpio_pin_set(LCD_DC_PIN);
    else
        nrf_gpio_pin_clear(LCD_DC_PIN);

    nrf_gpio_pin_clear(LCD_SCE_PIN);

    nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(&data, 1);
    spi_xfer_done = false;
    nrfx_spim_xfer(&SPIM_INST, &xfer, 0);
    while (!spi_xfer_done) {
        nrf_delay_us(10);
    }
    nrf_gpio_pin_set(LCD_SCE_PIN);
}

void gotoXY(uint8_t x, uint8_t y) {
    LCDWrite(LCD_COMMAND, 0x80 | x);
    LCDWrite(LCD_COMMAND, 0x40 | y);
}

void updateDisplay(void) {
    gotoXY(0, 0);
    for (uint16_t i = 0; i < (LCD_WIDTH * LCD_HEIGHT / 8); i++) {
        LCDWrite(LCD_DATA, displayMap[i]);
    }
}

void setPixel(uint8_t x, uint8_t y, uint8_t color) {
    if(x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    uint16_t byteIndex = x + (y / 8) * LCD_WIDTH;
    uint8_t bit_mask = 1 << (y % 8);
    if(color == BLACK)
        displayMap[byteIndex] |= bit_mask;
    else
        displayMap[byteIndex] &= ~bit_mask;
}

void drawCharScaled(char c, uint8_t x, uint8_t y, uint8_t scale) {
    const uint8_t *bitmap = NULL;
    if(c >= '0' && c <= '9') {
        bitmap = font_digits[c - '0'];
    } else if(c == ':') {
        bitmap = font_digits[10];
    } else if(c == '.') {
        bitmap = font_dot;
    } else if(c >= 'A' && c <= 'Z') {
        bitmap = font_upper[c - 'A'];
    } else if(c >= 'a' && c <= 'z') {
        bitmap = font_lower[c - 'a'];
    } else if(c == '!') {
        static const uint8_t font_exclam[5] = {
            0x00,
            0x5F,
            0x00,
            0x00,
            0x00
        };
        bitmap = font_exclam;
    } else if(c == ' ') {
        bitmap = font_space;
    } else {
        return;
    }

    for(uint8_t i = 0; i < 5; i++) {
        uint8_t line = bitmap[i];
        for(uint8_t row = 0; row < 8; row++) {
            if(line & (1 << row)) {
                for(uint8_t dy = 0; dy < scale; dy++) {
                    for(uint8_t dx = 0; dx < scale; dx++) {
                        setPixel(x + i * scale + dx, y + row * scale + dy, BLACK);
                    }
                }
            }
        }
    }
}

void lcdBegin(void) {
    printf("LCD Init: Configuring control pins...\r\n");
    nrf_gpio_cfg_output(LCD_RST_PIN);
    nrf_gpio_cfg_output(LCD_SCE_PIN);
    nrf_gpio_cfg_output(LCD_DC_PIN);
    
    printf("LCD Init: Setting initial state...\r\n");
    nrf_gpio_pin_set(LCD_RST_PIN);
    nrf_gpio_pin_set(LCD_DC_PIN);
    nrf_gpio_pin_set(LCD_SCE_PIN);
    
    printf("LCD Init: Starting reset sequence...\r\n");
    nrf_delay_ms(10);
    nrf_gpio_pin_clear(LCD_RST_PIN);
    nrf_delay_ms(100);
    nrf_gpio_pin_set(LCD_RST_PIN);
    nrf_delay_ms(10);
    
    printf("LCD Init: Sending initialization commands...\r\n");
    LCDWrite(LCD_COMMAND, 0x21);
    LCDWrite(LCD_COMMAND, 0xBF);
    LCDWrite(LCD_COMMAND, 0x04);
    LCDWrite(LCD_COMMAND, 0x14);
    LCDWrite(LCD_COMMAND, 0x20);
    LCDWrite(LCD_COMMAND, 0x0C);
    printf("LCD Init: Initialization complete.\r\n");
}

void drawStringScaled(const char *str, uint8_t x, uint8_t y, uint8_t scale, uint8_t spacing) {
    while(*str) {
        drawCharScaled(*str, x, y, scale);
        x += (5 * scale) + spacing;
        str++;
    }
}

void clear_display(void) {
    memset(displayMap, 0x00, sizeof(displayMap));
    updateDisplay();
}

static uint8_t hours = 0, minutes = 0, seconds = 0;
static uint8_t alarm_hours = 0, alarm_minutes = 0;
extern bool alarm_set_flag;

void update_time_display(void) {
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
    memset(displayMap, 0x00, sizeof(displayMap));
    uint8_t scale = 2, spacing = 0;
    uint8_t len = strlen(timeStr);
    uint8_t textWidth = len * (5 * scale) + (len - 1) * spacing;
    uint8_t startX = (LCD_WIDTH > textWidth) ? (LCD_WIDTH - textWidth) / 2 : 0;
    uint8_t textHeight = 8 * scale;
    uint8_t startY = (LCD_HEIGHT > textHeight) ? (LCD_HEIGHT - textHeight) / 2 : 0;
    drawStringScaled(timeStr, startX, startY, scale, spacing);
    updateDisplay();
}

void update_alarm_display(void) {
    char alarmStr[6];
    sprintf(alarmStr, "%02d:%02d", alarm_hours, alarm_minutes);
    memset(displayMap, 0x00, sizeof(displayMap));
    uint8_t scale = 2, spacing = 0;
    uint8_t len = strlen(alarmStr);
    uint8_t textWidth = len * (5 * scale) + (len - 1) * spacing;
    uint8_t startX = (LCD_WIDTH > textWidth) ? (LCD_WIDTH - textWidth) / 2 : 0;
    uint8_t textHeight = 8 * scale;
    uint8_t startY = (LCD_HEIGHT > textHeight) ? (LCD_HEIGHT - textHeight) / 2 : 0;
    drawStringScaled(alarmStr, startX, startY, scale, spacing);
    updateDisplay();
}

void display_time_up_message(void) {
    char msg[] = "WAKE!";
    memset(displayMap, 0x00, sizeof(displayMap));
    uint8_t scale = 2, spacing = 1;
    uint8_t len = strlen(msg);
    uint8_t textWidth = len * (5 * scale) + (len - 1) * spacing;
    uint8_t startX = (LCD_WIDTH > textWidth) ? (LCD_WIDTH - textWidth) / 2 : 0;
    uint8_t textHeight = 8 * scale;
    uint8_t startY = (LCD_HEIGHT > textHeight) ? (LCD_HEIGHT - textHeight) / 2 : 0;
    drawStringScaled(msg, startX, startY, scale, spacing);
    updateDisplay();
}

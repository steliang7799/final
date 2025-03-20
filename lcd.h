#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void spim_init(void);
void lcdBegin(void);

void LCDWrite(uint8_t data_or_command, uint8_t data);
void gotoXY(uint8_t x, uint8_t y);
void updateDisplay(void);

void setPixel(uint8_t x, uint8_t y, uint8_t color);
void drawCharScaled(char c, uint8_t x, uint8_t y, uint8_t scale);
void drawStringScaled(const char *str, uint8_t x, uint8_t y, uint8_t scale, uint8_t spacing);

void clear_display(void);

void update_time_display(void);
void update_alarm_display(void);
void display_time_up_message(void);

#define LCD_WIDTH 84
#define LCD_HEIGHT 48
extern uint8_t displayMap[LCD_WIDTH * LCD_HEIGHT / 8];

#endif

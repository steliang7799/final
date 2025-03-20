#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t eco2;
    uint16_t tvoc;
} sgp30_data_t;

typedef struct {
    float temperature;
    float humidity;
} sht45_data_t;

bool sgp30_init(void);
sgp30_data_t sgp30_read_data(void);

sht45_data_t sht45_read_data(void);

void update_environment_display(void);

#endif

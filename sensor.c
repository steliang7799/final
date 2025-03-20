#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nrf_delay.h"
#include "nrf_twi_mngr.h"
#include "sensor.h"

extern const nrf_twi_mngr_t* i2c_manager;

#define SGP30_ADDR                   0x58
#define INIT_AIR_QUALITY_MSB         0x20
#define INIT_AIR_QUALITY_LSB         0x03
#define MEASURE_AIR_QUALITY_MSB      0x20
#define MEASURE_AIR_QUALITY_LSB      0x08

#define SHT45_ADDR   0x44
#define MEASURE_CMD  0xFD

static uint8_t sgp30_crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

bool sgp30_init(void) {
    ret_code_t err_code;
    uint8_t init_cmd[2] = { INIT_AIR_QUALITY_MSB, INIT_AIR_QUALITY_LSB };
    nrf_twi_mngr_transfer_t init_xfer = NRF_TWI_MNGR_WRITE(SGP30_ADDR, init_cmd, 2, 0);
    err_code = nrf_twi_mngr_perform(i2c_manager, NULL, &init_xfer, 1, NULL);
    if (err_code != NRF_SUCCESS) {
        printf("SGP30 initialization failed! Error: 0x%lX\n", err_code);
        return false;
    }
    nrf_delay_ms(1000);
    return true;
}

sgp30_data_t sgp30_read_data(void) {
    sgp30_data_t result = {0, 0};
    ret_code_t err_code;
    uint8_t cmd[2] = { MEASURE_AIR_QUALITY_MSB, MEASURE_AIR_QUALITY_LSB };
    uint8_t data[6] = {0};

    nrf_twi_mngr_transfer_t write_xfer = NRF_TWI_MNGR_WRITE(SGP30_ADDR, cmd, 2, 0);
    err_code = nrf_twi_mngr_perform(i2c_manager, NULL, &write_xfer, 1, NULL);
    if (err_code != NRF_SUCCESS) {
        printf("SGP30 I2C write failed! Error: 0x%lX\n", err_code);
        return result;
    }

    nrf_delay_ms(15);

    nrf_twi_mngr_transfer_t read_xfer = NRF_TWI_MNGR_READ(SGP30_ADDR, data, 6, 0);
    err_code = nrf_twi_mngr_perform(i2c_manager, NULL, &read_xfer, 1, NULL);
    if (err_code != NRF_SUCCESS) {
        printf("SGP30 I2C read failed! Error: 0x%lX\n", err_code);
        return result;
    }

    uint8_t crc1 = sgp30_crc8(data, 2);
    if (crc1 != data[2]) {
        printf("eCO2 CRC error: expected %02X, got %02X\n", crc1, data[2]);
        return result;
    }
    uint8_t crc2 = sgp30_crc8(data + 3, 2);
    if (crc2 != data[5]) {
        printf("TVOC CRC error: expected %02X, got %02X\n", crc2, data[5]);
        return result;
    }

    result.eco2 = ((uint16_t)data[0] << 8) | data[1];
    result.tvoc = ((uint16_t)data[3] << 8) | data[4];

    return result;
}

sht45_data_t sht45_read_data(void) {
    sht45_data_t result = {0.0f, 0.0f};
    ret_code_t err_code;
    uint8_t cmd = MEASURE_CMD;
    uint8_t data[6] = {0};

    nrf_twi_mngr_transfer_t write_xfer = NRF_TWI_MNGR_WRITE(SHT45_ADDR, &cmd, 1, NRF_TWI_MNGR_NO_STOP);
    err_code = nrf_twi_mngr_perform(i2c_manager, NULL, &write_xfer, 1, NULL);
    if (err_code != NRF_SUCCESS) {
        printf("SHT45 I2C write failed! Error: 0x%lX\n", err_code);
        return result;
    }

    nrf_delay_ms(30);

    nrf_twi_mngr_transfer_t read_xfer = NRF_TWI_MNGR_READ(SHT45_ADDR, data, 6, 0);
    err_code = nrf_twi_mngr_perform(i2c_manager, NULL, &read_xfer, 1, NULL);
    if (err_code != NRF_SUCCESS) {
        printf("SHT45 I2C read failed! Error: 0x%lX\n", err_code);
        return result;
    }

    uint16_t temp_ticks = ((uint16_t)data[0] << 8) | data[1];
    result.temperature = -45.0f + (175.0f * (float)temp_ticks / 65535.0f);

    uint16_t hum_ticks = ((uint16_t)data[3] << 8) | data[4];
    result.humidity = -6.0f + (125.0f * (float)hum_ticks / 65535.0f);

    return result;
}

#include "lcd.h"
#include "alarm.h"

void update_environment_display(void) {
    sgp30_data_t air = sgp30_read_data();
    sht45_data_t tempData = sht45_read_data();
    printf("Temp=%.2fC, Humid=%.2f%%, eCO2=%u, TVOC=%u\n",
           tempData.temperature, tempData.humidity, air.eco2, air.tvoc);
    char line1[64], line2[64], line3[64], line4[64];
    sprintf(line1, "Temp: %.2f C", tempData.temperature);
    sprintf(line2, "Humid: %.2f %%RH", tempData.humidity);
    sprintf(line3, "eCO2: %u ppm", air.eco2);
    sprintf(line4, "TVOC: %u ppb", air.tvoc);

    memset(displayMap, 0x00, sizeof(displayMap));
    drawStringScaled(line1, 0, 0, 1, 1);
    drawStringScaled(line2, 0, 8, 1, 1);
    drawStringScaled(line3, 0, 16, 1, 1);
    drawStringScaled(line4, 0, 24, 1, 1);
    updateDisplay();

    if (tempData.temperature > 30.0f) {
        start_temp_alarm();
    }
    if (tempData.humidity > 70.0f) {
        start_humid_alarm();
    }
    if (air.eco2 > 800) {
        start_eco2_alarm();
    }
}

#ifndef BH1750_I2C_H
#define BH1750_I2C_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// bh1750 I2C address, isto utilizando o I2C 0 da BitDogLab
#define I2C0_SDA_PIN 2
#define I2C0_SCL_PIN 3

#define BH1750_ADDR 0x23
#define BH1750_POWER_ON 0x01
#define BH1750_CONT_HIGH_RES_MODE 0x10

// Function prototypes
i2c_inst_t* bh1750_init(i2c_inst_t *i2c_instance);
void bh1750_set_mode(i2c_inst_t *i2c, uint8_t mode);
void bh1750_read_lux(i2c_inst_t *i2c, float *lux);

#endif // BH1750_I2C_H
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Definições de pinos do I2C e velocidade
#define SSD1306_I2C_SDA 14
#define SSD1306_I2C_SCL 15
#define SSD1306_I2C_CLOCK 400  // Velocidade do I2C em kHz

// Protótipos de funções
void display_init(void);
void display_status_msg(const char* linha1, const char* linha2, const char* linha3);


#endif // DISPLAY_H

#include "display.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include <string.h>
#include <stdio.h>

// Área de renderização do display
static struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1
};

// Inicialização do I2C e do display SSD1306
void display_init(void) {
    // Inicializa o I2C
    i2c_init(i2c1, SSD1306_I2C_CLOCK * 1000);
    gpio_set_function(SSD1306_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_I2C_SDA);
    gpio_pull_up(SSD1306_I2C_SCL);

    // Inicializa o display SSD1306
    ssd1306_init();

    // Define o tamanho do buffer de renderização
    calculate_render_area_buffer_length(&frame_area);
}

void display_status_msg(const char* linha1, const char* linha2, const char* linha3) {
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    // Linha 1 na parte superior (Y = 0)
    ssd1306_draw_string(ssd, 0, 0, linha1);

    // Linha 2 (Y = 10)
    ssd1306_draw_string(ssd, 0, 10, linha2);

    // Linha 3 (Y = 20)
    ssd1306_draw_string(ssd, 0, 20, linha3);

    render_on_display(ssd, &frame_area);
}


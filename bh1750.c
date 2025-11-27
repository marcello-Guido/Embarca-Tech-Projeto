#include "bh1750.h"

// Configuração do I2C 0 da placa e do sensor
i2c_inst_t* bh1750_init(i2c_inst_t *i2c_instance) {
    printf("[BH1750] Configurando BH1750 no I2C...\n");

    // 1. Inicializa o barramento I2C com a instância e velocidade desejada
    i2c_init(i2c_instance, 400 * 1000);
    
    // 2. Define os pinos GPIO para as funções I2C
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    
    // 3. Ativa as resistências de pull-up internas
    gpio_pull_up(I2C0_SDA_PIN);
    gpio_pull_up(I2C0_SCL_PIN);
    
    // 4. Configura o sensor BH1750
    bh1750_set_mode(i2c_instance, BH1750_POWER_ON);
    sleep_ms(10); // Pequeno delay para estabilização
    bh1750_set_mode(i2c_instance, BH1750_CONT_HIGH_RES_MODE);

    // BH1750_CONT_HIGH_RES_MODE -> É um modo de leitura com uma resolução de 0.1 lx, dessa forma o 
    // valor lido pode variar entre 0 e 65535, basicamente 
    // O BH1750 continua a medir a luz continuamente com precisão de 1 lux e 
    // mantém os resultados atualizados para quando solicitar
    
    // 5. Aguarda o tempo da primeira conversão
    sleep_ms(180);

    printf("[BH1750] BH1750 configurado e pronto para leitura.\n");

    // Retorna o ponteiro para a instância I2C para uso futuro na main
    return i2c_instance;
}

// A função para configuração flexível, aceitando a instância I2C como parâmetro
void bh1750_set_mode(i2c_inst_t *i2c, uint8_t mode) {
    i2c_write_blocking(i2c, BH1750_ADDR, &mode, 1, false);

    // BH1750_ADDR -> Endereço do sensor BH1750 no barramento I2C
}

// Leitura do sensor BH1750 
void bh1750_read_lux(i2c_inst_t *i2c, float *lux) {
    uint8_t buffer[2];

    // Lê 2 bytes (MSB e LSB) do sensor através do barramento I2C.
    i2c_read_blocking(i2c, BH1750_ADDR, buffer, 2, false);
    
    // Combina os dois bytes (MSB << 8 | LSB) em um único valor inteiro de 16 bits.
    uint16_t raw = (buffer[0] << 8) | buffer[1];
    
    // Converte o valor bruto para lux, conforme a fórmula do datasheet para o modo de alta resolução.
    // Fórmula: Lux = (Valor Medido) / 1.2
    *lux = raw / 1.2f;
}
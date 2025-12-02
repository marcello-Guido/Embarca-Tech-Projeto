#include <stdio.h>
#include "pico/stdlib.h"
#include <stdlib.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Wifi
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include <string.h>

// Sensores
#include "hardware/i2c.h"
#include "aht10.h"
#include "bh1750.h"
#include "vl53l0x.h"

#define BUTTON1_PIN 5 // botão 1
#define BUTTON2_PIN 6 // botão 2
#define LED_PIN 12    // sensor

char button1[50] = "Nenhum evento";
char button2[50] = "Nenhum evento";
char http_response[1024];

float temperatura = 0.0f;
float humidade = 0.0f;
float luminosidade = 0.0f;
float distancia = 0.0f;

// ---------- WIFI TASK ----------
#define WIFI_SSID "MAURO GUIDO"
#define WIFI_PASS "1975mmpg"

#define I2C_PORT i2c0
#define SDA_PIN 0
#define SCL_PIN 1

// Prototipos das funções I2C
int i2c_write(uint8_t addr, const uint8_t *data, uint16_t len);
int i2c_read(uint8_t addr, uint8_t *data, uint16_t len);
void delay_ms(uint32_t ms);

volatile bool wifi_conectado = false; // outras tasks podem ler isso

// Resposta http
void create_http_response()
{
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "  <meta charset=\"UTF-8\">"
             "  <title>Controle do LED e Botões</title>"
             "  <script>"
             "    setInterval(() => {"
             "      window.location.reload();"
             "    }, 1000);" // Atualiza a página a cada 1000ms = 1 segundo
             "  </script>"
             "</head>"
             "<body>"
             "  <h1>Controle do Sensor e Status dos Botões</h1>"

             "  <p><a href=\"/led/on\">Ligar LED</a></p>"
             "  <p><a href=\"/led/off\">Desligar LED</a></p>"

             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"

             "  <h2>Sensores:</h2>"
             "  <p>Temperatura: %.2f °C</p>"
             "  <p>Umidade: %.2f %%</p>"
             "  <p>Luminosidade: %.0f lux</p>"
             "  <p>Distância: %.0f mm</p>"

             "</body>"
             "</html>\r\n",
             button1, button2, temperatura, humidade, luminosidade, distancia);
}

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL)
    {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    if (strstr(request, "GET /led/on"))
    {
        gpio_put(LED_PIN, 1); // Liga o LED
    }
    else if (strstr(request, "GET /led/off"))
    {
        gpio_put(LED_PIN, 0); // Desliga o LED
    }

    // Atualiza o conteúdo da página com base no estado dos botões
    create_http_response();

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_callback); // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);                // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback); // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

// Status dos botões
void buttons()
{
    static bool button1_last_state = false;
    static bool button2_last_state = false;
    bool button1_state = !gpio_get(BUTTON1_PIN); // Botão pressionado = LOW
    bool button2_state = !gpio_get(BUTTON2_PIN);
    if (button1_state != button1_last_state)
    {
        button1_last_state = button1_state;
        if (button1_state)
        {
            snprintf(button1, sizeof(button1), "Botão 1 foi pressionado!");
            printf("Botão 1 pressionado\n");
        }
        else
        {
            snprintf(button1, sizeof(button1), "Botão 1 foi solto!");
            printf("Botão 1 solto\n");
        }
    }
    if (button2_state != button2_last_state)
    {
        button2_last_state = button2_state;
        if (button2_state)
        {
            snprintf(button2, sizeof(button2), "Botão 2 foi pressionado!");
            printf("Botão 2 pressionado\n");
        }
        else
        {
            snprintf(button2, sizeof(button2), "Botão 2 foi solto!");
            printf("Botão 2 solto\n");
        }
    }
}

// Fila global
QueueHandle_t filaSensores;

// -------- TASK 1: Sensor (Temperatura e umidade) --------
void taskTempUmidade(void *pvParameters)
{
    AHT10_Handle aht10 = {
        .iface = {
            .i2c_write = i2c_write,
            .i2c_read = i2c_read,
            .delay_ms = delay_ms}};

    // Inicializa o sensor AHT10
    printf("Inicializando AHT10...\n");
    if (!AHT10_Init(&aht10))
    {
        printf("Falha na inicialização do sensor!\n");
        while (1)
             vTaskDelay(pdMS_TO_TICKS(4000));
    }

    while (1)
    {
        float temp, hum;
        if (AHT10_ReadTemperatureHumidity(&aht10, &temp, &hum))
        {
            temperatura = temp;
            humidade = hum;
        }
        else
        {
            printf("Falha na leitura dos dados!\n");
        }

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}
// -------- TASK 2: Sensor (Luminosidade) --------
void taskLuminosidade(void *pvParameters)
{
    bh1750_init(I2C_PORT);

    while (1)
    {

        float lux = bh1750_read_lux(I2C_PORT);
        luminosidade = lux;

        vTaskDelay(pdMS_TO_TICKS(4000)); // 2s
    }
}
// -------- TASK 3: Sensor (Distancia) --------
void taskDistancia(void *pvParameters)
{
    if (!vl53l0x_init(I2C_PORT))
    {
        printf("Falha ao inicializar o VL53L0X.\n");
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }
    }
    while (1)
    {
        int dis = vl53l0x_read_distance_mm(I2C_PORT);
        if (dis < 0)
        {
            printf("Erro na leitura da distância.\n");
        }
        else
        {
           distancia = dis;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 500ms
    }
}
// -------- TASK 4: Wifi --------
void taskWifi(void *pvParameters)
{
    printf("[WiFi] Inicializando módulo CYW43...\n");

    if (cyw43_arch_init())
    {
        printf("[WiFi] ERRO: falha ao inicializar CYW43!\n");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    cyw43_arch_enable_sta_mode();

    while (1)
    {
        printf("[WiFi] Conectando a %s...\n", WIFI_SSID);

        int result = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASS,
            CYW43_AUTH_WPA2_AES_PSK,
            10000 // timeout 10s
        );

        if (result == 0)
        {
            wifi_conectado = true;
            printf("[WiFi] Conectado com sucesso! 🎉\n");
            uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
            printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
            // Mantém a task viva e monitorando
            while (wifi_conectado)
            {
                cyw43_arch_poll(); // mantém o WiFi funcionando
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
        else
        {
            wifi_conectado = false;
            printf("[WiFi] Falha ao conectar (%d). Tentando novamente...\n", result);
            vTaskDelay(pdMS_TO_TICKS(5000)); // espera 5s antes de tentar de novo
        }
    }
}
// -------- TASK 5: Http --------
void taskHttp(void *pvParameters)
{
    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    start_http_server();
    absolute_time_t lp = get_absolute_time();

    while (1)
    {
        buttons();
        sleep_ms(200); // Reduz o uso da CPU
    }
}

int main()
{
    stdio_init_all();

    i2c_init(I2C_PORT, 100 * 1000); // 100 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    sleep_ms(200); // Para dar tempo da USB conectar

    sleep_ms(100); // Aguarda estabilização do I2C

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    printf("Botões configurados com pull-up nos pinos %d e %d\n", BUTTON1_PIN, BUTTON2_PIN);
    // Cria fila
    filaSensores = xQueueCreate(5, sizeof(int));

    if (filaSensores == NULL)
    {
        printf("Erro ao criar fila!\n");
        while (1)
            ;
    }

    // Cria tasks
    xTaskCreate(taskTempUmidade, "TempUmid", 1024, NULL, 1, NULL);
    xTaskCreate(taskLuminosidade, "Luminosidade", 1024, NULL, 1, NULL);
    xTaskCreate(taskDistancia, "Distancia", 1024, NULL, 1, NULL);
    xTaskCreate(taskWifi, "WiFi", 2048, NULL, 2, NULL);
    xTaskCreate(taskHttp, "Http", 2048, NULL, 2, NULL);

    // Inicia o scheduler
    vTaskStartScheduler();

    while (1)
    {
    }
}
// Função para escrita I2C
int i2c_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    int result = i2c_write_blocking(I2C_PORT, addr, data, len, false);
    return result < 0 ? -1 : 0;
}

// Função para leitura I2C
int i2c_read(uint8_t addr, uint8_t *data, uint16_t len)
{
    int result = i2c_read_blocking(I2C_PORT, addr, data, len, false);
    return result < 0 ? -1 : 0;
}

// Função para delay
void delay_ms(uint32_t ms)
{
    sleep_ms(ms);
}

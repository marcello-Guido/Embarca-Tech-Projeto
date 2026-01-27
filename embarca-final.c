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

#define BUTTON1_PIN 5  // botão 1
#define BUTTON2_PIN 6  // botão 2
#define BUZZER_PIN 15 // buzzer — acionado ao apertar qualquer botão (não acende LED azul)
// LED RGB: R=13, G=14, B=12 (azul = controle web e temp < 20 °C)
#define LED_PIN 12           // LED azul do RGB — controle manual (web) e temp < 20 °C
#define LED_RGB_RED_PIN 13   // canal vermelho — temperatura >= 26 °C
#define LED_RGB_GREEN_PIN 14 // canal verde — amarelo = R+G (20 a 26 °C)

char button1[50] = "Nenhum evento";
char button2[50] = "Nenhum evento";
char http_response[4096];

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

// Resposta http com HTML + CSS integrado
void create_http_response()
{
    char temp_alert[320];
    if (temperatura >= 26.0f)
    {
        snprintf(temp_alert, sizeof(temp_alert),
                 "<div class=\"alert\"><strong>Temperatura alta!</strong> Ative o ar condicionado a 20°C para resfriar o ambiente.</div>");
    }
    else
    {
        temp_alert[0] = '\0';
    }

    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html><html lang=\"pt-BR\">"
             "<head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
             "<title>Embarca - Controle e Sensores</title>"
             "<style>"
             "*{box-sizing:border-box;margin:0;padding:0}"
             "body{font-family:'Segoe UI',system-ui,sans-serif;background:linear-gradient(135deg,#1a1a2e 0%%,#16213e 100%%);min-height:100vh;color:#eee;padding:1.5rem}"
             "h1{text-align:center;margin-bottom:1.5rem;font-size:1.5rem;color:#00d9ff}"
             ".alert{background:rgba(220,38,38,.2);border:1px solid #dc2626;border-radius:8px;padding:.75rem 1rem;margin-bottom:1rem;color:#fecaca;font-size:.9rem}"
             ".card{background:rgba(255,255,255,.08);border-radius:12px;padding:1rem 1.25rem;margin-bottom:1rem;border:1px solid rgba(255,255,255,.1)}"
             ".card h2{font-size:.9rem;margin-bottom:.75rem;color:#7dd3fc;font-weight:600}"
             ".card p{font-size:.85rem;margin:.35rem 0}"
             ".btn-wrap{display:flex;gap:.75rem;flex-wrap:wrap;margin-top:.5rem}"
             "a{display:inline-block;padding:.5rem 1rem;border-radius:8px;text-decoration:none;font-weight:600;font-size:.85rem;transition:transform .15s,box-shadow .15s}"
             "a:active{transform:scale(.97)}"
             "a.led-on{background:#22c55e;color:#fff}"
             "a.led-on:hover{box-shadow:0 0 12px rgba(34,197,94,.5)}"
             "a.led-off{background:#ef4444;color:#fff}"
             "a.led-off:hover{box-shadow:0 0 12px rgba(239,68,68,.5)}"
             ".valor{color:#a5f3fc;font-weight:600}"
             ".footer{text-align:center;margin-top:1.5rem;font-size:.75rem;color:#64748b}"
             "</style>"
             "<script>setInterval(()=>location.reload(),1000);</script>"
             "</head><body>"
             "<h1>Embarca - Controle e Sensores</h1>"
             "%s"
             "<div class=\"card\"><h2>Controle do LED</h2>"
             "<div class=\"btn-wrap\">"
             "<a href=\"/led/on\" class=\"led-on\">Ligar LED</a>"
             "<a href=\"/led/off\" class=\"led-off\">Desligar LED</a>"
             "</div></div>"
             "<div class=\"card\"><h2>Estado dos Botões</h2>"
             "<p>Botão 1: <span class=\"valor\">%s</span></p>"
             "<p>Botão 2: <span class=\"valor\">%s</span></p></div>"
             "<div class=\"card\"><h2>Sensores</h2>"
             "<p>Temperatura: <span class=\"valor\">%.2f °C</span></p>"
             "<p>Umidade: <span class=\"valor\">%.2f %%</span></p>"
             "<p>Luminosidade: <span class=\"valor\">%.0f lux</span></p>"
             "<p>Distância: <span class=\"valor\">%.0f mm</span></p></div>"
             "<p class=\"footer\">Atualização automática a cada 1s • Pico W + FreeRTOS</p>"
             "</body></html>\r\n",
             temp_alert, button1, button2, temperatura, humidade, luminosidade, distancia);
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
    // Ao apertar qualquer botão: buzzer ligado; ao soltar todos: buzzer desligado (não acende LED azul)
    gpio_put(BUZZER_PIN, button1_state || button2_state);
}

// Atualiza LED RGB por temperatura: >=26 vermelho, <20 azul (pino 12), 20–26 amarelo (R+G)
void atualiza_leds_temperatura(void)
{
    if (temperatura >= 26.0f)
    {
        gpio_put(LED_RGB_RED_PIN, 1);
        gpio_put(LED_RGB_GREEN_PIN, 0);
        gpio_put(LED_PIN, 0); // azul
    }
    else if (temperatura < 20.0f)
    {
        gpio_put(LED_RGB_RED_PIN, 0);
        gpio_put(LED_RGB_GREEN_PIN, 0);
        gpio_put(LED_PIN, 1); // azul (mesmo pino do controle web)
    }
    else
    {
        gpio_put(LED_RGB_RED_PIN, 1);
        gpio_put(LED_RGB_GREEN_PIN, 1);
        gpio_put(LED_PIN, 0); // amarelo = vermelho + verde
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
            atualiza_leds_temperatura();
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
    gpio_init(LED_RGB_RED_PIN);
    gpio_set_dir(LED_RGB_RED_PIN, GPIO_OUT);
    gpio_init(LED_RGB_GREEN_PIN);
    gpio_set_dir(LED_RGB_GREEN_PIN, GPIO_OUT);

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0);

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

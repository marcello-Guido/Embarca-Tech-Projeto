#include <stdio.h>
#include "pico/stdlib.h"
#include <stdlib.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Wifi
#include "pico/cyw43_arch.h"

// ---------- WIFI TASK ----------
#define WIFI_SSID "MAURO GUIDO"
#define WIFI_PASS "1975mmpg"

volatile bool wifi_conectado = false;  // outras tasks podem ler isso

void taskWifi(void *pvParameters) {
    printf("[WiFi] Inicializando módulo CYW43...\n");

    if (cyw43_arch_init()) {
        printf("[WiFi] ERRO: falha ao inicializar CYW43!\n");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    cyw43_arch_enable_sta_mode();

    while (1) {
        printf("[WiFi] Conectando a %s...\n", WIFI_SSID);

        int result = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASS,
            CYW43_AUTH_WPA2_AES_PSK,
            10000   // timeout 10s
        );

        if (result == 0) {
            wifi_conectado = true;
            printf("[WiFi] Conectado com sucesso! 🎉\n");

            // Mantém a task viva e monitorando
            while (wifi_conectado) {
                cyw43_arch_poll();  // mantém o WiFi funcionando
                vTaskDelay(pdMS_TO_TICKS(2000));
            }

        } else {
            wifi_conectado = false;
            printf("[WiFi] Falha ao conectar (%d). Tentando novamente...\n", result);
            vTaskDelay(pdMS_TO_TICKS(5000)); // espera 5s antes de tentar de novo
        }
    }
}

// Fila global
QueueHandle_t filaSensores;

// -------- TASK 1: Sensor (simulado) --------
void taskSensor(void *pvParameters) {
    while (1) {
        int valor = rand() % 100;  // Simula leitura
        xQueueSend(filaSensores, &valor, 0);
        printf("[Sensor] Valor gerado: %d\n", valor);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1s
    }
}
// -------- TASK 2: Processamento --------
void taskProcessamento(void *pvParameters) {
    int recebido;

    while (1) {
        if (xQueueReceive(filaSensores, &recebido, portMAX_DELAY)) {
            int processado = recebido * 2; // Simulação
            printf("[Processamento] Recebido: %d | Resultado: %d\n",
                   recebido, processado);
        }
    }
}

// -------- TASK 3: Logger --------
void taskLogger(void *pvParameters) {
    while (1) {
        printf("[Logger] Sistema rodando...\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2s
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Para dar tempo da USB conectar

    // Cria fila
    filaSensores = xQueueCreate(5, sizeof(int));

    if (filaSensores == NULL) {
        printf("Erro ao criar fila!\n");
        while (1);
    }

    // Cria tasks
    xTaskCreate(taskSensor, "Sensor", 1024, NULL, 1, NULL);
    xTaskCreate(taskProcessamento, "Processamento", 1024, NULL, 1, NULL);
    xTaskCreate(taskLogger, "Logger", 1024, NULL, 1, NULL);
    xTaskCreate(taskWifi, "WiFi", 2048, NULL, 2, NULL);

    // Inicia o scheduler
    vTaskStartScheduler();

    while (1) {}
}

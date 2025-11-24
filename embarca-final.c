#include <stdio.h>
#include "pico/stdlib.h"
#include <stdlib.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

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

    // Inicia o scheduler
    vTaskStartScheduler();

    while (1) {}
}

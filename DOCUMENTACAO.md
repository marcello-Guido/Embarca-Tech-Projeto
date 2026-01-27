# Documentação do Projeto Embarca-Final

## Índice
1. [Visão Geral](#visão-geral)
2. [Desenvolvimento do Sistema](#desenvolvimento-do-sistema)
3. [Versões e Compatibilidades](#versões-e-compatibilidades)
4. [Tecnologias Utilizadas](#tecnologias-utilizadas)
5. [Principais Lógicas e Arquitetura](#principais-lógicas-e-arquitetura)
6. [Hardware e Pinagem](#hardware-e-pinagem)
7. [Estrutura do Projeto](#estrutura-do-projeto)

---

## Visão Geral

O **Embarca-Final** é um sistema embarcado baseado em **Raspberry Pi Pico W** que integra múltiplos sensores (temperatura/umidade, luminosidade e distância), interface com botões e LED, conectividade Wi-Fi e um servidor HTTP para monitoramento e controle remoto via navegador. O projeto utiliza **FreeRTOS** para execução concorrente das funções em tarefas independentes.

**Funcionalidades principais:**
- Leitura contínua de sensores (AHT10, BH1750, VL53L0X)
- Controle de LED e leitura de dois botões
- Conexão Wi-Fi e servidor HTTP na porta 80
- Interface web com atualização automática exibindo estado dos sensores e botões, além de links para ligar/desligar o LED

---

## Desenvolvimento do Sistema

### Arquitetura

O sistema foi desenvolvido seguindo uma arquitetura **multitarefa (multitasking)** com o FreeRTOS:

- **Separação de responsabilidades:** cada recurso (sensores, Wi-Fi, HTTP, GPIO) é tratado por uma ou mais tarefas dedicadas.
- **Comunicação via variáveis globais:** os sensores atualizam variáveis compartilhadas (`temperatura`, `humidade`, `luminosidade`, `distancia`, `button1`, `button2`) que são lidas pela task HTTP para montar a página web.
- **Prioridades:** tarefas de sensores (prioridade 1) e tarefas de rede/Wi-Fi/HTTP (prioridade 2) garantem que a conexão e o servidor tenham tempo de CPU adequado.
- **Polling e callbacks:** o Wi-Fi usa `cyw43_arch_poll()`; o servidor HTTP usa callbacks do lwIP (`tcp_recv`, `tcp_accept`) para tratar requisições.

### Fluxo de Desenvolvimento

1. **Inicialização (main):** configuração de I2C, GPIO (LED e botões), criação da fila FreeRTOS (reservada para uso futuro) e criação das 5 tarefas.
2. **Sensores:** cada task de sensor inicializa seu dispositivo via I2C e entra em loop com `vTaskDelay` para definir o intervalo de leitura (1–4 s).
3. **Wi-Fi:** a task `taskWifi` inicializa o CYW43, tenta conectar à rede (WPA2) e, após sucesso, mantém o link ativo com `cyw43_arch_poll()`.
4. **HTTP:** após delay inicial de 10 s, a task `taskHttp` sobe o servidor TCP na porta 80 e, em loop, chama `buttons()` para atualizar o estado dos botões e servir a página gerada dinamicamente.

---

## Versões e Compatibilidades

### Versões de Software

| Componente        | Versão / Referência |
|-------------------|---------------------|
| **Raspberry Pi Pico SDK** | 1.5.1 |
| **Toolchain (ARM GCC)**   | 13_2_Rel1 |
| **Picotool**              | 2.0.0 |
| **FreeRTOS Kernel**       | V202107.00 |
| **CMake**                 | ≥ 3.13 |
| **Padrão C**              | C11 |
| **Padrão C++**            | C++17 |

### Plataforma e Board

- **Board:** `pico_w` (Raspberry Pi Pico W com chip Wi-Fi CYW43439).
- **Microcontrolador:** RP2040 (dual-core ARM Cortex-M0+, 133 MHz).
- **Compatibilidade:** o código assume Pico W; em placas sem Wi-Fi (Pico padrão) seria necessário remover/adaptar `taskWifi`, `taskHttp` e dependências do `pico_cyw43_arch_lwip_threadsafe_background`.

### Bibliotecas e Dependências

- **FreeRTOS-Kernel** e **FreeRTOS-Kernel-Heap4** (heap 128 KB).
- **pico_cyw43_arch_lwip_threadsafe_background:** pilha Wi-Fi + lwIP em modo thread-safe/background.
- **hardware_i2c:** driver I2C do SDK para RP2040.
- **pico_stdlib:** utilitários e inicialização padrão (USB UART desativado, stdio via USB).

---

## Tecnologias Utilizadas

### Sistema Operacional em Tempo Real

- **FreeRTOS:** kernel com preempção, mutexes, semáforos, filas e timers. Configuração em `FreeRTOSConfig.h` (heap 128 KB, 32 níveis de prioridade, tick 1 kHz, suporte a dual-core RP2040).

### Conectividade e Rede

- **CYW43 (cyw43_arch):** driver do chip Wi-Fi do Pico W (CYW43439).
- **lwIP:** pilha TCP/IP (IPv4, TCP, UDP, DHCP, DNS). Opções em `lwipopts.h` (modo `NO_SYS`, raw API, buffers e pooling configurados para ambiente embarcado).

### Comunicação e Periféricos

- **I2C (hardware/i2c):** barramento único em 100 kHz, shared pelos três sensores:
  - **AHT10** — temperatura e umidade (endereço 0x38).
  - **BH1750** — luminosidade em lux (endereço 0x23).
  - **VL53L0X** — distância em mm por ToF (endereço 0x29).

### Sensores

| Sensor  | Função           | Interface | Endereço I2C |
|---------|------------------|-----------|--------------|
| **AHT10**   | Temperatura e umidade | I2C (abstração por callbacks) | 0x38 |
| **BH1750**  | Luminosidade (lux)    | I2C (`i2c_inst_t`)            | 0x23 |
| **VL53L0X** | Distância (mm) ToF    | I2C (`i2c_inst_t`)            | 0x29 |

### Aplicação e Serviços

- **Servidor HTTP:** implementado sobre **lwIP raw API** (TCP PCBs, `tcp_listen`, `tcp_accept`, `tcp_recv`). Responde com HTML estático/dinâmico e trata rotas `/led/on` e `/led/off` para controlar o LED.
- **GPIO:** leitura de dois botões (com pull-up) e controle de um LED.

---

## Principais Lógicas e Arquitetura

### Tarefas FreeRTOS

| Tarefa        | Nome          | Stack | Prioridade | Função principal |
|---------------|---------------|-------|------------|-------------------|
| `taskTempUmidade`  | TempUmid      | 1024  | 1 | Inicializa AHT10, lê temperatura/umidade a cada 4 s e atualiza variáveis globais. |
| `taskLuminosidade` | Luminosidade  | 1024  | 1 | Inicializa BH1750, lê lux a cada 4 s. |
| `taskDistancia`    | Distancia     | 1024  | 1 | Inicializa VL53L0X, lê distância em mm a cada 1 s. |
| `taskWifi`         | WiFi          | 2048  | 2 | Inicializa CYW43, conecta ao Wi-Fi (WPA2), mantém link com `cyw43_arch_poll()`. |
| `taskHttp`         | Http          | 2048  | 2 | Após 10 s, inicia servidor TCP:80, em loop chama `buttons()` e atende requisições. |

### Lógica do Servidor HTTP

1. **Listen:** `tcp_bind(pcb, IP_ADDR_ANY, 80)` e `tcp_listen(pcb)`.
2. **Accept:** em nova conexão, `connection_callback` registra `http_callback` em `tcp_recv(newpcb, http_callback)`.
3. **Request/Response:** em `http_callback`, o payload é inspecionado por `strstr(request, "GET /led/on")` e `strstr(request, "GET /led/off")` para acionar o LED; em seguida chama `create_http_response()` e envia o HTML via `tcp_write`.
4. **Conteúdo da página:** HTML com refresh automático (1 s), estado dos dois botões e valores atuais de temperatura, umidade, luminosidade e distância, além dos links para ligar/desligar o LED.

### Lógica dos Botões

- Função `buttons()` (chamada no loop da task Http):
  - Lê GPIO dos botões com `gpio_get()` (lógica invertida: botão pressionado = LOW).
  - Detecção de borda com `button1_last_state` e `button2_last_state`.
  - Atualiza as strings `button1` e `button2` (“Nenhum evento”, “Botão X foi pressionado!”, “Botão X foi solto!”), usadas no HTML.

### Variáveis Globais Compartilhadas

- `temperatura`, `humidade`, `luminosidade`, `distancia`: escritas pelas tasks de sensores, lidas em `create_http_response()`.
- `button1`, `button2`: escritas em `buttons()`, lidas em `create_http_response()`.
- `wifi_conectado`: escrita em `taskWifi`, disponível para possível uso por outras partes do código.
- `filaSensores`: fila FreeRTOS criada (capacidade 5, tamanho do elemento `sizeof(int)`); atualmente não é utilizada nas tasks — fica disponível para futura troca de dados entre tarefas.

### Abstração I2C do AHT10

O AHT10 usa uma interface genérica (`AHT10_Interface`) com ponteiros para `i2c_write`, `i2c_read` e `delay_ms`. No projeto, essas funções são implementadas em `embarca-final.c` usando `i2c_write_blocking`/`i2c_read_blocking` do SDK e `sleep_ms`, permitindo portar o driver para outros ambientes sem depender diretamente do hardware do Pico.

---

## Hardware e Pinagem

| Recurso    | Pino GPIO | Observação |
|------------|-----------|------------|
| **I2C SDA** | 0        | `GPIO_FUNC_I2C`, pull-up |
| **I2C SCL** | 1        | `GPIO_FUNC_I2C`, pull-up |
| **Botão 1** | 5        | Entrada, pull-up; pressionado = LOW |
| **Botão 2** | 6        | Entrada, pull-up; pressionado = LOW |
| **LED**     | 12       | Saída; 1 = ligado, 0 = desligado |

Os três sensores compartilham o mesmo barramento I2C (`i2c0`) em **100 kHz**.

---

## Estrutura do Projeto

```
embarca-final/
├── embarca-final.c      # Código principal: main, tasks, HTTP, botões, wrappers I2C
├── CMakeLists.txt       # Build: Pico W, FreeRTOS, libs, fontes e includes
├── FreeRTOSConfig.h     # Configuração do kernel FreeRTOS
├── lwipopts.h           # Opções da pilha lwIP
├── pico_sdk_import.cmake
├── inc/                 # Inclusões adicionais (ex.: SSD1306; não usadas no build atual)
├── lib/
│   ├── AHT10/           # Driver AHT10 (temperatura/umidade)
│   ├── BH1750/          # Driver BH1750 (luminosidade)
│   └── VL53L0X/         # Driver VL53L0X (distância)
└── .vscode/             # Configuração VS Code / Cursor (build, debug, kits)
```

### Build e Execução

- **Configuração:** CMake com kit/toolchain do Raspberry Pi Pico (ARM GCC 13.2, SDK 1.5.1).
- **Saída padrão:** stdio via USB (`pico_enable_stdio_usb(embarca-final 1)`), UART desativado.
- **Executável:** `embarca-final.uf2` (ou binário conforme configurado), gravável no Pico W via bootloader.

---

## Resumo para Finalização do Projeto

O **Embarca-Final** conclui a integração de:

1. **FreeRTOS** para multitarefa e escalonamento.
2. **Sensores I2C** (AHT10, BH1750, VL53L0X) com leituras periódicas e exposição na interface web.
3. **Wi-Fi** (CYW43 + lwIP) para conexão à rede e serviço HTTP.
4. **Servidor HTTP** em raw TCP/lwIP com páginas dinâmicas e controle do LED.
5. **GPIO** para dois botões e um LED, com estados refletidos na mesma interface.

As versões de SDK, FreeRTOS e ferramentas estão documentadas no CMake e em `FreeRTOSConfig.h`; a compatibilidade é garantida para **Raspberry Pi Pico W** com o SDK e o ambiente de build indicados.

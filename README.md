# Controlador de Nível de Líquido

Este projeto implementa um sistema embarcado para controle e monitoramento de nível de líquido em um reservatório, utilizando um **Raspberry Pi Pico W**, sensores analógicos, atuadores (bomba d´água e relé) e uma interface web integrada.

## Funcionalidades

- Leitura do nível de líquido por meio de um potenciômetro acoplado a uma boia.
- Controle automático de uma bomba de água com base em limites configuráveis.
- Interface web para exibição e ajuste dos limites mínimo e máximo.
- Feedback visual via matriz de LEDs WS2812 (NeoPixel).
- Display OLED com informações em tempo real.
- Alerta sonoro via buzzer em caso de nível fora dos limites.
- Botão para reset dos limites e entrada em modo BOOTSEL.

## Componentes Utilizados

- Raspberry Pi Pico W
- Potenciômetro (como sensor de nível)
- Display OLED SSD1306 (I2C)
- Matriz de LEDs WS2812 (5x5)
- Relé 5V para bomba de água
- Buzzer piezoelétrico (PWM)
- Botões físicos (GPIO)
- Interface web embarcada

## Estrutura do Projeto

```
main.c                // Código principal do sistema
lib/
├── ssd1306.h/.c      // Driver para o display OLED
├── font.h            // Fonte usada no display
├── webserver.h/.c    // Servidor web embarcado
ws2812.pio.h/.pio     // Driver PIO para WS2812
```

## Instalação e Execução

### Pré-requisitos

- [SDK do Raspberry Pi Pico](https://github.com/raspberrypi/pico-sdk)
- [CMake](https://cmake.org/)
- Compilador GCC para ARM

### Compilação

```bash
mkdir build
cd build
cmake ..
make
```

### Upload

Use o botão BOOTSEL no Raspberry Pi Pico W para colocá-lo no modo de armazenamento USB e arraste o `.uf2` gerado.

## Interface Web

O dispositivo cria uma rede Wi-Fi ou se conecta a uma existente, disponibilizando uma interface web onde é possível:

- Visualizar o nível atual do reservatório.
- Ajustar os limites mínimo e máximo de nível.
- Observar o estado da bomba.

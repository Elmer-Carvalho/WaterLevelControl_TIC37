Monitoramento de Iluminação com ADC no Raspberry Pi Pico W (BitDogLab)

Este projeto demonstra o uso do conversor analógico-digital (ADC) do Raspberry Pi Pico W na placa BitDogLab para monitorar níveis de iluminação ambiente em tempo real. A leitura é exibida graficamente em um display OLED SSD1306 via I2C.

Funcionalidades:
   Leitura da intensidade de luz através do ADC no pino GPIO 28.
   Conversão do valor analógico em porcentagem de luminosidade.
   Exibição das informações no display OLED (SSD1306), com interface gráfica básica.
   Suporte ao modo BOOTSEL via botão físico (GPIO 6) para reinício USB.

Obs. O Botão físico B "GPIO 6" é utilizado para testes quando do desenvolvimento do programa. Não é essencial para o funcionamento
desta aplicação. Leva o RP2 para o modo BOOTSEL.


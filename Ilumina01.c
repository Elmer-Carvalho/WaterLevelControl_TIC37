/*
 * Por: Wilton Lacerda Silva
 *    
 * Monitoramento de Iluminação com ADC no Raspberry Pi Pico W (BitDogLab)
 *
 *  Este projeto demonstra o uso do conversor analógico-digital (ADC) do Raspberry 
 *  Pi Pico W na placa BitDogLab para monitorar níveis de iluminação ambiente em 
 *  tempo real. A leitura é exibida graficamente em um display OLED SSD1306 via I2C.
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  // GPIO compartilhada com o microfone na BitDogLab.


//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

int main()
{
  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  //Aqui termina o trecho para modo BOOTSEL com botão B

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_t ssd; // Inicializa a estrutura do display
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();
  adc_gpio_init(ADC_PIN); // GPIO 28 como entrada analógica


  uint16_t adc_value_x;
  float ldr_pcento;
  char str_x[5];  // Buffer para armazenar a string
  char str_y[10]; // Buffer para armazenar a string
  
  bool cor = true;
  while (true)
  {
    adc_select_input(2); // O pino 28 como entrada analógica
    adc_value_x = adc_read(); // Faz leitura do LRD
    ldr_pcento = (adc_value_x * 100) / 4095; // Converte em porcentagem
   
    sprintf(str_x, "%d", adc_value_x);  // Converte o inteiro em string
    sprintf(str_y, "%1.0f", 100-ldr_pcento);  // Converte o float em string
    
    //cor = !cor;
    // Atualiza o conteúdo do display com animações
    ssd1306_fill(&ssd, !cor); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
    ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
    ssd1306_line(&ssd, 3, 37, 123, 37, cor); // Desenha uma linha   
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
    ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16); // Desenha uma string
    ssd1306_draw_string(&ssd, "  Iluminacao", 10, 28); // Desenha uma string 
    ssd1306_draw_string(&ssd, "ADC", 13, 41); // Desenha uma string    
    ssd1306_draw_string(&ssd, "Nivel LUZ", 50, 41); // Desenha uma string    
    ssd1306_line(&ssd, 44, 37, 44, 60, cor); // Desenha uma linha vertical         
    ssd1306_draw_string(&ssd, str_x, 8, 52); // Desenha uma string     
    ssd1306_draw_string(&ssd, str_y, 75, 52); // Desenha uma string   
    ssd1306_send_data(&ssd); // Atualiza o display
    sleep_ms(700);
  }
}
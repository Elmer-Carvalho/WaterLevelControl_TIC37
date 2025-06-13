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
#define RELAY_PIN 8

#define LIM_MIN 30.0
#define LIM_MAX 70.0


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

  // I2C Initialisation
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
  gpio_pull_up(I2C_SDA); 
  gpio_pull_up(I2C_SCL); 

  // Inicializa o display
  ssd1306_t ssd; 
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); 
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  // Incializa o ADC
  adc_init();
  adc_gpio_init(ADC_PIN); 


  // Inicializa o pino do relé
  gpio_init(RELAY_PIN);
  gpio_set_dir(RELAY_PIN, GPIO_OUT); // Define o pino como saída
  gpio_put(RELAY_PIN, 1); 

  uint16_t adc_value_x;
  float nivel_percentual;
  char buffer_adc[10];  // Buffer para armazenar a string
  char buffer_nivel[10]; // Buffer para armazenar a string
  bool bomba_ligada = false;
  
  bool cor = true;
  while (true)
  {
    adc_select_input(2); 
    adc_value_x = adc_read(); 
    nivel_percentual = (adc_value_x * 100) / 4095; // Converte em porcentagem

     ssd1306_fill(&ssd, !cor); 
    if (nivel_percentual < LIM_MIN && !bomba_ligada){
      gpio_put(RELAY_PIN, 0); // Liga o relé
      bomba_ligada = true;
    } else if(nivel_percentual > LIM_MAX && bomba_ligada){
      gpio_put(RELAY_PIN, 1); 
      bomba_ligada = false;
    }
    
    sprintf(buffer_adc, "%d", adc_value_x);  // Converte o inteiro em string
    sprintf(buffer_nivel, "%1.0f", nivel_percentual);  // Converte o float em string
    // Atualiza o conteúdo do display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
    ssd1306_draw_string(&ssd, "Nivel de Agua:", 8, 6); 
    ssd1306_draw_string(&ssd, buffer_nivel, 8, 22); 
    ssd1306_draw_string(&ssd, "ADC:", 8, 41); 
    ssd1306_draw_string(&ssd, buffer_adc, 40, 41);  
    ssd1306_draw_string(&ssd, bomba_ligada ? "Bomba: ON" : "Bomba: OFF", 8, 52);  
    ssd1306_send_data(&ssd); 
    sleep_ms(700);
  }
}
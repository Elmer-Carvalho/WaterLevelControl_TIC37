#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "pico/cyw43_arch.h" 

#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/webserver.h" 

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  
#define RELAY_PIN 8
#define botaoB 6

volatile float lim_min = 30.0;
volatile float lim_max = 70.0;
volatile float nivel_percentual = 0;
volatile bool bomba_ligada = false;

// Função de IRQ para o botão de BOOTSEL
void gpio_irq_handler(uint gpio, uint32_t events) {
  reset_usb_boot(0, 0);
}

int main() {
  stdio_init_all();

  // Configuração do botão B para BOOTSEL
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

  // Inicialização do I2C
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
  gpio_pull_up(I2C_SDA); 
  gpio_pull_up(I2C_SCL); 

  // Inicializa o display 
  ssd1306_t ssd; 
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); 
  ssd1306_config(&ssd);
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  // Inicializa o ADC e o Relé
  adc_init();
  adc_gpio_init(ADC_PIN); 
  gpio_init(RELAY_PIN);
  gpio_set_dir(RELAY_PIN, GPIO_OUT);
  gpio_put(RELAY_PIN, 1); 

  // Conexão Wi-Fi e servidor 
  ssd1306_draw_string(&ssd, "Conectando WiFi", 6, 22);
  ssd1306_send_data(&ssd);

  if (!webserver_init()) {
      printf("Falha ao iniciar o servidor web.\n");
      ssd1306_fill(&ssd, false);
      ssd1306_draw_string(&ssd, "WiFi: FALHA", 8, 22);
      ssd1306_send_data(&ssd);
      while(true); 
  }
  uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
  char ip_str[24];
  snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  printf("IP: %s\n", ip_str);

  ssd1306_fill(&ssd, false);
  ssd1306_draw_string(&ssd, "IP:", 8, 6);
  ssd1306_draw_string(&ssd, ip_str, 8, 22);
  ssd1306_send_data(&ssd);
  sleep_ms(3000); // Mostra o IP por 3 segundos

  uint16_t adc_value_x;
  char buffer_adc[10];
  char buffer_nivel[10];
  
  while (true) {
    cyw43_arch_poll();
  
    adc_select_input(2); 
    adc_value_x = adc_read(); 
    nivel_percentual = (adc_value_x * 100) / 4095;

    if (nivel_percentual < lim_min && !bomba_ligada) {
      gpio_put(RELAY_PIN, 0); 
      bomba_ligada = true;
    } else if (nivel_percentual > lim_max && bomba_ligada) {
      gpio_put(RELAY_PIN, 1); 
      bomba_ligada = false;
    }
    
    sprintf(buffer_nivel, "%1.0f%%", nivel_percentual);
    sprintf(buffer_adc, "ADC: %d", adc_value_x);
    
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Nivel de Agua:", 8, 6); 
    ssd1306_draw_string(&ssd, buffer_nivel, 8, 22); 
    ssd1306_draw_string(&ssd, buffer_adc, 8, 41);
    ssd1306_draw_string(&ssd, bomba_ligada ? "Bomba: LIGADA" : "Bomba: DESLIGADA", 8, 52);  
    ssd1306_send_data(&ssd);

    sleep_ms(500); 
  }
}
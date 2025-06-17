#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "ws2812.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  // GPIO compartilhada com o microfone na BitDogLab.
#define RELAY_PIN 8
#define BUZZER 21
#define BUTTON_A 5
#define BUTTON_B 6
#define MATRIX_PIN 7
#define NUM_LEDS 25

#define LIM_MIN_PADRAO 30.0
#define LIM_MAX_PADRAO 70.0

volatile float lim_min = LIM_MIN_PADRAO;
volatile float lim_max = LIM_MAX_PADRAO;

volatile bool resetar_limites = false;
volatile uint32_t ultimo_tempo_A = 0;
const uint32_t debounce = 200;


void irq_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_B) {
        reset_usb_boot(0, 0);
    }
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if (gpio == BUTTON_A && (tempo_atual - ultimo_tempo_A > debounce)) {       
            resetar_limites = true;
            ultimo_tempo_A = tempo_atual;
    }
}


void ws2812_put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

void atualiza_matriz(float nivel_percentual) {
    uint32_t frame[NUM_LEDS] = {0};  // buffer com 25 LEDs apagados

    if (nivel_percentual < 20.0) {
        // Todos desligados
        // Nada a fazer
    }
    else if (nivel_percentual >= 20.0 && nivel_percentual <= 30.0) {
        for (int i = 0; i <= 4; i++) {
            frame[i] = urgb_u32(255, 0, 0);
        }
    }
    else if (nivel_percentual >= 31.0 && nivel_percentual <= 39.0) {
        for (int i = 0; i <= 4; i++) {
            frame[i] = urgb_u32(0, 0, 255);
        }
    }
    else if (nivel_percentual >= 40.0 && nivel_percentual <= 59.0) {
        for (int i = 0; i <= 9; i++) {
            frame[i] = urgb_u32(0, 0, 255);
        }
    }
    else if (nivel_percentual >= 60.0 && nivel_percentual <= 70.0) {
        for (int i = 0; i <= 14; i++) {
            frame[i] = urgb_u32(0, 0, 255);
        }
    }
    else if (nivel_percentual >= 71.0 && nivel_percentual <= 79.0) {
        for (int i = 0; i <= 14; i++) {
            frame[i] = urgb_u32(255, 0, 0);
        }
    }
    else if (nivel_percentual >= 80.0 && nivel_percentual <= 99.0) {
        for (int i = 0; i <= 19; i++) {
            frame[i] = urgb_u32(255, 0, 0);
        }
    }
    else if (nivel_percentual >= 100.0) {
        for (int i = 0; i <= 24; i++) {
            frame[i] = urgb_u32(255, 0, 0);
        }
    }

    // Envia o frame completo para o WS2812:
    for (int i = 0; i < NUM_LEDS; i++) {
        ws2812_put_pixel(frame[i]);
    }
    sleep_us(70);
}

void alerta_buzzer(float nivel_percentual) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    static int contador = 0;
    contador++;

    if (nivel_percentual < lim_min || nivel_percentual > lim_max) {
        if (contador % 4 < 2) {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, 1250);
        } else {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
        }
    } else {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    }
}

int main()
{
  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(BUTTON_B);
  gpio_set_dir(BUTTON_B, GPIO_IN);
  gpio_pull_up(BUTTON_B);
  gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &irq_callback);

  gpio_init(BUTTON_A);
  gpio_set_dir(BUTTON_A, GPIO_IN);
  gpio_pull_up(BUTTON_A);
  gpio_set_irq_enabled(BUTTON_A, GPIO_IRQ_EDGE_FALL, true);
 

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

  // Incializa o Buzzer
  gpio_init(BUZZER);
  gpio_set_dir(BUZZER, GPIO_OUT);

  gpio_set_function(BUZZER, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(BUZZER);
  pwm_set_clkdiv(slice_num, 100.0f);
  pwm_set_wrap(slice_num, 2500);  //125MHz / 100 / 2500 = 500Hz
  pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
  pwm_set_enabled(slice_num, true);

  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm, offset, MATRIX_PIN, 800000, false);

  uint16_t adc_value_x;
  float nivel_percentual;
  char buffer_adc[10];  // Buffer para armazenar a string
  char buffer_nivel[10]; // Buffer para armazenar a string
  bool bomba_ligada = false;
  
  bool cor = true;
  while (true)
  {
    if (resetar_limites) {
      lim_min = LIM_MIN_PADRAO;
      lim_max = LIM_MAX_PADRAO;
      resetar_limites = false;
    }

    adc_select_input(2); 
    adc_value_x = adc_read(); 
    nivel_percentual = (adc_value_x * 100) / 4095; // Converte em porcentagem

    ssd1306_fill(&ssd, !cor); 
    if (nivel_percentual < lim_min && !bomba_ligada){
      gpio_put(RELAY_PIN, 0); // Liga o relé
      bomba_ligada = true;
    } else if(nivel_percentual > lim_max && bomba_ligada){
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
    sleep_ms(500);

    atualiza_matriz(nivel_percentual);

    alerta_buzzer(nivel_percentual);
  }
}
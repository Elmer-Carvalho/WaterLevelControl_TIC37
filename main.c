#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "ws2812.pio.h"
#include "lib/webserver.h" 

// ===== DEFINIÇÕES DE HARDWARE =====
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  
#define RELAY_PIN 8
#define BUZZER 21
#define BUTTON_A 5
#define BUTTON_B 6
#define MATRIX_PIN 7
#define NUM_LEDS 25

// ===== CONSTANTES DO SISTEMA =====
#define LIM_MIN_PADRAO 30.0
#define LIM_MAX_PADRAO 70.0
#define DEBOUNCE_TIME 200

// ===== VARIÁVEIS GLOBAIS =====
volatile float lim_min = LIM_MIN_PADRAO;
volatile float lim_max = LIM_MAX_PADRAO;
volatile float nivel_percentual = 0;
volatile bool bomba_ligada = false;
volatile bool resetar_limites = false;
volatile uint32_t ultimo_tempo_A = 0;

// ===== PROTÓTIPOS DE FUNÇÕES =====
void irq_callback(uint gpio, uint32_t events);
void inicializar_hardware(void);
void inicializar_display(ssd1306_t *ssd);
void inicializar_webserver(ssd1306_t *ssd);
void ws2812_put_pixel(uint32_t pixel_grb);
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void atualiza_matriz(float nivel_percentual);
void alerta_buzzer(float nivel_percentual);
void controla_bomba(float nivel);
void atualiza_display(ssd1306_t *ssd, uint16_t adc_value);

// ===== IMPLEMENTAÇÃO DAS FUNÇÕES =====

/**
 * Callback para interrupções dos botões
 */
void irq_callback(uint gpio, uint32_t events) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    
    if (gpio == BUTTON_B) {
        reset_usb_boot(0, 0);  // Reset para modo BOOTSEL
        return;
    }
    
    if (gpio == BUTTON_A && (tempo_atual - ultimo_tempo_A > DEBOUNCE_TIME)) {       
        resetar_limites = true;
        ultimo_tempo_A = tempo_atual;
    }
}

/**
 * Inicializa todos os periféricos de hardware
 */
void inicializar_hardware(void) {
    stdio_init_all();
    
    // Configuração dos botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &irq_callback);
    
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);

    // Configuração do I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
    gpio_pull_up(I2C_SDA); 
    gpio_pull_up(I2C_SCL); 

    // Configuração do ADC
    adc_init();
    adc_gpio_init(ADC_PIN); 

    // Configuração do relé
    gpio_init(RELAY_PIN);
    gpio_set_dir(RELAY_PIN, GPIO_OUT);
    gpio_put(RELAY_PIN, 1); // Inicialmente desligado

    // Configuração do buzzer (PWM)
    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_clkdiv(slice_num, 100.0f);
    pwm_set_wrap(slice_num, 2500);  // 125MHz / 100 / 2500 = 500Hz
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    pwm_set_enabled(slice_num, true);

    // Configuração da matriz de LEDs WS2812
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, MATRIX_PIN, 800000, false);
}

/**
 * Inicializa e configura o display OLED
 */
void inicializar_display(ssd1306_t *ssd) {
    ssd1306_init(ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); 
    ssd1306_config(ssd);
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
}

/**
 * Inicializa o servidor web e mostra informações no display
 */
void inicializar_webserver(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);
    ssd1306_draw_string(ssd, "Conectando WiFi", 6, 22);
    ssd1306_send_data(ssd);

    if (!webserver_init()) {
        printf("Falha ao iniciar o servidor web.\n");
        ssd1306_fill(ssd, false);
        ssd1306_draw_string(ssd, "WiFi: FALHA", 8, 22);
        ssd1306_send_data(ssd);
        while(true); // Para execução em caso de falha
    }
    
    // Obtém e exibe o IP
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str);

    ssd1306_fill(ssd, false);
    ssd1306_draw_string(ssd, "IP:", 8, 6);
    ssd1306_draw_string(ssd, ip_str, 8, 22);
    ssd1306_send_data(ssd);
    sleep_ms(3000); // Mostra o IP por 3 segundos
}

/**
 * Envia um pixel para a matriz WS2812
 */
void ws2812_put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
 * Converte valores RGB para formato u32 (GRB)
 */
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

/**
 * Atualiza a matriz de LEDs baseada no nível percentual
 */
void atualiza_matriz(float nivel_percentual) {
    uint32_t frame[NUM_LEDS] = {0};  // Buffer com 25 LEDs apagados
    uint32_t cor_azul = urgb_u32(0, 0, 255);
    uint32_t cor_vermelha = urgb_u32(255, 0, 0);

    // Define quais LEDs acender baseado no nível
    if (nivel_percentual >= 20.0 && nivel_percentual <= 30.0) {
        // Primeira linha (0-4) - vermelha (nível baixo)
        for (int i = 0; i <= 4; i++) {
            frame[i] = cor_vermelha;
        }
    }
    else if (nivel_percentual >= 31.0 && nivel_percentual <= 39.0) {
        // Primeira linha - azul (nível OK)
        for (int i = 0; i <= 4; i++) {
            frame[i] = cor_azul;
        }
    }
    else if (nivel_percentual >= 40.0 && nivel_percentual <= 59.0) {
        // Duas primeiras linhas - azul
        for (int i = 0; i <= 9; i++) {
            frame[i] = cor_azul;
        }
    }
    else if (nivel_percentual >= 60.0 && nivel_percentual <= 70.0) {
        // Três primeiras linhas - azul
        for (int i = 0; i <= 14; i++) {
            frame[i] = cor_azul;
        }
    }
    else if (nivel_percentual >= 71.0 && nivel_percentual <= 79.0) {
        // Três primeiras linhas - vermelha (nível alto)
        for (int i = 0; i <= 14; i++) {
            frame[i] = cor_vermelha;
        }
    }
    else if (nivel_percentual >= 80.0 && nivel_percentual <= 99.0) {
        // Quatro primeiras linhas - vermelha
        for (int i = 0; i <= 19; i++) {
            frame[i] = cor_vermelha;
        }
    }
    else if (nivel_percentual >= 100.0) {
        // Todas as linhas - vermelha (nível crítico)
        for (int i = 0; i <= 24; i++) {
            frame[i] = cor_vermelha;
        }
    }

    // Envia o frame completo para a matriz WS2812
    for (int i = 0; i < NUM_LEDS; i++) {
        ws2812_put_pixel(frame[i]);
    }
    sleep_us(70);
}

/**
 * Controla o buzzer baseado no nível de água
 */
void alerta_buzzer(float nivel_percentual) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    static int contador = 0;
    contador++;

    // Buzzer intermitente quando fora dos limites
    if (nivel_percentual < lim_min || nivel_percentual > lim_max) {
        if (contador % 4 < 2) {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, 1250);  // Liga buzzer
        } else {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);     // Desliga buzzer
        }
    } else {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);         // Buzzer desligado
    }
}

/**
 * Controla a bomba baseada nos limites configurados
 */
void controla_bomba(float nivel) {
    if (nivel < lim_min && !bomba_ligada) {
        gpio_put(RELAY_PIN, 0);  // Liga a bomba
        bomba_ligada = true;
    } else if (nivel > lim_max && bomba_ligada) {
        gpio_put(RELAY_PIN, 1);  // Desliga a bomba
        bomba_ligada = false;
    }
}

/**
 * Atualiza as informações no display OLED
 */
void atualiza_display(ssd1306_t *ssd, uint16_t adc_value) {
    char buffer_adc[20];
    char buffer_nivel[10];
    
    sprintf(buffer_nivel, "%1.0f%%", nivel_percentual);
    sprintf(buffer_adc, "ADC: %d", adc_value);
    
    ssd1306_fill(ssd, false);
    ssd1306_draw_string(ssd, "Nivel de Agua:", 8, 6); 
    ssd1306_draw_string(ssd, buffer_nivel, 8, 22); 
    ssd1306_draw_string(ssd, buffer_adc, 8, 41);
    ssd1306_draw_string(ssd, bomba_ligada ? "Bomba: LIGADA" : "Bomba: DESLIGADA", 8, 52);  
    ssd1306_send_data(ssd);
}

// ===== FUNÇÃO PRINCIPAL =====
int main() {
    // Inicialização do hardware
    inicializar_hardware();
    
    // Inicialização do display
    ssd1306_t ssd;
    inicializar_display(&ssd);
    
    // Inicialização do servidor web
    inicializar_webserver(&ssd);
    
    uint16_t adc_value_x;
    
    // Loop principal
    while (true) {
        // Poll do WiFi
        cyw43_arch_poll();
        
        // Reset dos limites se solicitado pelo botão A
        if (resetar_limites) {
            lim_min = LIM_MIN_PADRAO;
            lim_max = LIM_MAX_PADRAO;
            resetar_limites = false;
        }

        // Leitura do ADC e cálculo do nível percentual
        adc_select_input(2); 
        adc_value_x = adc_read(); 
        nivel_percentual = (adc_value_x * 100.0) / 4095.0;

        // Controle da bomba
        controla_bomba(nivel_percentual);
        
        // Atualização do display
        atualiza_display(&ssd, adc_value_x);
        
        // Atualização da matriz de LEDs
        atualiza_matriz(nivel_percentual);
        
        // Controle do buzzer de alerta
        alerta_buzzer(nivel_percentual);
        
        sleep_ms(500);
    }
    
    return 0;
}

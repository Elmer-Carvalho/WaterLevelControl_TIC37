#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#include "lib/ssd1306.h"
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  // GPIO compartilhada com o microfone na BitDogLab.
#define RELAY_PIN 8

volatile float lim_min = 30.0;
volatile float lim_max = 70.0;
volatile float nivel_percentual = 0;
volatile bool bomba_ligada = false;

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASSWORD"

const char HTML_BODY[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Controle de Nivel</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
    "body { font-family: sans-serif; text-align: center; padding: 20px; background: #f0f0f0; }"
    "h1 { color: #333; }"
    ".container { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); max-width: 400px; margin: auto; }"
    "p { font-size: 18px; } #status { font-weight: bold; }"
    "form { margin-top: 20px; } label { display: block; margin-bottom: 5px; font-weight: bold; }"
    "input[type=number] { width: 90%; padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px; }"
    "input[type=submit] { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }"
    "</style>"
    "<script>"
    "function atualizar() {"
    "  fetch('/estado').then(res => res.json()).then(data => {"
    "    document.getElementById('nivel').innerText = data.nivel + '%';"
    "    document.getElementById('bomba').innerText = data.bomba ? 'LIGADA' : 'Desligada';"
    "    document.getElementById('bomba').style.color = data.bomba ? '#4CAF50' : '#f44336';"
    "  });"
    "}"
    "setInterval(atualizar, 2000);"
    "</script></head><body onload='atualizar()'>"
    "<div class='container'>"
    "<h1>Controle de Nível</h1>"
    "<p>Nível Atual: <span id='nivel'>--</span></p>"
    "<p>Status da Bomba: <span id='bomba'>--</span></p>"
    "<form action='/limites' method='get'>"
    "<label for='min'>Limite Mínimo (%):</label>"
    "<input type='number' id='min' name='min' required>"
    "<label for='max'>Limite Máximo (%):</label>"
    "<input type='number' id='max' name='max' required>"
    "<input type='submit' value='Atualizar Limites'>"
    "</form>"
    "</div></body></html>";


struct http_state {
    char response[4096];
    size_t len;
    size_t sent;
};

// Esta função é chamada quando os dados são enviados com sucesso
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Esta é a função principal que processa as requisições
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    hs->sent = 0;

    // Lógica para lidar com as diferentes URLs
    if (strstr(req, "GET /limites")) {
      // Extrai os novos limites da URL
      char *min_str = strstr(req, "min=");
      char *max_str = strstr(req, "max=");
        if (min_str && max_str) {
            sscanf(min_str, "min=%f", &lim_min);
            sscanf(max_str, "max=%f", &lim_max);
        }
      // Apenas redireciona para a página principal
      const char *redir_hdr = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
      hs->len = snprintf(hs->response, sizeof(hs->response), redir_hdr);

    } else if (strstr(req, "GET /estado")) {
        // Prepara um JSON com o estado atual do sistema
        char json_payload[128];
        // OBS: Você precisa ter as variáveis 'nivel_percentual' e 'bomba_ligada' como globais
        // para que esta função possa acessá-las. Vamos ajustar isso.
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                  "{\"nivel\":%.1f,\"bomba\":%s}",
                                  nivel_percentual, bomba_ligada ? "true" : "false");

        hs->len = snprintf(hs->response, sizeof(hs->response),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n%s",
                          json_len, json_payload);
    } else {
      // Se nenhuma URL especial for encontrada, serve a página principal
      hs->len = snprintf(hs->response, sizeof(hs->response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n\r\n%s",
                        (int)strlen(HTML_BODY), HTML_BODY);

      }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

// Callback para aceitar novas conexões
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Função para iniciar o servidor
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP iniciado na porta 80\n");
}

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

int main()
{
  stdio_init_all();
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

  //Configurações de Wi-Fi
     //Inicializa a arquitetura do cyw43
    if (cyw43_arch_init()) {
        printf("Falha para iniciar o cyw43\n");
        return - 1;
    }

    cyw43_arch_enable_sta_mode();

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Conectando ao", 8, 6);
    ssd1306_draw_string(&ssd, WIFI_SSID, 8, 22);
    ssd1306_send_data(&ssd);

    // Conecta com um tempo limite de 10 segundos
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
    printf("Falha para conectar ao Wi-Fi\n");
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi: FALHA", 8, 22);
    ssd1306_send_data(&ssd);
    return -1;
    } else {
    printf("Conectado ao Wi-Fi!\n");

    // Pega o endereço IP e prepara para mostrar no display
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str);

    // Exibe o IP no display
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "IP:", 8, 6);
    ssd1306_draw_string(&ssd, ip_str, 8, 22);
    ssd1306_send_data(&ssd);
    sleep_ms(2000); // Mostra o IP por 2 segundos
    

    start_http_server();
    }

  uint16_t adc_value_x;
  char buffer_adc[10];  // Buffer para armazenar a string
  char buffer_nivel[10]; // Buffer para armazenar a string
  
  bool cor = true;
  while (true)
  {

    cyw43_arch_poll();
  
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
    sleep_ms(700);
  }
}
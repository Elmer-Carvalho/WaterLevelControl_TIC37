#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

#include "webserver.h" // Inclui o nosso novo cabeçalho

extern volatile float lim_min;
extern volatile float lim_max;
extern volatile float nivel_percentual;
extern volatile bool bomba_ligada;

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASSWORD"

// Conteúdo da página HTML
const char HTML_BODY[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Controle de Nivel</title>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
    "body { font-family: 'Poppins', Tahoma, Geneva, Verdana, sans-serif; text-align: center; padding: 20px; background: linear-gradient(135deg,rgb(104, 169, 243) 0%, #764ba2 100%); display: flex; justify-content: center; align-items: center; height: 99vh; margin: 0;  }"
    "h1 { color: #764ba2 }"
    ".container { background: rgba(255, 255, 255, 0.95); padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); max-width: 400px; margin: auto;}"
    "p { font-size: 18px; } #status { font-weight: bold; }"
    "form { margin-top: 20px; } label { display: block; margin-bottom: 5px; font-weight: bold; }"
    "input[type=number] { width: 90%; padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px; }"
    "input[type=submit] { background: white; color: #764ba2 ; padding: 10px 20px; border: none; border-radius: 12px; font-size: 16px; cursor: pointer; font-weight: bold; transition: background 0.3s, color 0.3s; }"
    ".card-limites { background: linear-gradient(135deg, #764ba2 0%,rgb(104, 169, 243) 100%); padding: 10px; margin-top: 30px; border-radius: 10px; box-shadow: inset 0 0 5px rgba(0,0,0,0.05); text-align: left; text-align: center; color:rgb(255, 255, 255); }"
    "</style>"
    "<script>"
    "function atualizar() {"
    "  fetch('/estado').then(res => res.json()).then(data => {"
    "    document.getElementById('nivel').innerText = data.nivel + '%';"
    "    document.getElementById('barra').style.width = data.nivel + '%';"
    "    document.getElementById('bomba').innerText = data.bomba ? 'LIGADA' : 'Desligada';"
    "    document.getElementById('bomba').style.color = data.bomba ? '#4CAF50' : '#f44336';"
    "  });"
    "}"
    "setInterval(atualizar, 1000);"
    "</script></head><body onload='atualizar()'>"
    "<div class='container'>"
    "<div style='font-size: 48px;'>💧</div>"
    "<h1>Controle de Nível de Água</h1>"
    "<p style='text-align: center; font-weight: bold;'>Nível Atual:</p>"
    "<div style='position: relative; height: 24px; background: #eee; border-radius: 12px; overflow: hidden;'>"
    "<div id='barra' style='height: 100%; width: 0%; background:linear-gradient(135deg,rgb(104, 169, 243) 0%,rgb(107, 109, 197) 100%);;'></div>"
    "<span id='nivel' style='position: absolute; top: 2px; left: 50%; transform: translateX(-50%); font-weight: bold; color:rgb(167, 179, 233);'></span>"
    "</div>"
    "<p style='font-weight: bold;'>Status da Bomba: <span id='bomba'>--</span></p>"
    "<div class='card-limites'>"
    "<h2>Gerenciar Limites</h2>"
    "<form action='/limites' method='get'>"
    "<label for='min'>Limite Mínimo (%):</label>"
    "<input type='number' id='min' name='min' required>"
    "<label for='max'>Limite Máximo (%):</label>"
    "<input type='number' id='max' name='max' required>"
    "<input type='submit' value='Atualizar Limites'>"
        "</form>"
        "</div>"
    "</div></body></html>";

// Estrutura para manter o estado da resposta HTTP
struct http_state {
    char response[4096];
    size_t len;
    size_t sent;
};

// Callback chamado quando os dados são enviados com sucesso
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Função principal que processa as requisições HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    hs->sent = 0;

    if (strstr(req, "GET /limites")) {
        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");
        if (min_str && max_str) {
            sscanf(min_str, "min=%f", &lim_min);
            sscanf(max_str, "max=%f", &lim_max);
        }
        const char *redir_hdr = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
        hs->len = snprintf(hs->response, sizeof(hs->response), redir_hdr);

    } else if (strstr(req, "GET /estado")) {
        char json_payload[128];
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

// Função para iniciar o listener do servidor
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP iniciado na porta 80\n");
}


// Função de inicialização principal do webserver
bool webserver_init(void) {
    if (cyw43_arch_init()) {
        printf("Falha para iniciar o cyw43\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 15000)) {
        printf("Falha para conectar ao Wi-Fi\n");
        cyw43_arch_deinit();
        return false;
    }
    
    printf("Conectado com sucesso!\n");
    start_http_server();
    return true;
}
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <RadioLib.h>
#include "Config.h"
#include "DisplayManager.h"

// --- Instâncias Globais ---
DisplayManager ui;
unsigned long lastInteraction = 0;
bool isDisplayOn = true;
const unsigned long DISPLAY_TIMEOUT = 20000;

// Declaração do Rádio
SX1276 lora = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);

// --- Variáveis de Estado e Tráfego ---
SystemState currentState = STATE_MENU;
float totalDown = 0;
float totalUp = 0;
unsigned long lastNetCheck = 0;

// --- Variáveis LoRa ---
String lastLoraData = "Aguardando...";
int loraRSSI = 0;
float loraSNR = 0;
uint32_t packetCount = 0;

// --- Configuração do Menu ---
int menuIndex = 0;
const char *menuOptions[] = {"WiFi Monitor", "LoRa Stats", "System Info"};
int totalOptions = 3;

// --- Controle de Botão ---
unsigned long buttonPressTime = 0;
bool lastButtonState = HIGH;

void sendToFirebaseREST(String data, int rssi, float snr, uint32_t packets) {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure(); 
        HTTPClient http;

        String url = "https://" + String(FIREBASE_HOST) + "/rastreador/status.json";
        
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");

        // Criando um JSON completo com todos os parâmetros
        String json = "{";
        json += "\"localizacao\":\"" + data + "\",";
        json += "\"rssi\":" + String(rssi) + ",";
        json += "\"snr\":" + String(snr) + ",";
        json += "\"packets\":" + String(packets);
        json += "}";

        http.PATCH(json);
        http.end();
    }
}

// Função de Diagnóstico de Reset
String getResetReason()
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "Power On";
    case ESP_RST_SW:
        return "Software";
    case ESP_RST_WDT:
        return "Watchdog";
    case ESP_RST_BROWNOUT:
        return "Brownout";
    default:
        return "Outro";
    }
}

void handleButton() {
    bool currentStateBtn = digitalRead(BUTTON_PIN);

    // Botão Pressionado
    if (lastButtonState == HIGH && currentStateBtn == LOW) {
        buttonPressTime = millis();
        lastInteraction = millis(); // Reset do timer apenas na interação física

        if (!isDisplayOn) {
            ui.turnOn();
            isDisplayOn = true;
            lastButtonState = LOW;
            return;
        }
    }

    // Botão Solto
    if (lastButtonState == LOW && currentStateBtn == HIGH) {
        unsigned long duration = millis() - buttonPressTime;
        lastInteraction = millis(); // Reset do timer ao soltar

        if (duration > 800) {
            currentState = (currentState == STATE_MENU) ? (SystemState)(menuIndex + 1) : STATE_MENU;
            if (currentState == STATE_LORA_RECEIVER) lora.startReceive();
        } else if (duration > 50 && currentState == STATE_MENU) {
            menuIndex = (menuIndex + 1) % totalOptions;
        }
    }
    lastButtonState = currentStateBtn;
}

void updateNetworkStats()
{
    if (WiFi.status() == WL_CONNECTED && (millis() - lastNetCheck > 10000))
    {
        HTTPClient http;
        http.begin("http://www.google.com");
        http.setTimeout(1500);
        if (http.GET() > 0)
        {
            totalDown += (http.getSize() > 0) ? (http.getSize() / 1024.0) : 0.5;
            totalUp += 0.2;
        }
        http.end();
        lastNetCheck = millis();
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    ui.init();

    // Pino DIO0 é essencial para saber quando o pacote chegou
    pinMode(LORA_DIO0, INPUT);

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = true};
    esp_task_wdt_reconfigure(&twdt_config);
#else
    esp_task_wdt_init(10, true);
#endif
    esp_task_wdt_add(NULL);

    Serial.print(F("[LoRa] Inicializando... "));
    int state = lora.begin(915.0);
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("Sucesso!"));
        lora.startReceive(); // Ativa a escuta inicial
    }
    else
    {
        Serial.printf("Erro (%d)\n", state);
    }

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Conectando ao WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" conectado!");
}

void loop()
{
    esp_task_wdt_reset();
    handleButton();

    // 1. RECEPÇÃO LORA OTIMIZADA (Verifica pino de interrupção DIO0)
    if (digitalRead(LORA_DIO0) == HIGH)
    {
        String str;

        // Captura RSSI/SNR ANTES de ler os dados para garantir precisão
        int currentRSSI = lora.getRSSI();
        float currentSNR = lora.getSNR();

        int state = lora.readData(str);

        if (state == RADIOLIB_ERR_NONE)
        {
            str.trim();
            if (str.startsWith("L:"))
            {
                packetCount++;

                // Atualiza variáveis globais com novos valores
                loraRSSI = currentRSSI;
                loraSNR = currentSNR;

                String cleanData = str.substring(2);
                cleanData.replace("|", " ");
                lastLoraData = cleanData;

                Serial.println("[Heltec] Novo Pacote: " + lastLoraData + " | RSSI: " + String(loraRSSI));

                // ENVIO PARA O APP (Firebase)
                sendToFirebaseREST(lastLoraData, loraRSSI, loraSNR, packetCount);
            }
        }
        // Reseta o rádio para continuar ouvindo o próximo pacote
        lora.startReceive();
    }

    // 2. GESTÃO DE STANDBY (Único bloco)
    if (isDisplayOn && (millis() - lastInteraction > DISPLAY_TIMEOUT)) {
        Serial.println("[Sistema] Standby ativado por inatividade do botão.");
        ui.turnOff();
        isDisplayOn = false;
    }

    // 3. ATUALIZAÇÃO DA UI (Sincronizada)
    if (isDisplayOn)
    {
        static unsigned long lastUI = 0;
        if (millis() - lastUI > 200)
        {
            switch (currentState)
            {
            case STATE_LORA_RECEIVER:
                ui.showLoRaStats(lastLoraData, loraRSSI, loraSNR, packetCount);
                break;
            case STATE_MENU:
                ui.showMenu(menuOptions, totalOptions, menuIndex);
                break;
            case STATE_WIFI_STATS:
                updateNetworkStats();
                ui.showDashboard(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Off", totalDown, totalUp, WiFi.RSSI());
                break;
            case STATE_SYSTEM_INFO:
                ui.showSystemInfo(ESP.getFreeHeap(), millis(), getResetReason());
                break;
            }
            lastUI = millis();
        }
    }
    else
    {
        // Se a tela estiver apagada, apenas processa rede em background
        updateNetworkStats();
    }

    delay(10);
}
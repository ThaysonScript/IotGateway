#include "DisplayManager.h"

DisplayManager::DisplayManager() : display(0x3c, OLED_SDA, OLED_SCL) {}

void DisplayManager::init() {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW); delay(50); digitalWrite(OLED_RST, HIGH);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
}

// Desenha uma barra superior elegante
void DisplayManager::drawHeader(String title) {
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, title);
    display.drawHorizontalLine(0, 13, 128);
}

// Desenha um ícone de sinal de celular baseado no RSSI
void DisplayManager::drawSignalBar(int x, int y, int rssi) {
    int bars = map(constrain(rssi, -120, -30), -120, -30, 1, 5);
    for (int i = 0; i < 5; i++) {
        int height = (i + 1) * 2;
        if (i < bars) display.fillRect(x + (i * 3), y + (10 - height), 2, height);
        else display.drawRect(x + (i * 3), y + (10 - height), 2, height);
    }
}

void DisplayManager::showMenu(const char* options[], int count, int selectedIndex) {
    display.clear();
    drawHeader("GATEWAY MENU");
    
    for(int i = 0; i < count; i++) {
        if (i == selectedIndex) {
            display.fillRect(0, 16 + (i * 14), 128, 13);
            display.setColor(BLACK);
        } else {
            display.setColor(WHITE);
        }
        display.drawString(5, 17 + (i * 14), options[i]);
        display.setColor(WHITE);
    }
    display.display();
}

void DisplayManager::showLoRaStats(String lastPacket, int rssi, float snr, uint32_t count) {
    display.clear();
    
    // 1. Cabeçalho com Título e RSSI Numérico
    drawHeader("LORA MONITOR");
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(108, 0, String(rssi) + "dBm"); // RSSI numérico ao lado das barras
    drawSignalBar(112, 2, rssi);

    // 2. Linha Central: Contador de Pacotes e SNR
    // Lado Esquerdo: "Pkts" em destaque
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 16, "Pacotes:");
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 26, "#" + String(count));

    // Lado Direito: "SNR"
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 16, "Qualidade (SNR):");
    display.setFont(ArialMT_Plain_16);
    display.drawString(127, 26, String(snr, 1) + "dB");

    // 3. Moldura Inferior: Dados do GPS (Last Data)
    display.setFont(ArialMT_Plain_10);
    display.drawRect(0, 44, 128, 20); // Moldura para as coordenadas
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    
    if(lastPacket.length() > 0) {
        // Exibe o conteúdo vindo do STM32
        display.drawString(64, 48, lastPacket);
    } else {
        display.drawString(64, 48, "Aguardando sinal...");
    }

    display.display();
}

void DisplayManager::showDashboard(String ip, float down, float up, int rssi) {
    display.clear();
    drawHeader("WIFI MONITOR");
    drawSignalBar(110, 2, rssi);

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 16, "IP: " + ip);
    
    // Gráfico simples de tráfego
    display.drawRect(0, 32, 60, 20);
    display.drawString(2, 34, "D:" + String(down, 1));
    
    display.drawRect(64, 32, 60, 20);
    display.drawString(66, 34, "U:" + String(up, 1));

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 54, "Conexão Estável");
    
    display.display();
}

void DisplayManager::showSystemInfo(uint32_t heap, uint32_t uptime, String resetReason) {
    display.clear();
    drawHeader("SYSTEM HEALTH");
    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 16, "RAM Livre: " + String(heap / 1024) + " KB");
    display.drawString(0, 28, "Uptime: " + String(uptime / 60000) + " min");
    display.drawString(0, 40, "Causa Reset: " + resetReason);
    
    // Barra de progresso para a memória (estimativa de 250KB total)
    float heapPerc = (heap / 250000.0) * 100;
    display.drawProgressBar(0, 54, 120, 8, (int)heapPerc);
    
    display.display();
}

void DisplayManager::turnOff() { display.displayOff(); }
void DisplayManager::turnOn() { display.displayOn(); }
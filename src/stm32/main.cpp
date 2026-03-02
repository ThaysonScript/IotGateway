#include <Arduino.h>
#include <RadioLib.h>
#include <TinyGPS++.h>

#define LORA_NSS PA4
#define LORA_DIO0 PA1
#define LORA_RST PA0
#define LORA_DIO1 PA2
#define LED_PIN PC13

SX1276 lora = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
TinyGPSPlus gps;
HardwareSerial SerialGPS(PA10, PA9); 

uint32_t txCount = 0; // Contador para provar que a mensagem muda

void setup() {
    Serial.begin(115200);
    SerialGPS.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    int state = lora.begin(915.0);
    if (state == RADIOLIB_ERR_NONE) {
        lora.setOutputPower(2);
        lora.startReceive();
    }
}

void loop() {
    // 1. Alimentar o GPS constantemente
    while (SerialGPS.available() > 0) {
        gps.encode(SerialGPS.read());
    }

    // 2. Envio Não-Bloqueante a cada 3 segundos
    static unsigned long lastTx = 0;
    if (millis() - lastTx > 3000) {
        txCount++;
        String msg = "L:ID" + String(txCount) + " ";
        
        if (gps.location.isValid()) {
            msg += "A" + String(gps.location.lat(), 4) + "|O" + String(gps.location.lng(), 4);
        } else {
            msg += "Buscando GPS...";
        }

        digitalWrite(LED_PIN, LOW);
        int state = lora.transmit(msg);
        digitalWrite(LED_PIN, HIGH);
        
        Serial.println("TX: " + msg);
        lastTx = millis();
        lora.startReceive(); // Volta a ouvir
    }
}
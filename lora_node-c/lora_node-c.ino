#include <Arduino.h>
#include "LoRa_E22.h"

// Node C: End sensor node (Fixed Transmission)

// E22 Pin Definitions
#define LORA_RX 32
#define LORA_TX 33

#define AUX_PIN 21
#define M0_PIN  22
#define M1_PIN  23

HardwareSerial loraSerial(2);

LoRa_E22 e22(
    &loraSerial,
    AUX_PIN,
    M0_PIN,
    M1_PIN
);

// Network Configuration
#define CHANNEL 79

// Node Addresses
#define NODE_A_ADDH 0x00
#define NODE_A_ADDL 0x01

#define NODE_B_ADDH 0x00
#define NODE_B_ADDL 0x02

#define NODE_C_ADDH 0x00
#define NODE_C_ADDL 0x03

// Sensor C Pin Definitions & Setup
#define SENSOR_POWER_PIN 27
#define SENSOR_RX 25
#define SENSOR_TX 26

HardwareSerial sensor(1);

// Read distance from UART ultrasonic sensor on demand
int readSensorOnDemand() {
    int lastDistance = -1;

    // Clear old UART bytes
    while (sensor.available()) {
        sensor.read();
    }

    unsigned long startTime = millis();

    while (millis() - startTime < 1000) {
        if (sensor.available() >= 4) {
            byte header = sensor.read();

            if (header != 0xFF) continue;

            byte highByte = sensor.read();
            byte lowByte = sensor.read();
            byte checksum = sensor.read();

            byte calculatedChecksum =
                (0xFF + highByte + lowByte) & 0xFF;

            if (checksum != calculatedChecksum) continue;

            int distance = (highByte << 8) | lowByte;

            lastDistance = distance;
        }
    }

    return lastDistance;
}

// Send fixed message to Node B
void sendToNodeB(String message) {
    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("NODE C -> NODE B");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("----------------------------------------");

    ResponseStatus rs = e22.sendFixedMessage(
        NODE_B_ADDH,
        NODE_B_ADDL,
        CHANNEL,
        message
    );

    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}

// Process CALL from Node B (e.g. CALL:001)
void processCall(String requestId) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C PROCESSING REQUEST");
    Serial.println("REQUEST ID: " + requestId);
    Serial.println("========================================");

    int sensorValueC = readSensorOnDemand();

    Serial.println();
    Serial.print("RANDOM SENSOR C VALUE: ");
    Serial.println(sensorValueC);

    String dataMessage =
        "DATA:" +
        requestId +
        ":C:" +
        String(sensorValueC) +
        "break";

    Serial.println();
    Serial.println("CREATED DATA PACKET:");
    Serial.println(dataMessage);

    sendToNodeB(dataMessage);

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C REQUEST COMPLETE");
    Serial.println("RETURN TO IDLE");
    Serial.println("========================================");
}

// Handle CALL packet
void handleCall(String message) {
    int colonIndex = message.indexOf(':');

    if (colonIndex == -1) {
        Serial.println("ERROR: INVALID CALL PACKET");
        return;
    }

    // Extract request ID
    String requestId =
        message.substring(colonIndex + 1);

    requestId.trim();

    // Validate request ID
    if (requestId.length() == 0) {
        Serial.println("ERROR: EMPTY REQUEST ID");
        return;
    }

    // Process request
    processCall(requestId);
}

// Listen for incoming LoRa messages
void receiveLoRa() {
    if (e22.available() > 1) {
        ResponseContainer rc =
            e22.receiveMessage();

        // Check receive status
        if (rc.status.code != 1) {
            Serial.println();
            Serial.println("LORA RECEIVE ERROR");
            Serial.println(
                rc.status.getResponseDescription()
            );
            return;
        }

        // Get message
        String message = rc.data;
        message.trim();

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE C RECEIVED");
        Serial.println("MESSAGE: " + message);
        Serial.println("========================================");

        // Handle CALL from Node B
        if (message.startsWith("CALL:")) {
            handleCall(message);
            return;
        }

        // Ignore other messages
        Serial.println();
        Serial.println("UNKNOWN / IGNORED MESSAGE");
        Serial.println(message);
    }
}

void setup() {
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C STARTING");
    Serial.println("ROLE: SENSOR NODE");
    Serial.println("MODE: FIXED ONLY");
    Serial.println("========================================");

    randomSeed(micros());

    loraSerial.begin(
        9600,
        SERIAL_8N1,
        LORA_RX,
        LORA_TX
    );

    sensor.begin(
        9600,
        SERIAL_8N1,
        SENSOR_RX,
        SENSOR_TX
    );

    e22.begin();

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE C READY");
    Serial.println("WAITING FOR NODE B...");
    Serial.println("========================================");
}

void loop() {
    receiveLoRa();
    delay(10);
}
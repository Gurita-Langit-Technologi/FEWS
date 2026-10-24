#include <Arduino.h>
#include "LoRa_E220.h"

// Node A: Gateway between Raspberry Pi (Transparent) and Nodes B & C (Fixed)

// E220 Pin Definitions
#define LORA_RX 16
#define LORA_TX 17

#define AUX_PIN 21
#define M0_PIN  22
#define M1_PIN  23

HardwareSerial loraSerial(2);

LoRa_E220 e220ttl(
    &loraSerial,
    AUX_PIN,
    M0_PIN,
    M1_PIN
);

// Network Configuration
#define CHANNEL 79

#define NODE_A_ADDH 0x00
#define NODE_A_ADDL 0x01

#define NODE_B_ADDH 0x00
#define NODE_B_ADDL 0x02

#define NODE_C_ADDH 0x00
#define NODE_C_ADDL 0x03

// Collection State
enum E220OperationMode {
    MODE_TRANSPARENT,
    MODE_FIXED
};

E220OperationMode currentMode = MODE_TRANSPARENT;

bool collecting = false;

String currentRequestId = "";

String dataA = "";
String dataB = "";
String dataC = "";

bool gotB = false;
bool gotC = false;

unsigned long collectionStart = 0;

const unsigned long COLLECTION_TIMEOUT = 15000;

// Read Sensor A
int readSensorA() {
    return random(10, 95);
}

// Change E220 transmission mode (Transparent / Fixed)
bool changeTransmissionMode(E220OperationMode newMode) {
    if (currentMode == newMode) {
        return true;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("CHANGING E220 TRANSMISSION MODE");

    if (newMode == MODE_FIXED) {
        Serial.println("NEW MODE: FIXED");
    } else {
        Serial.println("NEW MODE: TRANSPARENT");
    }

    Serial.println("========================================");

    // Read current configuration
    ResponseStructContainer c = e220ttl.getConfiguration();

    if (c.status.code != 1) {
        Serial.println("FAILED TO READ CONFIG");
        Serial.println(c.status.getResponseDescription());
        return false;
    }

    Configuration configuration = *(Configuration *)c.data;

    // Set transmission mode
    if (newMode == MODE_FIXED) {
        configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    } else {
        configuration.TRANSMISSION_MODE.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
    }

    // Write new configuration to E220 (RAM only)
    ResponseStatus rs = e220ttl.setConfiguration(
        configuration,
        WRITE_CFG_PWR_DWN_LOSE
    );

    if (rs.code != 1) {
        Serial.println("FAILED TO WRITE CONFIG");
        Serial.println(rs.getResponseDescription());
        c.close();
        return false;
    }
    c.close();

    currentMode = newMode;

    Serial.println("CONFIGURATION CHANGED SUCCESSFULLY");

    // Allow E220 module to stabilize
    delay(500);

    return true;
}

// Send fixed message to Node B
void sendToNodeB(String message) {
    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("NODE A -> NODE B");
    Serial.println("MODE    : FIXED");
    Serial.println("MESSAGE : " + message);
    Serial.println("--------------------------------");

    ResponseStatus rs = e220ttl.sendFixedMessage(
        NODE_B_ADDH,
        NODE_B_ADDL,
        CHANNEL,
        message
    );

    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}

// Send transparent message to Raspberry Pi
void sendToPi(String message) {
    if (!changeTransmissionMode(MODE_TRANSPARENT)) {
        Serial.println("FAILED CHANGE MODE TO TRANSPARENT");
        collecting = false;
        return;
    }

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("NODE A -> PI");
    Serial.println("MODE    : TRANSPARENT");
    Serial.println("MESSAGE : " + message);
    Serial.println("--------------------------------");

    ResponseStatus rs = e220ttl.sendMessage(message);

    Serial.print("STATUS: ");
    Serial.println(rs.getResponseDescription());
}

// Start collection upon receiving CALL from Pi (e.g. CALL:001)
void startCollection(String requestId) {
    // Prevent overlapping requests
    if (collecting) {
        Serial.println();
        Serial.println("WARNING: ALREADY COLLECTING");
        Serial.println("IGNORE NEW CALL");
        return;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE A START COLLECTION");
    Serial.println("REQUEST ID: " + requestId);
    Serial.println("========================================");

    // Reset state
    currentRequestId = requestId;
    collecting = true;
    gotB = false;
    gotC = false;
    dataA = "";
    dataB = "";
    dataC = "";
    collectionStart = millis();

    // Read sensor A
    int sensorValueA = readSensorA();
    dataA = String(sensorValueA);

    Serial.println();
    Serial.println("NODE A SENSOR DATA");
    Serial.println(dataA);

    // Forward CALL to Node B in Fixed Mode
    if (!changeTransmissionMode(MODE_FIXED)) {
        Serial.println("FAILED TO CHANGE TO FIXED MODE");
        collecting = false;
        return;
    }

    String callMessage =
        "CALL:" +
        currentRequestId;

    sendToNodeB(callMessage);
}

// Handle DATA packet from Node B (e.g. DATA:001:B:120:C:250)
void handleData(String message) {
    if (!collecting) {
        Serial.println();
        Serial.println("WARNING: NOT COLLECTING");
        Serial.println("IGNORE DATA: " + message);
        return;
    }

    // Parse packet fields (DATA:REQUEST_ID:NODE_B:VAL_B:NODE_C:VAL_C)
    int p1 = message.indexOf(':');
    int p2 = message.indexOf(':', p1 + 1);
    int p3 = message.indexOf(':', p2 + 1);
    int p4 = message.indexOf(':', p3 + 1);
    int p5 = message.indexOf(':', p4 + 1);
    int p6 = message.indexOf(':', p5 + 1);
    int p7 = message.indexOf(':', p6 + 1);

    if (p1 == -1 ||
        p2 == -1 ||
        p3 == -1) {
        Serial.println();
        Serial.println("ERROR: INVALID DATA PACKET");
        Serial.println(message);
        return;
    }

    String packetType =
        message.substring(0, p1);

    String requestId =
        message.substring(p1 + 1, p2);

    String nodeB =
        message.substring(p2 + 1, p3);

    String valueB =
        message.substring(p3 + 1, p4);

    String nodeC =
        message.substring(p4 + 1, p5);

    String valueC =
        message.substring(p5 + 1, p6);

    // Validate packet type
    if (packetType != "DATA") {
        Serial.println("ERROR: UNKNOWN PACKET TYPE");
        return;
    }

    // Validate request ID
    if (requestId != currentRequestId) {
        Serial.println();
        Serial.println("WARNING: WRONG REQUEST ID");
        Serial.println(
            "EXPECTED: " +
            currentRequestId
        );
        Serial.println(
            "RECEIVED: " +
            requestId
        );
        return;
    }

    // Process Node B data
    if (nodeB == "B") {
        dataB = valueB;
        gotB = true;

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE A RECEIVED DATA FROM B");
        Serial.println("DATA: " + dataB);
        Serial.println("========================================");
    }

    // Process Node C data
    if (nodeC == "C") {
        dataC = valueC.toInt() / 10;
        gotC = true;

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE A RECEIVED DATA FROM C");
        Serial.println("DATA: " + dataC);
        Serial.println("========================================");
    } else {
        Serial.println();
        Serial.println("WARNING: UNKNOWN NODE");
        Serial.println(
            "NODE: " + nodeB
        );
        return;
    }

    // Complete collection if all nodes responded
    if (gotB && gotC) {
        finishCollection();
    }
}

// Assemble final data and send to Raspberry Pi
void finishCollection() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("ALL SENSOR DATA RECEIVED");
    Serial.println("========================================");

    Serial.println("A: " + dataA);
    Serial.println("B: " + dataB);
    Serial.println("C: " + dataC);

    // Final message format for Pi: DATA:<ID>:A:<VAL_A>:B:<VAL_B>:C:<VAL_C>break
    String finalMessage =
        "DATA:" +
        currentRequestId +
        ":A:" +
        dataA +
        ":B:" +
        dataB +
        ":C:" +
        dataC +
        "break";

    Serial.println();
    Serial.println("FINAL MESSAGE FOR PI:");
    Serial.println(finalMessage);

    // Send to Pi in Transparent Mode
    sendToPi(finalMessage);

    collecting = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println("COLLECTION COMPLETE");
    Serial.println("NODE A RETURN TO IDLE");
    Serial.println("========================================");
}

// Listen for incoming LoRa messages
void receiveLoRa() {
    if (e220ttl.available() > 1) {
        ResponseContainer rc =
            e220ttl.receiveMessage();

        if (rc.status.code != 1) {
            Serial.println();
            Serial.println("LORA RECEIVE ERROR:");
            Serial.println(
                rc.status.getResponseDescription()
            );
            return;
        }

        String message = rc.data;
        message.trim();

        Serial.println();
        Serial.println("========================================");
        Serial.println("NODE A RECEIVED");
        Serial.println("MESSAGE: " + message);
        Serial.println("========================================");

        // CALL message from Raspberry Pi
        if (message.startsWith("CALL:")) {
            String requestId =
                message.substring(5);
            requestId.trim();

            if (requestId.length() == 0) {
                Serial.println(
                    "ERROR: EMPTY REQUEST ID"
                );
                return;
            }

            startCollection(requestId);
            return;
        }

        // DATA message from Node B
        if (message.startsWith("DATA:")) {
            handleData(message);
            return;
        }

        Serial.println();
        Serial.println("UNKNOWN MESSAGE");
        Serial.println(message);
    }
}

// Handle collection timeout
void checkCollectionTimeout() {
    if (!collecting) {
        return;
    }

    unsigned long elapsed =
        millis() - collectionStart;

    if (elapsed >= COLLECTION_TIMEOUT) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("COLLECTION TIMEOUT");
        Serial.println("========================================");

        Serial.print("REQUEST ID: ");
        Serial.println(currentRequestId);

        Serial.print("GOT B: ");
        Serial.println(gotB ? "YES" : "NO");

        Serial.print("GOT C: ");
        Serial.println(gotC ? "YES" : "NO");

        // Send partial data to Pi upon timeout
        String timeoutMessage =
            "DATA:" +
            currentRequestId +
            ":A:" +
            dataA +
            ":B:" +
            dataB +
            ":C:" +
            dataC + 
            "break";

        Serial.println();
        Serial.println("SEND PARTIAL DATA TO PI:");
        Serial.println(timeoutMessage);

        sendToPi(timeoutMessage);

        collecting = false;

        Serial.println();
        Serial.println("NODE A RETURN TO IDLE");
    }
}

void setup() {
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("NODE A STARTING");
    Serial.println("ROLE: PI GATEWAY");
    Serial.println("PI -> A : TRANSPARENT");
    Serial.println("A -> B  : FIXED");
    Serial.println("B -> A  : FIXED");
    Serial.println("A -> PI : TRANSPARENT");
    Serial.println("========================================");

    randomSeed(
        micros() ^
        analogRead(34)
    );

    loraSerial.begin(
        9600,
        SERIAL_8N1,
        LORA_RX,
        LORA_TX
    );

    e220ttl.begin();

    // Start in TRANSPARENT mode to listen for Pi
    currentMode = MODE_FIXED;

    if (!changeTransmissionMode(MODE_TRANSPARENT)) {
        Serial.println("FATAL: CANNOT SET TRANSPARENT MODE");
    } else {
        Serial.println("NODE A READY");
        Serial.println("MODE: TRANSPARENT");
        Serial.println("WAITING FOR PI...");
    }
}

void loop() {
    receiveLoRa();
    checkCollectionTimeout();
    delay(10);
}
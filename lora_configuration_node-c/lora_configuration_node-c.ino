// NODE C: ESP-W-IE + e22-900t22d
// CH/ADDL: 79/0x03

#include <Arduino.h>
#include "LoRa_E22.h"

#define RXD2 32
#define TXD2 33

#define AUX_PIN 21
#define M0_PIN 22
#define M1_PIN 23

LoRa_E22 e22(&Serial2, AUX_PIN, M0_PIN, M1_PIN);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    Serial.println("Starting...");

    e22.begin();

    ResponseStructContainer c = e22.getConfiguration();

    if (c.status.code != E22_SUCCESS)
    {
        Serial.println(c.status.getResponseDescription());
        return;
    }

    Configuration configuration = *(Configuration *)c.data;

    configuration.ADDH = 0x00;
    configuration.ADDL = 0x03;

    configuration.CHAN = 79;

    configuration.SPED.uartParity = MODE_00_8N1;
    configuration.SPED.uartBaudRate = UART_BPS_9600;
    configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;

    configuration.OPTION.subPacketSetting = SPS_240_00;
    configuration.OPTION.transmissionPower = POWER_22;
    configuration.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED;

    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    configuration.TRANSMISSION_MODE.enableLBT = LBT_DISABLED;
    configuration.TRANSMISSION_MODE.enableRSSI = RSSI_DISABLED;
    configuration.TRANSMISSION_MODE.WORPeriod = WOR_2000_011;

    ResponseStatus rs =
        e22.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);

    Serial.println(rs.getResponseDescription());

    c.close();

    delay(1000);

    ResponseStructContainer verify = e22.getConfiguration();
    Configuration cfg = *(Configuration *)verify.data;

    Serial.println("===== VERIFY =====");
    Serial.print("ADDH : ");
    Serial.println(cfg.ADDH);
    Serial.print("ADDL : ");
    Serial.println(cfg.ADDL);
    Serial.print("CHAN : ");
    Serial.println(cfg.CHAN);

    verify.close();
}

void loop()
{
}
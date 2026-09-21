/*
   NimBLE_Teensy_EddystoneTLM_Beacon

   Create a BLE server that sends periodic Eddystone TLM frames, on Teensy
   4.1 + Murata Type 1YN.

   EddystoneTLM frame specification https://github.com/google/eddystone/blob/master/eddystone-tlm/tlm-plain.md

   To read data advertised by this beacon, use the BLE_Teensy_Beacon_Scanner
   example (or BLE_Beacon_Scanner on an ESP32/nRF52).

   This is adapted from the original ESP32 BLE_EddystoneTLM_Beacon example,
   which advertises, then deep-sleeps the ESP32 between beacons using its
   RTC memory/esp_deep_sleep APIs. Teensy has neither of those, so this
   version just re-randomizes and re-advertises the beacon data on a timer
   instead of sleeping -- nimble_port_teensy_pump() must be called every
   loop() iteration for the BLE host to run (no RTOS task does it for you
   on Teensy), which a real deep sleep would preclude anyway.
*/

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEEddystoneTLM.h>

extern "C" void nimble_port_teensy_pump(void);

#define BEACON_UPDATE_PERIOD_MS 10000 // re-beacon every 10s (was: advertise 10s then deep sleep 10s)
#define BEACON_POWER            3     // 3dbm

static uint32_t          bootcount = 0; // no RTC memory on Teensy -- resets on every reboot
NimBLEAdvertising*        pAdvertising;

// Check
// https://github.com/google/eddystone/blob/master/eddystone-tlm/tlm-plain.md
// and http://www.hugi.scene.org/online/coding/hugi%2015%20-%20cmtadfix.htm
// for the temperature value. It is a 8.8 fixed-point notation
void setBeacon() {
    NimBLEEddystoneTLM eddystoneTLM;
    eddystoneTLM.setVolt((uint16_t)random(2800, 3700)); // 3300mV = 3.3V
    eddystoneTLM.setTemp(random(-3000, 3000));          // 3000 = 30.00 C
    Serial.printf("Random Battery voltage is %d mV = 0x%04X\n", eddystoneTLM.getVolt(), eddystoneTLM.getVolt());
    Serial.printf("Random Temperature is: %d.%d 0x%04X\n",
                  eddystoneTLM.getTemp() / 256,
                  eddystoneTLM.getTemp() % 256 * 100 / 256);

    NimBLEAdvertisementData        oAdvertisementData = BLEAdvertisementData();
    NimBLEAdvertisementData        oScanResponseData  = BLEAdvertisementData();
    NimBLEEddystoneTLM::BeaconData beaconData         = eddystoneTLM.getData();
    oScanResponseData.setServiceData(NimBLEUUID("FEAA"),
                                     reinterpret_cast<const uint8_t*>(&beaconData),
                                     sizeof(NimBLEEddystoneTLM::BeaconData));

    oAdvertisementData.setName("Teensy TLM Beacon");
    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->setScanResponseData(oScanResponseData);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Starting Teensy TLM Beacon. Bootcount = %lu\n", bootcount++);

    NimBLEDevice::init("TLMBeacon");
    NimBLEDevice::setPower(BEACON_POWER);

    pAdvertising = NimBLEDevice::getAdvertising();
    setBeacon();
    pAdvertising->start();
    Serial.println("Advertising started");
}

void loop() {
    nimble_port_teensy_pump();

    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > BEACON_UPDATE_PERIOD_MS) {
        lastUpdate = millis();
        pAdvertising->stop();
        setBeacon();
        pAdvertising->start();
    }
}

#pragma once
#include <Arduino.h>
#include <BluetoothSerial.h>

class ELM327Client {
public:
    ELM327Client();

    // Connect to adapter by MAC address. Returns true on success.
    bool connect(const char* mac);

    // Returns true if BT SPP link is up.
    bool isConnected();

    // Send AT init sequence (ATZ, ATE0, ATL0, ATS0, ATSP0).
    // Call once after connect().
    void init();

    // Query Mode 01 PID. Fills A and B bytes. Returns true on valid response.
    bool queryPID(uint8_t pid, uint8_t& A, uint8_t& B);

    // Return protocol string from ATDP.
    String getProtocol();

private:
    BluetoothSerial _bt;

    // Send a raw command string and wait for the '>' prompt.
    // Returns the response (excluding the prompt).
    String sendCommand(const String& cmd, uint32_t timeoutMs = 2000);

    // Parse a hex response line like "41 0C 1A F8" into byte array.
    // Returns number of data bytes parsed.
    int parseResponse(const String& response, uint8_t* buf, size_t bufLen);
};

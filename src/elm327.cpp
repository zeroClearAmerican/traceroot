#include "elm327.h"
#include <Arduino.h>

ELM327Client::ELM327Client() {}

bool ELM327Client::connect(const char* mac) {
    _bt.begin("TraceRoot", true);   // true = master mode
    Serial.printf("[ELM327] Connecting to %s...\n", mac);

    // Convert "XX:XX:XX:XX:XX:XX" to uint8_t array
    uint8_t addr[6];
    sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);

    bool ok = _bt.connect(addr);
    if (ok) {
        Serial.println("[ELM327] BT connected.");
    } else {
        Serial.println("[ELM327] BT connection failed.");
    }
    return ok;
}

bool ELM327Client::isConnected() {
    return _bt.connected();
}

void ELM327Client::init() {
    Serial.println("[ELM327] Initializing...");
    sendCommand("ATZ",   2500);  // reset — can be slow
    sendCommand("ATE0");         // echo off
    sendCommand("ATL0");         // linefeeds off
    sendCommand("ATS0");         // spaces off
    sendCommand("ATSP0");        // auto protocol
    Serial.println("[ELM327] Init complete.");
}

String ELM327Client::getProtocol() {
    String resp = sendCommand("ATDPN");
    resp.trim();
    return resp;
}

bool ELM327Client::queryPID(uint8_t pid, uint8_t& A, uint8_t& B) {
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "01%02X", pid);
    String resp = sendCommand(String(cmd), 1500);

    uint8_t buf[8];
    int n = parseResponse(resp, buf, sizeof(buf));
    // Response format: 41 <PID> <A> [<B> ...]
    if (n >= 3 && buf[0] == 0x41 && buf[1] == pid) {
        A = buf[2];
        B = (n >= 4) ? buf[3] : 0;
        return true;
    }
    return false;
}

// ─── Private helpers ──────────────────────────────────────────────────────────

String ELM327Client::sendCommand(const String& cmd, uint32_t timeoutMs) {
    // Flush input
    while (_bt.available()) _bt.read();

    _bt.print(cmd);
    _bt.print('\r');

    String result;
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        while (_bt.available()) {
            char c = (char)_bt.read();
            if (c == '>') {
                result.trim();
                return result;
            }
            result += c;
        }
        delay(1);
    }
    Serial.printf("[ELM327] Timeout on cmd: %s\n", cmd.c_str());
    return result;
}

int ELM327Client::parseResponse(const String& response, uint8_t* buf, size_t bufLen) {
    // Find the last non-empty line (skip echo, status lines)
    String line;
    int lastNewline = response.lastIndexOf('\n');
    if (lastNewline >= 0) {
        line = response.substring(lastNewline + 1);
    } else {
        line = response;
    }
    line.trim();

    // Parse pairs of hex digits (spaces already off via ATS0, but be safe)
    int count = 0;
    for (int i = 0; i + 1 < (int)line.length() && (size_t)count < bufLen; ) {
        char c = line[i];
        if (c == ' ') { i++; continue; }
        char hexStr[3] = { line[i], line[i+1], '\0' };
        buf[count++] = (uint8_t)strtoul(hexStr, nullptr, 16);
        i += 2;
    }
    return count;
}

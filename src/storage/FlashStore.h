#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class FlashStore {
public:
    bool begin();
    void end();

    bool writeAtomic(const char* path, const uint8_t* data, size_t len);
    bool readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead);

    bool writeString(const char* path, const String& data);
    String readString(const char* path);

    bool ensureDir(const char* path);
    bool exists(const char* path);
    bool remove(const char* path);

    bool format();

    bool isReady() const { return _ready; }

private:
    bool _ready = false;
};

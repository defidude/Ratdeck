#include "Trackball.h"

volatile int8_t Trackball::_deltaX = 0;
volatile int8_t Trackball::_deltaY = 0;
volatile bool Trackball::_clickFlag = false;
Trackball* Trackball::_instance = nullptr;

bool Trackball::begin() {
    _instance = this;

    // Configure trackball GPIOs as inputs with pullup
    pinMode(TBALL_UP, INPUT_PULLUP);
    pinMode(TBALL_DOWN, INPUT_PULLUP);
    pinMode(TBALL_LEFT, INPUT_PULLUP);
    pinMode(TBALL_RIGHT, INPUT_PULLUP);
    pinMode(TBALL_CLICK, INPUT_PULLUP);

    // Attach interrupts for movement detection
    attachInterrupt(digitalPinToInterrupt(TBALL_UP), isrUp, FALLING);
    attachInterrupt(digitalPinToInterrupt(TBALL_DOWN), isrRight, FALLING);   // Physical down pin = rightward
    attachInterrupt(digitalPinToInterrupt(TBALL_LEFT), isrLeft, FALLING);
    attachInterrupt(digitalPinToInterrupt(TBALL_RIGHT), isrDown, FALLING);   // Physical right pin = downward
    attachInterrupt(digitalPinToInterrupt(TBALL_CLICK), isrClick, FALLING);

    Serial.println("[TRACKBALL] Initialized");
    return true;
}

void Trackball::update() {
    noInterrupts();
    int8_t dx = _deltaX;
    int8_t dy = _deltaY;
    bool click = _clickFlag;
    _deltaX = 0;
    _deltaY = 0;
    _clickFlag = false;
    interrupts();

    _lastDX = dx;
    _lastDY = dy;

    _cursorX += dx * _speed;
    _cursorY += dy * _speed;

    if (_cursorX < 0) _cursorX = 0;
    if (_cursorX >= TFT_WIDTH) _cursorX = TFT_WIDTH - 1;
    if (_cursorY < 0) _cursorY = 0;
    if (_cursorY >= TFT_HEIGHT) _cursorY = TFT_HEIGHT - 1;

    _clicked = click;
    _hadMovement = (dx != 0 || dy != 0);
}

void IRAM_ATTR Trackball::isrUp()    { _deltaY--; }
void IRAM_ATTR Trackball::isrDown()  { _deltaY++; }
void IRAM_ATTR Trackball::isrLeft()  { _deltaX--; }
void IRAM_ATTR Trackball::isrRight() { _deltaX++; }
void IRAM_ATTR Trackball::isrClick() { _clickFlag = true; }

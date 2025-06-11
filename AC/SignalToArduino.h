#pragma once

#include <SerialClass.h>

void SignalToArduino(Serial& arduinoSerial, const char* Command);
void SignalToArduino(Serial& arduinoSerial, int pwmValue);
void SignalToArduino(Serial& arduinoSerial, int pwmValue, int delay);
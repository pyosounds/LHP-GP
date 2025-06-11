#include "cube.h"
#include "SignalToArduino.h"
#include <assert.h>

void SignalToArduino(Serial& arduinoSerial, const char* Command)
{
    if (arduinoSerial.IsConnected()) 
    {
        arduinoSerial.WriteData(Command, strlen(Command));
    }
    else 
    {
        assert(nullptr);
    }
}

void SignalToArduino(Serial& arduinoSerial, int pwmValue)
{
    if (arduinoSerial.IsConnected())
    {
        // PWM 값을 문자열로 변환
        char Command[30];
        snprintf(Command, sizeof(Command), "%d\n", pwmValue);

        // 아두이노에 명령어 전송
        arduinoSerial.WriteData(Command, strlen(Command));
    }
    else
    {
        assert(nullptr);  // 연결되지 않으면 오류
    }
}

void SignalToArduino(Serial& arduinoSerial, int pwmValue, int delay)
{
    if (arduinoSerial.IsConnected())
    {
        // PWM 값을 문자열로 변환
        char Command[30];
        snprintf(Command, sizeof(Command), "2 1 %d %d 1 0 %d\n", pwmValue, delay, delay);
          
        // 아두이노에 명령어 전송
        arduinoSerial.WriteData(Command, strlen(Command));
    }
    else
    {
        assert(nullptr);  // 연결되지 않으면 오류
    }
}
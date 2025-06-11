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
        // PWM ���� ���ڿ��� ��ȯ
        char Command[30];
        snprintf(Command, sizeof(Command), "%d\n", pwmValue);

        // �Ƶ��̳뿡 ��ɾ� ����
        arduinoSerial.WriteData(Command, strlen(Command));
    }
    else
    {
        assert(nullptr);  // ������� ������ ����
    }
}

void SignalToArduino(Serial& arduinoSerial, int pwmValue, int delay)
{
    if (arduinoSerial.IsConnected())
    {
        // PWM ���� ���ڿ��� ��ȯ
        char Command[30];
        snprintf(Command, sizeof(Command), "2 1 %d %d 1 0 %d\n", pwmValue, delay, delay);
          
        // �Ƶ��̳뿡 ��ɾ� ����
        arduinoSerial.WriteData(Command, strlen(Command));
    }
    else
    {
        assert(nullptr);  // ������� ������ ����
    }
}
#include <iostream>
#include <Windows.h>
#include <mmsystem.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>

using std::cout;
using std::cerr;

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#pragma comment(lib, "winmm.lib")

// --- �⺻ ���� ---
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 256
#define SHOT_SIGNAL "S"
const double VOLUME_THRESHOLD = 0.13;
const auto SHOT_COOLDOWN = std::chrono::milliseconds(120);
const auto DETECTION_GRACE_PERIOD = std::chrono::milliseconds(100);
const auto SINGLE_SHOT_MAX_DURATION = std::chrono::milliseconds(180);

// --- ���� ���� ���� ---
HANDLE g_hSolenoidArduino = INVALID_HANDLE_VALUE;  // �ַ����̵� �Ƶ��̳�
HANDLE g_hVibrationArduino = INVALID_HANDLE_VALUE; // ���� ���� �Ƶ��̳�

HWAVEIN g_hWaveIn = NULL;
HHOOK g_hMouseHook = NULL;
std::atomic<bool> g_detection_active = false;
std::chrono::steady_clock::time_point g_last_signal_time;
std::chrono::steady_clock::time_point g_press_time;
std::thread g_grace_period_timer;

// --- �Լ� ���� ---
void SignalToArduino(const char* signal);
float CalculateRms(const float* data, int frame_count);
void CALLBACK AudioCallback(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
void cleanup();

int main()
{
    cout << "�ѱ� �߻� ���� ���α׷� ���� (2-Arduino �ý���)\n";

    // 1. �ַ����̵� �Ƶ��̳� ����
    g_hSolenoidArduino = CreateFileW(L"\\\\.\\COM3", GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (g_hSolenoidArduino == INVALID_HANDLE_VALUE)
    {
        cerr << "�ַ����̵� �Ƶ��̳� ���� ���� (COM3)\n";
    }
    else
    {
        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        GetCommState(g_hSolenoidArduino, &dcbSerialParams);
        dcbSerialParams.BaudRate = CBR_115200;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        SetCommState(g_hSolenoidArduino, &dcbSerialParams);
        cout << "�ַ����̵� �Ƶ��̳� ���� ����\n";
    }

    // 2. ���� ���� �Ƶ��̳� ����
    g_hVibrationArduino = CreateFileW(L"\\\\.\\COM5", GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (g_hVibrationArduino == INVALID_HANDLE_VALUE)
    {
        cerr << "���� ���� �Ƶ��̳� ���� ����\n";
    }
    else
    {
        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        GetCommState(g_hVibrationArduino, &dcbSerialParams);
        dcbSerialParams.BaudRate = CBR_115200;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        SetCommState(g_hVibrationArduino, &dcbSerialParams);
        cout << "���� ���� �Ƶ��̳� ���� ����\n";
    }

    // ����� �� ���콺 �� ����
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    WAVEHDR waveHeader = {};
    char waveBuffer[BLOCK_SIZE * sizeof(float)];
    waveHeader.lpData = waveBuffer;
    waveHeader.dwBufferLength = sizeof(waveBuffer);

    if (waveInOpen(&g_hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)AudioCallback, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
    {
        waveInPrepareHeader(g_hWaveIn, &waveHeader, sizeof(WAVEHDR));
        waveInAddBuffer(g_hWaveIn, &waveHeader, sizeof(WAVEHDR));
        waveInStart(g_hWaveIn);
        cout << "�ǽð� ���� �м� ����\n";
    }
    else
    {
        cerr << "����ũ�� �� �� �����ϴ�\n";
    }

    g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    if (!g_hMouseHook)
    {
        cerr << "���콺 �� ��ġ ����\n";
        cleanup();
        return 1;
    }

    cout << "���콺 ������ ���� (�ܼ� â�� ������ ����)\n";

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    cleanup();

    return 0;
}

void cleanup()
{
    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);
    if (g_hWaveIn)
    {
        waveInStop(g_hWaveIn);
        waveInClose(g_hWaveIn);
    }
    
    if (g_hSolenoidArduino != INVALID_HANDLE_VALUE) CloseHandle(g_hSolenoidArduino);
    if (g_hVibrationArduino != INVALID_HANDLE_VALUE) CloseHandle(g_hVibrationArduino);

    if (g_grace_period_timer.joinable()) g_grace_period_timer.join();

    cout << "���α׷��� �����մϴ�.\n";
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_LBUTTONDOWN)
        {
            if (g_grace_period_timer.joinable()) g_grace_period_timer.detach();
            g_press_time = std::chrono::steady_clock::now();
            cout << "-------------------------------------\n";
            cout << "���콺 ���� -> �Ҹ� ���� ON\n";
            g_detection_active = true;
        }
        else if (wParam == WM_LBUTTONUP)
        {
            auto release_time = std::chrono::steady_clock::now();
            auto click_duration = release_time - g_press_time;

            if (click_duration <= SINGLE_SHOT_MAX_DURATION)
            {
                cout << "�ܹ� ��� ���� -> ���� �ð�(" << DETECTION_GRACE_PERIOD.count() << "ms) ����\n";
                if (g_grace_period_timer.joinable()) g_grace_period_timer.join();
                g_grace_period_timer = std::thread([]()
                    {
                        std::this_thread::sleep_for(DETECTION_GRACE_PERIOD);
                        g_detection_active = false;
                        cout << "���� �ð� ���� -> �Ҹ� ���� OFF\n";
                    });
            }
            else
            {
                cout << "���� ��� ���� -> �Ҹ� ���� OFF\n";
                g_detection_active = false;
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

float CalculateRms(const float* data, int frame_count)
{
    if (frame_count == 0) return 0.0f;
    double sum_squares = 0.0;
    for (int i = 0; i < frame_count; ++i) sum_squares += data[i] * data[i];
    return static_cast<float>(sqrt(sum_squares / frame_count));
}


void CALLBACK AudioCallback(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg != WIM_DATA || !g_detection_active)
    {
        if (g_hWaveIn && uMsg == WIM_DATA) waveInAddBuffer(hWaveIn, (WAVEHDR*)dwParam1, sizeof(WAVEHDR));
        return;
    }

    WAVEHDR* pWaveHdr = (WAVEHDR*)dwParam1;
    if (pWaveHdr->dwBytesRecorded > 0)
    {
        float* audio_data = (float*)pWaveHdr->lpData;
        int frame_count = pWaveHdr->dwBytesRecorded / sizeof(float);
        float rms_volume = CalculateRms(audio_data, frame_count);

        if (rms_volume > VOLUME_THRESHOLD)
        {
            auto current_time = std::chrono::steady_clock::now();
            if (current_time - g_last_signal_time > SHOT_COOLDOWN)
            {
                g_last_signal_time = current_time;
                printf("�ѱ� �߻� ����! (����: %.4f) -> �Ƶ��̳� ��ȣ ����\n", rms_volume);
                
                SignalToArduino(SHOT_SIGNAL);
            }
        }
    }

    if (g_hWaveIn)
    {
        waveInAddBuffer(hWaveIn, pWaveHdr, sizeof(WAVEHDR));
    }
}

// �� �Ƶ��̳뿡 ��ȣ�� ������ �Լ�
void SignalToArduino(const char* signal)
{
    DWORD bytes_written = 0;
    
    // �ַ����̵� �Ƶ��̳뿡 ��ȣ ����
    if (g_hSolenoidArduino != INVALID_HANDLE_VALUE)
    {
        WriteFile(g_hSolenoidArduino, signal, 1, &bytes_written, NULL);
    }

    // ���� ���� �Ƶ��̳뿡 ��ȣ ����
    if (g_hVibrationArduino != INVALID_HANDLE_VALUE)
    {
        WriteFile(g_hVibrationArduino, signal, 1, &bytes_written, NULL);
    }
}
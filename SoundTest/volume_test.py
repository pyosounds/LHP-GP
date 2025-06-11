import sounddevice as sd
import numpy as np
import serial
from pynput import mouse
import time
import threading

# --- 기본 설정 ---
SERIAL_PORT = 'COM3'
BAUD_RATE = 9600
INPUT_DEVICE_INDEX = None # 사용할 마이크 장치 번호

# 사운드 장치 설정
SAMPLE_RATE = 44100
BLOCK_SIZE = 1024
CHANNELS = 1

# --- 핵심 설정값 ---
VOLUME_THRESHOLD = 0.1
SHOT_SIGNAL = b'S'
SHOT_COOLDOWN_SECONDS = 0.12
DETECTION_GRACE_PERIOD_SECONDS = 0.17


# ===========단발/연발 구분을 위한 새로운 설정값=============
# 이 시간보다 짧게 클릭하면 단발로 간주하여 유예 시간을 적용
SINGLE_SHOT_MAX_DURATION_SECONDS = 0.18

# 이 시간 이상 누르고 있으면 연발 모드
FULL_AUTO_THRESHOLD_SECONDS = 0.22
# =========================================================


# --- 상태 관리 변수 ---
detection_event = threading.Event()
arduino = None
last_signal_time = 0.0
grace_period_timer = None
press_time = 0.0  # 마우스 버튼 누른 시간 기록


def signal_to_arduino(arduino_serial, signal):
    if arduino_serial and arduino_serial.is_open:
        try:
            arduino_serial.write(signal)
        except serial.SerialException as e:
            print(f"Error writing to serial port: {e}")
    else:
        print("Arduino not connected or port is closed.")

def audio_callback(indata, frames, time_info, status):
    global last_signal_time
    if status:
        print(f"Audio stream error: {status}")
        return

    if not detection_event.is_set():
        return

    rms_volume = np.sqrt(np.mean(indata**2))

    if rms_volume > VOLUME_THRESHOLD:
        current_time = time.time()
        if current_time - last_signal_time > SHOT_COOLDOWN_SECONDS:
            last_signal_time = current_time
            print(f"총기 발사 감지! (볼륨: {rms_volume:.4f}) -> 아두이노 신호 전송")
            signal_to_arduino(arduino, SHOT_SIGNAL)

def on_click(x, y, button, pressed):
    global grace_period_timer, press_time # 전역 변수 추가
    
    if button == mouse.Button.left:
        if pressed:
            # 이전에 실행 중이던 유예 시간 타이머가 있다면 취소
            if grace_period_timer and grace_period_timer.is_alive():
                grace_period_timer.cancel()
            
            press_time = time.time() # 마우스 누른 시간 기록
            print("-------------------------------------")
            print("마우스 눌림 - 소리 감지 모드 ON")
            detection_event.set()
        else:
            # 마우스를 뗄 때, 클릭 지속 시간 계산
            if press_time == 0: return # 눌림 이벤트 없이 떼기만 한 경우 방지

            release_time = time.time()
            click_duration = release_time - press_time

            # 클릭 시간에 따라 단발/연발 로직 분기
            if click_duration <= SINGLE_SHOT_MAX_DURATION_SECONDS:
                # 단발 처리: 유예 시간을 적용
                print(f"단발 사격 감지 (클릭 시간: {click_duration:.2f}초) - 유예 시간 후 감지 종료")
                grace_period_timer = threading.Timer(DETECTION_GRACE_PERIOD_SECONDS, detection_event.clear)
                grace_period_timer.start()
            else:
                # 연발 처리: 유예 시간 없이 즉시 감지 종료
                print(f"연발 사격 감지 (클릭 시간: {click_duration:.2f}초) - 즉시 감지 종료")
                detection_event.clear()
            
            press_time = 0

# --- 메인 실행 ---
if __name__ == "__main__":
    try:
        arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"아두이노 연결 성공: {SERIAL_PORT} @ {BAUD_RATE}")
        time.sleep(2)
    except serial.SerialException as e:
        print(f"아두이노 연결 실패: {e}")
        arduino = None

    print("\nFPS 총기 발사 감지 프로그램을 시작")
    print(f"총성 판단 볼륨 임계값: {VOLUME_THRESHOLD}")
    print(f"신호 재사용 대기시간: {SHOT_COOLDOWN_SECONDS}초")
    print(f"단발 최대 클릭 시간: {SINGLE_SHOT_MAX_DURATION_SECONDS}초")
    print(f"단발 클릭 유예 시간: {DETECTION_GRACE_PERIOD_SECONDS}초")

    mouse_listener = mouse.Listener(on_click=on_click)
    mouse_listener.start()

    try:
        with sd.InputStream(device=INPUT_DEVICE_INDEX, samplerate=SAMPLE_RATE,
                            blocksize=BLOCK_SIZE, channels=CHANNELS,
                            dtype='float32', callback=audio_callback):
            print("\n실시간 사운드 분석 시작. (Ctrl+C 로 종료)")
            mouse_listener.join()
    except KeyboardInterrupt:
        print("\n프로그램을 종료")
    except Exception as e:
        print(f"오류 발생: {e}")
    finally:
        if mouse_listener.is_alive():
            mouse_listener.stop()
        if arduino and arduino.is_open:
            arduino.close()
            print("아두이노 연결 종료")
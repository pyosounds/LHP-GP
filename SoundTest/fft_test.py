import sounddevice as sd
import numpy as np
from scipy.fft import fft
import serial
from pynput import mouse, keyboard
import time
import threading


# --- 사운드 설정값 ---
AK48_FREQUENCY_RANGE = (70, 90)
AK48_THRESHOLD = (0.025, 0.06)
TRG_FREQUENCY_RANGE = (45, 55)
TRG_THRESHOLD = (0.078, 0.13)

SAMPLE_RATE = 44100
BLOCK_SIZE = 2048
CHANNELS = 1
TARGET_FREQUENCY_RANGE = (50, 100)
TARGET_FREQUENCY_RANGE_TRG = (45, 55)
TARGET_FREQUENCY_RANGE_GUN = (70, 90)
TARGET_FREQUENCY_RANGE_AK = (45, 55)
THRESHOLD = 0.08
THRESHOLD_TRG = (0.078, 0.1)
THRESHOLD_GUN = (0.05, 0.1)
THRESHOLD_AK = (0.5, 0.1)
SERIAL_PORT = 'COM3'
BAUD_RATE = 9600
SIGNAL_ON = b'1'
SIGNAL_OFF = b'0'

GUN_TYPE = 'AK47'


# --- 설정값 ---
SINGLE_SHOT_MAX_DURATION_SECONDS = 0.18     # 이 시간 안에 떼면 단발로 간주 (조정 필요)
FULL_AUTO_THRESHOLD_SECONDS = 0.22          # 이 시간 이상 누르고 있으면 연발 모드 고려 시작 (SINGLE_SHOT_MAX_DURATION보다 커야 함)
FULL_AUTO_SIGNAL_INTERVAL_SECONDS = 0.1     # 연발 시 신호 전송 간격 (0.1초마다)
TOGGLE_KEY_CHAR = '`'                       # 마우스 감지 모드 ON/OFF 토글 키

# --- 상태 변수 ---
left_button_down = False
press_time = 0
is_in_full_auto_mode = False    # 현재 연발 모드인지 여부

full_auto_check_timer = None    # 연발 모드 진입 여부 확인 타이머
periodic_signal_timer = None    # 주기적 연발 신호 타이머

mouse_detection_active = False  # 마우스 감지 모드 상태



# 아두이노 신호 전송 함수
def signal_to_arduino(signal_type, arduino_serial, pwm_val, delay_val):
    if arduino_serial.is_open:
        # PWM 값을 문자열로 변환하여 명령어 생성
        command = f"2 1 {pwm_val} {delay_val} 1 255 {delay_val}\n".encode('utf-8')
        
        if signal_type == "single":
            print("[아두이노 신호] 단발")
            arduino_serial.write(command)
            #time.sleep(0.1)        
        elif signal_type == "auto_start":
            print("[아두이노 신호] 연발 시작/지속")
            arduino_serial.write(command)
            #time.sleep(0.1)        
        elif signal_type == "auto_stop":
            print("[아두이노 신호] 연발 중지")
            #time.sleep(0.1)
        
        arduino_serial.flush()

    else:
        raise AssertionError("아두이노 연결 실패")



# 주기적으로 연발 신호를 보내는 함수
def _send_periodic_full_auto_signal():
    global periodic_signal_timer, left_button_down, is_in_full_auto_mode
    # mouse_detection_active 조건 추가
    if left_button_down and is_in_full_auto_mode and mouse_detection_active:
        signal_to_arduino("auto_start", arduino, 0, 10)
        periodic_signal_timer = threading.Timer(FULL_AUTO_SIGNAL_INTERVAL_SECONDS, _send_periodic_full_auto_signal)
        periodic_signal_timer.start()
    else:
        if is_in_full_auto_mode:
            _stop_full_auto_firing_logic()

# 연발 모드 진입 여부를 확인하고, 맞다면 연발 시작
def _check_and_start_full_auto():
    global is_in_full_auto_mode, left_button_down
    # mouse_detection_active 조건 추가
    if left_button_down and mouse_detection_active:
        print("연발 모드 시작.")
        is_in_full_auto_mode = True
        _send_periodic_full_auto_signal()

# 연발 발사 관련 로직 및 타이머를 중지하는 함수
def _stop_full_auto_firing_logic():
    global is_in_full_auto_mode, full_auto_check_timer, periodic_signal_timer
    
    if is_in_full_auto_mode:
        print("연발 종료")

    is_in_full_auto_mode = False
    if full_auto_check_timer and full_auto_check_timer.is_alive():
        full_auto_check_timer.cancel()
    if periodic_signal_timer and periodic_signal_timer.is_alive():
        periodic_signal_timer.cancel()



# --- 마우스 클릭 콜백 함수 ---
def on_click(x, y, button, pressed):
    global press_time, left_button_down
    global full_auto_check_timer, is_in_full_auto_mode

    # --- 마우스 감지 모드 확인 ---
    if not mouse_detection_active:
        return # 모드가 꺼져있으면 아무것도 하지 않음
    # --- 마우스 감지 모드 확인 끝 ---

    if button == mouse.Button.left:
        if pressed:
            #print("왼쪽 버튼 눌림")
            _stop_full_auto_firing_logic() # 이전 상태 정리

            left_button_down = True
            press_time = time.time()
            
            full_auto_check_timer = threading.Timer(FULL_AUTO_THRESHOLD_SECONDS, _check_and_start_full_auto)
            full_auto_check_timer.start()
        else: # 버튼을 뗐을 때
            if not left_button_down: # 이미 처리된 릴리즈 이벤트면 무시
                return
                
            #print("왼쪽 버튼 떼어짐")
            release_time = time.time()
            # press_time이 0이 아닌 경우에만 click_duration 계산
            click_duration = release_time - press_time if press_time > 0 else 0 
            
            was_in_full_auto_before_release = is_in_full_auto_mode
            _stop_full_auto_firing_logic() # 연발 관련 로직 및 타이머 중지/정리
            
            left_button_down = False # 버튼 상태 업데이트

            if not was_in_full_auto_before_release: # 연발 모드로 진입하기 전이었다면
                if click_duration > 0 and click_duration < SINGLE_SHOT_MAX_DURATION_SECONDS : # 유효한 클릭 시간에만
                    print("단발 판정 ")
                    signal_to_arduino("single", arduino, 0, 100)
                    #signal_to_arduino("single", arduino, 0, 100) # 단발 신호 전송
                elif click_duration > 0 : # 단발 기준은 넘었지만 연발은 아니었던 경우
                    print("클릭 시간 애매함 (단발 기준 초과, 연발 기준 미만)")
            # else: 연발 모드 중이었다면 _stop_full_auto_firing_logic()가 이미 모든 정리 수행



# --- 키보드 입력 콜백 함수 ---
def on_key_press(key):
    global mouse_detection_active, left_button_down # left_button_down 상태도 초기화 필요
    
    try:
        if key.char == TOGGLE_KEY_CHAR:
            mouse_detection_active = not mouse_detection_active
            if mouse_detection_active:
                print("마우스 감지 모드 ON")
            else:
                print("마우스 감지 모드 OFF")
                # 감지 모드 OFF 시, 진행 중이던 마우스 액션 관련 상태 강제 종료 및 초기화
                _stop_full_auto_firing_logic()
                left_button_down = False
    except AttributeError:
        pass



def audio_callback(indata, frames, time, status):
    if status:
        #print(f"오디오 스트림 오류: {status}")
        return
    if any(indata):
        audio_data = indata[:, 0].astype(np.float32)  # 모노 채널, float32 변환
        window = np.hanning(BLOCK_SIZE)
        windowed_data = audio_data[:BLOCK_SIZE] * window
        fft_result = fft(windowed_data)
        magnitude_spectrum = np.abs(fft_result[:BLOCK_SIZE//2]) * 2 / BLOCK_SIZE
        frequencies = np.fft.fftfreq(BLOCK_SIZE, 1/SAMPLE_RATE)[:BLOCK_SIZE//2]


        ak48_start_bin = np.argmin(np.abs(frequencies - AK48_FREQUENCY_RANGE[0]))
        ak48_end_bin = np.argmin(np.abs(frequencies - AK48_FREQUENCY_RANGE[1]))
        ak48_energy = np.sum(magnitude_spectrum[ak48_start_bin:ak48_end_bin])

        trg_start_bin = np.argmin(np.abs(frequencies - TRG_FREQUENCY_RANGE[0]))
        trg_end_bin = np.argmin(np.abs(frequencies - TRG_FREQUENCY_RANGE[1]))
        trg_energy = np.sum(magnitude_spectrum[trg_start_bin:trg_end_bin])

        if arduino:
            if trg_energy > TRG_THRESHOLD[0] and trg_energy < TRG_THRESHOLD[1]:
                #print("TRG 감지 (에너지: {trg_energy:.4f}) - 아두이노에 신호 전송")
                #arduino.write(SIGNAL_TRG)

                #print("TRG 감지 - 아두이노에 신호 전송")
                #signal_to_arduino(arduino, 0, 100)
                GUN_TYPE = "TRG"

            
            elif ak48_energy > AK48_THRESHOLD[0] and ak48_energy < AK48_THRESHOLD[1]:
                #print("AK48 감지 (에너지: {ak48_energy:.4f}) - 아두이노에 신호 전송")
                #arduino.write(SIGNAL_AK48)

                #print("AK47 감지 - 아두이노에 신호 전송")
                #signal_to_arduino(arduino, 0, 100)
                GUN_TYPE = "AK47"
            
            
            
            #else:
                #arduino.write(SIGNAL_OFF)
                #signal_to_arduino(arduino, 255, 0)



# --- 메인 실행 ---
if __name__ == "__main__":
    try:
        arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"아두이노 연결 성공: {SERIAL_PORT} @ {BAUD_RATE}")
    except serial.SerialException as e:
        print(f"아두이노 연결 실패: {e}")
        arduino = None


    print(f"마우스 감지 모드 ON/OFF: '{TOGGLE_KEY_CHAR}'")
    print(f"초기 마우스 감지 모드 {'ON' if mouse_detection_active else 'OFF'}")

    # 리스너 생성
    mouse_listener = mouse.Listener(on_click=on_click)
    keyboard_listener = keyboard.Listener(on_press=on_key_press) # on_press 사용

    # 리스너 시작
    mouse_listener.start()
    keyboard_listener.start()


    try:
        with sd.InputStream(samplerate=SAMPLE_RATE, blocksize=BLOCK_SIZE,
                        channels=CHANNELS, dtype='float32', callback=audio_callback):
            print("실시간 사운드 분석 시작")
            while True:
                #with mouse.Listener(on_click=on_click) as listener:
                #    listener.join()
                time.sleep(0.05)
            

    except sd.PortAudioError as e:
        print(f"PortAudio 오류: {e}")
    except KeyboardInterrupt:
        print("\n실시간 사운드 분석 종료.")
    finally:
        if arduino and arduino.is_open:
            arduino.close()
            print("아두이노 연결 종료.")


    try:
        # 메인 스레드가 종료되지 않도록 리스너 스레드들이 끝날 때까지 대기
        # 또는 다른 방식으로 메인 스레드 유지 (예: while True: time.sleep(1))
        # 각 리스너의 join을 순차적으로 호출                
        while mouse_listener.is_alive() or keyboard_listener.is_alive(): # 두 리스너 중 하나라도 살아있으면 유지
            time.sleep(0.05)

    except KeyboardInterrupt:
        current_time_str = time.strftime('%H:%M:%S.%f')[:-3]
        
    finally:
        current_time_str = time.strftime('%H:%M:%S.%f')[:-3]
        
        _stop_full_auto_firing_logic()

        if mouse_listener.is_alive():
            mouse_listener.stop()
            # mouse_listener.join()
        if keyboard_listener.is_alive():
            keyboard_listener.stop()
            # keyboard_listener.join()
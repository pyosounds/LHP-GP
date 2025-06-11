const int solenoidPin = 3;

// 솔레노이드 동작 정의
const int ACTION_PWM_VALUE = 0;
const int ACTION_DURATION_MS = 100;
const int REST_PWM_VALUE = 255;

// 상태 관리 변수
int currentState = 0; // 0: 대기 상태, 1: 동작 상태
unsigned long actionStartTime = 0; // 동작 시작 시간을 기록할 변수

void setup() 
{
  pinMode(solenoidPin, OUTPUT);
  analogWrite(solenoidPin, REST_PWM_VALUE);
  Serial.begin(115200);
}

void loop() 
{
  if (Serial.available() > 0) 
  {
    char command = Serial.read();

    // 'S' 신호를 받고, 현재 대기 상태일 때만 동작 시작
    if (command == 'S' && currentState == 0) 
    {
      Serial.println("Action Triggered!");
      
      currentState = 1; // 상태를 '동작'으로 변경
      actionStartTime = millis();

      analogWrite(solenoidPin, ACTION_PWM_VALUE);
    }
  }

  // 상태에 따른 솔레노이드 제어
  // 현재 '동작' 상태이고, 지정된 시간이 지났다면
  if (currentState == 1 && (millis() - actionStartTime >= ACTION_DURATION_MS)) 
  {
    currentState = 0; // 상태를 '대기'로
    analogWrite(solenoidPin, REST_PWM_VALUE); // 솔레노이드 원위치
  }
}
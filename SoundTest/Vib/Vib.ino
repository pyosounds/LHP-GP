const int motorPin1 = 10; // 첫 번째 진동 모터 핀
const int motorPin2 = 11; // 두 번째 진동 모터 핀

const int VIBRATION_DURATION_MS = 150;

// 상태 관리 변수
bool isActionActive = false;
unsigned long actionStartTime = 0;

void setup() 
{
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  Serial.begin(115200);
}

void loop() 
{
  // 1. PC로부터 'S' 신호 수신
  if (Serial.available() > 0) 
  {
    char command = Serial.read();
    if (command == 'S' && !isActionActive) 
    {
      isActionActive = true;
      actionStartTime = millis();
      digitalWrite(motorPin1, HIGH);
      digitalWrite(motorPin2, HIGH);
    }
  }

  // 2. 상태에 따른 제어
  if (isActionActive && (millis() - actionStartTime >= VIBRATION_DURATION_MS)) 
  {
    isActionActive = false;
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, LOW);
  }
}
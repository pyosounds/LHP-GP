const int solenoidPin = 3; // 솔레노이드가 연결된 핀 (3번)
const int MAX_NUMS = 100;

void setup() 
{
  pinMode(solenoidPin, OUTPUT);
  analogWrite(solenoidPin, 255);
  Serial.begin(9600);
}

void loop() 
{
  if (Serial.available() > 0) 
  {
    String input = Serial.readStringUntil('\n');
    input.trim();
    int numbers[MAX_NUMS];
    int numCount = 0;
    
    
    int index = 0;
    while (index < input.length() && numCount < MAX_NUMS) 
    {
      int spaceIndex = input.indexOf(' ', index);
      if (spaceIndex == -1) spaceIndex = input.length();

      String numStr = input.substring(index, spaceIndex);
      numbers[numCount] = numStr.toInt();
      numCount++;

      index = spaceIndex + 1;
    }

    for (int i = 0; i < numCount / 3; i++) 
    {
        analogWrite(solenoidPin, numbers[i*3+2]);
        
        delay(numbers[i*3+3]); 
    }

  }
}
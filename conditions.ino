#define RECORD_BUTTON D1
#define PLAY_BUTTON D2
#define Acs712 A0
#define buzz D12 

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

bool lastRecordState = LOW;
bool lastPlayState = LOW;
bool lastCombinedState = LOW; 

const int MAX_STEPS = 100;
int servoPositions[MAX_STEPS][5];
int arrayPointer = 0;
int currentAngles[5];   

bool isPlaying = false;
int playIndex = 0;


// = - - - - - - - - - - - - =   ---> Sensor Variables
float zeroCurrent = 2.5;
float sensitivity = 0.100;

unsigned long overCurrentStartTime =0;
// = - - - - - - - - - - - - =

void setup() {
  Serial.begin(115200);
  pinMode(RECORD_BUTTON, INPUT);   
  pinMode(PLAY_BUTTON, INPUT);   
  pinMode(Acs712, INPUT);  
  pinMode(buzz,OUTPUT);
}

// Clearing the date after push the both buttons 

void clearMemory() {
  arrayPointer = 0;
  isPlaying = false;
  playIndex = 0;
  Serial.println("Reseted .");
}


// Read the Angles From Servos in currentAngles

void storeCurrentStep() {
  if (arrayPointer < MAX_STEPS) {
    for (int i = 0; i < 5; i++) {
      servoPositions[arrayPointer][i] = currentAngles[i];
    }
    arrayPointer++;
    Serial.print("Sotred : "); Serial.println(arrayPointer);
  } else {
    Serial.println("Sorry but no memory to store");
  }
}

// For play the data stored
void startPlayback() {
  if (arrayPointer > 0) {
    isPlaying = true;
    playIndex = 0;
    Serial.println("Playing");
  } else {
    Serial.println("No stored data");
  }
}


void loop() {

  bool recordState = digitalRead(RECORD_BUTTON);
  bool playState = digitalRead(PLAY_BUTTON);

  //Clear data
  if (recordState == HIGH && playState == HIGH && lastCombinedState == LOW) {
    clearMemory();
    lastCombinedState = HIGH;
  } else if (recordState == LOW || playState == LOW) {
    lastCombinedState = LOW;  
  }
  
  // Store Date
  if (recordState == HIGH && lastRecordState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();
    storeCurrentStep();
  }
  lastRecordState = recordState;

  // Play mode
  if (playState == HIGH && lastPlayState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();
    startPlayback();
  }
  lastPlayState = playState;

  // ACS Sensor
/*
  float currentAmps = readCurrent();
  if (currentAmps > 3.0) {
    if (overCurrentStartTime == 0) overCurrentStartTime = millis();
    else if (millis() - overCurrentStartTime > 500) digitalWrite(buzz, HIGH);
  }  
  else {
    overCurrentStartTime = 0;
    digitalWrite(buzz, LOW);
  }

}



float readCurrent (){
  int raw = analogRead(Acs712);
  float voltage = (raw / 1024.0)*3.3;
  float actualVoltage = voltage / 0.66; // Because two Resistors 
  float current = (actualVoltage - zeroCurrent) / sensitivity;
  return abs(current);
}
*/
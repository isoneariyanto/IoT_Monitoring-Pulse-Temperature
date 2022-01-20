#include <ESP8266WiFi.h>
#include <Wire.h>
#include <FirebaseArduino.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define pulsePin A0 // default pulse pin for ecg8266
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
#define FIREBASE_HOST "paste here your database url"
#define FIREBASE_AUTH "Paste Here your database authentications" 
#define ssid "******" // Nama SSID AP/Hotspot
#define password "*********" //Password Wifi
WiFiClient client;
double ObjectTemp,AmbientTemp;
unsigned long timeout;

int rate[10];                    
unsigned long sampleCounter = 0; 
unsigned long lastBeatTime = 0;  
unsigned long lastTime = 0, NewTime;
int BPM = 0;
int IBI = 600;
int P = 512;
int T = 512;
int thresh = 525;  
int amp = 100;                   
int Signal;
boolean Pulse = false;
boolean firstBeat = true;          
boolean secondBeat = true;
boolean PulseDetected = false;  
int pulse;

void setup() {
  Serial.begin(9600);
  //Lcd
  lcd.init();
  lcd.backlight();
  //MLX90614 Temperature Sensor  
  mlx.begin();
  //AD8232 ECG Sensor
  pinMode(12, INPUT); // Setup for leads off detection LO +
  pinMode(14, INPUT); // Setup for leads off detection LO -
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0,0);
    lcd.print("Connecting..");
    delay(1000);
    lcd.clear();
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  lcd.setCursor(0,1);
  lcd.print("Ip : ");
  lcd.setCursor(4,1);
  lcd.print(WiFi.localIP());
  delay(1000);
  lcd.clear();
  
}

void loop() {
  upload();
}

void nilai_suhu(){
  ObjectTemp = int(mlx.readObjectTempC());
  Serial.print("\tsuhu : ");
  Serial.print(ObjectTemp);
}
void upload(){
  if (PulseDetected == true) {
    Serial.print("PASIEN 1");
    nilai_suhu();
    Serial.print("\tnadi : ");
    pulse = BPM;
    Serial.println(pulse);
    //LCD SUHU
    lcd.setCursor(0,1);
    lcd.print(">> Suhu : ");
    lcd.setCursor(10,1);
    lcd.print(int(ObjectTemp));
    lcd.setCursor(13,1);
    lcd.print("<<");
    //LCD BPM
    lcd.setCursor(0,0);
    lcd.print(">> BPM  : ");
    lcd.setCursor(10,0);
    lcd.print(pulse);
    lcd.setCursor(13,0);
    lcd.print("<<");
    delay(5000);
    Firebase.pushInt("nadi", pulse);
    Firebase.pushInt ("suhu", ObjectTemp);
    // handle error
    if (Firebase.failed()) {
        Serial.print("setting /number failed:");
        Serial.println(Firebase.error());  
        return;
    }
    PulseDetected = false;
    lcd.clear();           
  }
  else if (millis() >= (lastTime + 2)) {
     readPulse(); 
     lastTime = millis();
  }
}

void readPulse() {
  if((digitalRead(12) == 1)||(digitalRead(14) == 1)){
    Serial.println('!');
  }else{
    Signal = analogRead(pulsePin);              
    sampleCounter += 2;                           
    int NewTime = sampleCounter - lastBeatTime;   
  
    detectSetHighLow();
  
    if (NewTime > 250) {  
      if ( (Signal > thresh) && (Pulse == false) && (NewTime > (IBI / 5) * 3) )
        pulseDetected();
    }
  
    if (Signal < thresh && Pulse == true) {  
      Pulse = false;
      amp = P - T;
      thresh = amp / 2 + T;  
      P = thresh;
      T = thresh;
    }
  
    if (NewTime > 2500) {
      thresh = 525;
      P = 512;
      T = 512;
      lastBeatTime = sampleCounter;
      firstBeat = true;            
      secondBeat = true;           
    }
  }
}

void detectSetHighLow() {

  if (Signal < thresh && NewTime > (IBI / 5) * 3) {
    if (Signal < T) {                       
      T = Signal;                         
    }
  }

  if (Signal > thresh && Signal > P) {    
    P = Signal;                           
  }                                       

}

void pulseDetected() {
  Pulse = true;                           
  IBI = sampleCounter - lastBeatTime;     
  lastBeatTime = sampleCounter;           

  if (firstBeat) {                       
    firstBeat = false;                 
    return;                            
  }
  if (secondBeat) {                    
    secondBeat = false;                
    for (int i = 0; i <= 9; i++) {   
      rate[i] = IBI;
    }
  }

  word runningTotal = 0;                   

  for (int i = 0; i <= 8; i++) {          
    rate[i] = rate[i + 1];            
    runningTotal += rate[i];          
  }

  rate[9] = IBI;                      
  runningTotal += rate[9];            
  runningTotal /= 10;                 
  BPM = 60000 / runningTotal;         
  PulseDetected = true;
  delay(1);
}

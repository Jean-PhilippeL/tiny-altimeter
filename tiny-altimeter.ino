#include <SFE_BMP180.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Button.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <Dht11.h>
#include <Time.h>
#include <DS1307RTC.h>
#include "LowPower.h"

#define BUTTON_UP_PIN     3
#define BUTTON_DOWN_PIN     2

#define DHT11_PIN     8

#define CURRENT_ALTITUDE_SCREEN 1
#define MAX_ALTITUDE_SCREEN 2
#define MIN_ALTITUDE_SCREEN 3
#define PRESSION_SCREEN 4
#define TEMPERATURE_SCREEN 5
#define BATTERY_SCREEN 6
#define HUMIDITY_SCREEN 7
//#define UPTIME_SCREEN 8
#define HOUR_SCREEN 8

#define NB_SCREENS HOUR_SCREEN

#define READ_DHT11_MAX_TRY 10



Adafruit_SSD1306 display(-1);

//extern uint8_t Font24x40[];
//extern uint8_t Symbol[];
//extern uint8_t Splash[];
extern uint8_t Battery[];

SFE_BMP180 pressure;
//Button buttonUp = Button(BUTTON_UP_PIN);
//Button buttonDown = Button(BUTTON_DOWN_PIN);

boolean settingMode = false;
double QNH, savedQNH;
double temperature, pression, altitude = 0;
//double baseAltitude, saveBaseAltitude = 0;
double savedQnhAltitude =0;
double altiMin = 9999.0;
double altiMax = 0.0;
double lastValue = 0.0;
long lastVcc, vcc = 0;
double lastHumidity, humidity = 0;
int eepromAddr = 10;

#define MAX_SAMPLES 20
double samplesBuffer[MAX_SAMPLES];
int indexBfr = 0;
double averagePressure = 0;
boolean bufferReady = false;
int screen = HOUR_SCREEN; // numero d'ecran
boolean skipClic = false;
tmElements_t tm;

volatile boolean buttonWasPressed = false;

void setup()   {   
  display.begin();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay(); 
  display.println(F("init"));
  display.display();

  // init QNH
  EEPROM_readAnything(eepromAddr, QNH);
  if (isnan(QNH)) {
    QNH = 1013.25;
    EEPROM_writeAnything(eepromAddr, QNH);  // QNH standard 
     
  }
  
  savedQNH = QNH;
    display.print(F("QNH:"));
     display.println(QNH);
     display.display();

  if (pressure.begin()) {
     display.println(F("Pressure sensor OK:"));
     display.display();
    updatePressureAndTemperature();
    savedQnhAltitude = altitude;
  }else {
    display.println(F("fail"));
    display.display();
    delay(3000); 
  }

  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);

  
  enableInterrupts();
//TODO : diminuer luminosité :display.dim(true);
}

void enableInterrupts(){
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN), test, FALLING );
  attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN_PIN), test2, FALLING );
}

void disableInterrupts(){
  detachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN) );
  detachInterrupt(digitalPinToInterrupt(BUTTON_DOWN_PIN));
}

void test(){
  buttonWasPressed = true;
}

void test2(){
  buttonWasPressed = true;
}

void loop() {
 
  if(buttonWasPressed){
       disableInterrupts();
       manageButtons();
       buttonWasPressed = false;
       enableInterrupts();
      }
  updatePressureAndTemperature();
  refreshScreen();
               
   LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF); 
}

void refreshScreen(){
   display.invertDisplay(settingMode); 
   displayMainScreen();
}

void manageButtons(){
  unsigned long  milli = millis();
  boolean upPressed = false;
  boolean downPressed = false;

  // debounce
  while(millis() < milli + 400){
    upPressed |=  !digitalRead(BUTTON_UP_PIN);
    downPressed |= !digitalRead(BUTTON_DOWN_PIN);

    if(upPressed && downPressed){
        delay(200); // debounce push
        while(digitalRead(BUTTON_UP_PIN) && digitalRead(BUTTON_UP_PIN)){
          //wait release
        }
        delay(200); // debounce release
        break;
    }
    
  }
   
  // 2 boutons en mm tps
  if(upPressed &&  downPressed){
        settingMode = !settingMode; 
  }
 
  // appui bref
  if(upPressed && digitalRead(BUTTON_UP_PIN)){
    handleButtonReleaseEvents(true, true);
  }
  if(downPressed && digitalRead(BUTTON_DOWN_PIN)){
    handleButtonReleaseEvents(false, true);
  }

  // appui long
   while(!digitalRead(BUTTON_UP_PIN)){
    handleButtonReleaseEvents(true, false);
    delay(1000);
   } 
  
   while(!digitalRead(BUTTON_DOWN_PIN)){
    handleButtonReleaseEvents(false, false);
    delay(1000); //TODO a gérer avec millis
   }

    //TODO : rebond de release

}



// Affiche les données d'un ecran
void showScreen(String label, double value, String unit) {
  display.clearDisplay(); 
  showBatterylevel(readVcc());
  display.setCursor(0,0);
  display.println(label);

  char buffer2[6];
  sprintf(buffer2, "%4.2i", (int)value);

  display.setCursor(0,10);
  display.setTextSize(4);
  display.print(buffer2);
  display.setTextSize(1);
  display.print(unit);

  display.display();  
}

void displayMainScreen(){
    // init baseAltitude
    /*if (baseAltitude == 0) { 
      baseAltitude = round(altitude);
      saveBaseAltitude = baseAltitude;
    }
    // calculate QNH
   /* if (baseAltitude != saveBaseAltitude) {
      QNH = pressure.sealevel(pression, baseAltitude);
      saveBaseAltitude = baseAltitude;
    }*/
    // Save QNH in EEPROM
    if (round(altitude)!=round(savedQnhAltitude) && savedQNH!=QNH) {
      savedQnhAltitude = altitude;
      savedQNH = QNH;
      EEPROM_writeAnything(eepromAddr, QNH);
    }

    
    switch (screen) {
    case CURRENT_ALTITUDE_SCREEN :

      if (lastValue != altitude) {
        showScreen(F("ALT."), altitude, "m");
        lastValue = altitude;
      }  
      break;

    case MAX_ALTITUDE_SCREEN :
      if (lastValue != altiMax) {
        showScreen(F("ALT. MAX"), altiMax, "m");
        lastValue = altiMax;
      }  
      break;

    case MIN_ALTITUDE_SCREEN :
      if (lastValue != altiMin) {
        showScreen(F("ALT. MIN"), altiMin, "m");
        lastValue = altiMin;
      }  
      break;

    case PRESSION_SCREEN :
      if (lastValue != pression) {
        showScreen(F("PRESS"), pression, "hPa");
        lastValue = pression;
      }  
      break;

    case TEMPERATURE_SCREEN :
      if (lastValue != temperature) {
        showScreen(F("TEMP"), temperature, "°");
        lastValue = temperature;
      }  
      break;

    case BATTERY_SCREEN :
      vcc = readVcc();
      if (lastVcc != vcc) {
        showScreen(F("BAT"), vcc, "mV");
        lastVcc = vcc;
      }       
      break;
   
   case HUMIDITY_SCREEN :
      humidity = readHumidity();
      if (humidity != lastHumidity) {
        showScreen(F("HUMI"), humidity, "%");
        lastHumidity = humidity;
      }       
      break;   
      
      /*
  case UPTIME_SCREEN:
     lastValue = millis()/1000;
     if(lastValue>60){
       lastValue=lastValue/60; 
     }
     showScreen("UPTIME", lastValue, NO_SYMBOL);
     //delay(100);
      break;  */

   case HOUR_SCREEN:
  RTC.read(tm);
  display.clearDisplay(); 
  showBatterylevel(readVcc());
  display.setCursor(0,0);
  display.println("HEURE");
  display.setCursor(0,10);
  display.setTextSize(4);
  
  char buffer2[6];

    sprintf(buffer2, "%02i", tm.Hour );
    display.print(buffer2);
    display.setCursor(40,10);
    display.print(":");
    display.setCursor(55,10);
    sprintf(buffer2, "%02i",tm.Minute );
    display.print(buffer2);
    display.setTextSize(1);
    sprintf(buffer2, "%02i",tm.Second );
    display.print(buffer2);
    display.display();  
      break;  
   
    }
}
/*
void displaySettings(){
  display.clearDisplay(); 
    //display.setTextSize(1);
    display.setCursor(0,0);
    display.println(F("CALIBRATION"));
    display.setCursor(0,15);
    display.print("QNH : ");
    display.print(QNH,2);
    display.println(" hPa");    
    int value;
    if (value != 0) {
      baseAltitude += value; 
      value = 0; 
      // recalcule le QNH
      QNH = pressure.sealevel(pression, baseAltitude);
    }
    display.setCursor(0,45);
    display.print("Alti : ");
    display.println(baseAltitude, 0);
    display.display();  
}*/

// Enregistre un echantillon de pression
void savePressureSample(float pressure) {
  if (indexBfr == MAX_SAMPLES)  {
    indexBfr = 0;
    bufferReady = true;  
  }
  samplesBuffer[indexBfr++] = pressure; 
}

// Retourne la moyenne des echantillons de pression
float getPressureAverage() {
  double sum = 0;
  for (int i =0; i<MAX_SAMPLES; i++) {
    sum += samplesBuffer[i];
  }
  return sum/MAX_SAMPLES;
}

void updatePressureAndTemperature(){
    // get pressure and temperature and calculate altitude 
  char status = pressure.startTemperature();
  if (status != 0) {
    delay(status);
    status = pressure.getTemperature(temperature);
    if (status != 0) {
      status = pressure.startPressure(0);
      if (status != 0) {
        delay(status);
        status = pressure.getPressure(pression, temperature);
        if (status != 0) {
          savePressureSample(pression);
          averagePressure = getPressureAverage();
          if (bufferReady) {
            altitude = pressure.altitude(averagePressure*100, QNH*100);
            setAltiMinMax();
          }

        }
      } 
    } 
  }
}

// Enregistre les altitudes Min & Max
void setAltiMinMax() {
  if (altitude > altiMax) altiMax = altitude;
  if (altitude < altiMin) altiMin = altitude;
}


void resetAltiMinMax() {
  altiMax = altiMin = altitude;
}


// Gestion du bouton relaché
void handleButtonReleaseEvents(boolean up, boolean shortPush) {
    if (settingMode) {
           switch (screen) {
              case CURRENT_ALTITUDE_SCREEN :
                altitude += (up?1:-1)*(shortPush?1:10);    
                // recalcule le QNH
                QNH = pressure.sealevel(pression, altitude);
                 updatePressureAndTemperature();
                break;
          
              case MAX_ALTITUDE_SCREEN :
                 altiMax =  altitude;
                break;
              case MIN_ALTITUDE_SCREEN :
               altiMin =  altitude;
                break;
              case PRESSION_SCREEN :
                break;
              case TEMPERATURE_SCREEN : 
                break;
              case BATTERY_SCREEN :  
                break;
             case HUMIDITY_SCREEN :
            
                break;   
             
             case HOUR_SCREEN:
                   RTC.read(tm); 
                   tm.Hour+=(up?1:-1) * (shortPush?0:1);
                   tm.Minute+=(up?1:-1) * (shortPush?1:0); 
                   RTC.write(tm); 
                   break;   
              } 
           
    }
    else { // Change screen
      screen+=up?1:-1;
      screen%=NB_SCREENS;
      if  (screen == 0){
             screen = NB_SCREENS;
          } 
      
    }
    lastValue = 0; // ???
     refreshScreen();
}


// Show battery level icon
void showBatterylevel(long vcc) {
  if (vcc > 3600) display.drawBitmap(104, 1,  Battery, 20, 7, 1); 
  else if (vcc > 3400) display.drawBitmap(104, 1,  Battery+21, 20, 7, 1); 
  else if (vcc > 3200) display.drawBitmap(104, 1,  Battery+42, 20, 7, 1); 
  else if (vcc > 3000) display.drawBitmap(104, 1,  Battery+63, 20, 7, 1); 
  else display.drawBitmap(104, 1,  Battery+84, 20, 7, 1); 
}
// Read VCC
long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

long readHumidity() {
    Dht11 dht11 = Dht11(DHT11_PIN);
    if(dht11.read()==0){
      return dht11.getHumidity();
    }
    return 0;
}






































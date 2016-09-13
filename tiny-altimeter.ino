#include <SFE_BMP180.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Button.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include <Dht11.h>
#include <Time.h>
#include <DS1307RTC.h>
#include "LowPower.h"


// If using software SPI (the default case):
#define OLED_CS_PIN    0 //unused

#define OLED_DC_PIN         6
#define OLED_RESET_PIN      5
#define OLED_MOSI_PIN       4
#define OLED_CLK_PIN        3
#define OLED_VCC_PIN        2
//#define OLED_GND_PIN        4


#define BUTTON_UP_PIN     10
#define BUTTON_DOWN_PIN     9

#define DHT11_PIN     8
#define DHT11_VCC_PIN     7

#define NO_SYMBOL -1
#define SYMBOL_HPA 0
#define SYMBOL_METER 1
#define SYMBOL_DEG 2
#define SYMBOL_MVOLT 3
//#define SYMBOL_UP 4
//#define SYMBOL_DOWN 5
#define SYMBOL_PERCENT 6

#define CURRENT_ALTITUDE_SCREEN 1
#define MAX_ALTITUDE_SCREEN 2
#define MIN_ALTITUDE_SCREEN 3
#define PRESSION_SCREEN 4
#define TEMPERATURE_SCREEN 5
#define BATTERY_SCREEN 6
#define HUMIDITY_SCREEN 7
//#define UPTIME_SCREEN 8
#define HOUR_SCREEN 8
#define MIN_SCREEN 9
#define NB_SCREENS 9

#define READ_DHT11_MAX_TRY 10

//Adafruit_SSD1306 display(OLED_MOSI_PIN, OLED_CLK_PIN, OLED_DC_PIN, OLED_RESET_PIN, OLED_CS_PIN);

Adafruit_SSD1306 display(-1);


/* Uncomment this block to use hardware SPI
 #define OLED_DC     6
 #define OLED_CS     7
 #define OLED_RESET  8
 Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);
 */

extern uint8_t Font24x40[];
extern uint8_t Symbol[];
//extern uint8_t Splash[];
extern uint8_t Battery[];

SFE_BMP180 pressure;
Button buttonUp = Button(BUTTON_UP_PIN);
Button buttonDown = Button(BUTTON_DOWN_PIN);



int value = 0;
boolean settingMode = false;
double QNH, saveQNH;
double temperature, pression, altitude = 0;
double baseAltitude, saveBaseAltitude = 0;
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
int screen = CURRENT_ALTITUDE_SCREEN; // numero d'ecran
boolean skipClic = false;
tmElements_t tm;

unsigned long lastUpdate=0;

void setup()   {   
  
  //digitalWrite( OLED_VCC_PIN, HIGH);
  //digitalWrite( DHT11_VCC_PIN, HIGH);
 // digitalWrite( OLED_GND_PIN, LOW);
  
  //digitalWrite( BUTTON_UP_PIN, HIGH); //active la pull up interne
  //digitalWrite( BUTTON_DOWN_PIN, HIGH); //active la pull up interne
  
  buttonUp.clickHandler(handleButtonReleaseEvents);
  buttonDown.clickHandler(handleButtonReleaseEvents);
  buttonUp.holdHandler(handleButtonHoldEvents,1000);
  buttonDown.holdHandler(handleButtonHoldEvents,1000);

 // display.begin(SSD1306_SWITCHCAPVCC);
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
  saveQNH = QNH;


  if (!pressure.begin()) {
  
  //display.setCursor(0,0);
    display.println(F("fail"));
    display.display();
    delay(3000); 
  }

//TODO : diminuer luminosité :display.dim(true);
}

void loop() {
  bool b1 = buttonUp.isPressed();
  bool b2 = buttonDown.isPressed();
  if(b1 && b2){
    value = 0;
    // inverse l'écran pour montrer que la manipulation à été prise en compte
    display.invertDisplay(true);
    delay(500);
    display.invertDisplay(false);
      if (screen == MAX_ALTITUDE_SCREEN || screen == MIN_ALTITUDE_SCREEN) {
        resetAltiMinMax();
      } else {
        settingMode = !settingMode;
      }   
      }
  
  
  

  if (settingMode) {
    displaySettings();
  } else {
    if(millis() -1000 > lastUpdate){
      updatePressureAndTemperature();
       displayMainScreen();
       lastUpdate = millis();
    }
               
  }
  delay(50);
  
   LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_OFF); 
}

// Affiche les données d'un ecran
void showScreen(String label, double value, int unit) {
  display.clearDisplay(); 
  showBatterylevel(readVcc());
  display.setCursor(0,0);
  display.println(label);
  drawFloatValue(0, 20, value, unit);
  display.display();  
}

void displayMainScreen(){
    // init baseAltitude
    if (baseAltitude == 0) { 
      baseAltitude = round(altitude);
      saveBaseAltitude = baseAltitude;
    }
    // calculate QNH
    if (baseAltitude != saveBaseAltitude) {
      QNH = pressure.sealevel(pression, baseAltitude);
      saveBaseAltitude = baseAltitude;
    }
    // Save QNH in EEPROM
    if (QNH != saveQNH) {
      saveQNH = QNH;
      EEPROM_writeAnything(eepromAddr, QNH);
    }

    switch (screen) {
    case CURRENT_ALTITUDE_SCREEN :
      if (lastValue != altitude) {
        showScreen(F("ALT."), altitude, SYMBOL_METER);
        lastValue = altitude;
      }  
      break;

    case MAX_ALTITUDE_SCREEN :
      if (lastValue != altiMax) {
        showScreen(F("ALT. MAX"), altiMax, SYMBOL_METER);
        lastValue = altiMax;
      }  
      break;

    case MIN_ALTITUDE_SCREEN :
      if (lastValue != altiMin) {
        showScreen(F("ALT. MIN"), altiMin, SYMBOL_METER);
        lastValue = altiMin;
      }  
      break;

    case PRESSION_SCREEN :
      if (lastValue != pression) {
        showScreen(F("PRESS"), pression, SYMBOL_HPA);
        lastValue = pression;
      }  
      break;

    case TEMPERATURE_SCREEN :
      if (lastValue != temperature) {
        showScreen(F("TEMP"), temperature, SYMBOL_DEG);
        lastValue = temperature;
      }  
      break;

    case BATTERY_SCREEN :
      vcc = readVcc();
      if (lastVcc != vcc) {
        showScreen(F("BAT"), vcc, SYMBOL_MVOLT);
        lastVcc = vcc;
      }       
      break;
   
   case HUMIDITY_SCREEN :
      humidity = readHumidity();
      if (humidity != lastHumidity) {
        showScreen(F("HUMI"), humidity, SYMBOL_PERCENT);
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
     showScreen("H", tm.Hour, NO_SYMBOL);
      break;  
      
   case  MIN_SCREEN:
     RTC.read(tm);
     showScreen("MIN", tm.Minute, NO_SYMBOL);
      break;  
      
    }
}

void displaySettings(){
  display.clearDisplay(); 
    //display.setTextSize(1);
    display.setCursor(0,0);
    display.println(F("CALIBRATION"));
    display.setCursor(0,15);
    display.print("QNH : ");
    display.print(QNH,2);
    display.println(" hPa");    
    //display.print("Debug : ");
    //display.println(debugMsg);
    //display.print("Etat : ");
    //display.println(etat);
    //if (etat == 1) drawSymbol(100, 40, SYMBOL_UP); //UP
    //if (etat == 2) drawSymbol(100, 40, SYMBOL_DOWN); // DOWN
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
}

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
void handleButtonReleaseEvents(Button &btn) {
lastUpdate=0;
if(skipClic){
  skipClic = false;
  return;
}
  
      if (settingMode) { // Settings
        if(btn == buttonUp){
          value++;
        } else {
          value--;
        }     
      }
      else { // Change screen

        if(btn == buttonUp){
            screen++;
            if (screen > NB_SCREENS){
              screen = 1;
            } 
        } else {
          screen--;
          if  (screen == 0){
             screen = NB_SCREENS;
          } 
        }

        lastValue = 0;
      }
}
 

// Gestion de l'appui prolongé sur le bouton
void handleButtonHoldEvents(Button &btn) {
skipClic=true;

   if (settingMode) { // Settings
        if(btn == buttonUp){
          value+=10;
        } else {
          value-=10;
        }     
      }
      
    if(screen == HOUR_SCREEN){
        RTC.read(tm); 
         if(btn == buttonUp){
            tm.Hour++;
        } else {
            tm.Hour--;
        }   
        RTC.write(tm); 
    }

    if(screen == MIN_SCREEN){
        RTC.read(tm); 
         if(btn == buttonUp){
            tm.Minute++;
        } else {
            tm.Minute--;
        }   
        RTC.write(tm); 
    }
}

// Affiche un caractére en x, y
void drawCar(int sx, int sy, int num, uint8_t *font, int fw, int fh, int color) {
  byte row;
  for(int y=0; y<fh; y++) {
    for(int x=0; x<(fw/8); x++) {
      row = pgm_read_byte_near(font+x+y*(fw/8)+(fw/8)*fh*num);
      for(int i=0;i<8;i++) {
        if (bitRead(row, 7-i) == 1) display.drawPixel(sx+(x*8)+i, sy+y, color);
      }
    }
  }
}

// Affiche un gros caractére en x, y
void drawBigCar(int sx, int sy, int num) {
  drawCar(sx, sy, num, Font24x40, 24, 40, WHITE) ;
}

void drawDot(int sx, int sy, int h) {
  display.fillRect(sx, sy-h, h, h, WHITE);
}

// Affiche un symbole en x, y
void drawSymbol(int sx, int sy, int num) {
  drawCar(sx, sy, num, Symbol, 16, 16, WHITE) ;
}

// Affiche un nombre decimal
void drawFloatValue(int sx, int sy, double val, int unit) {
  char charBuf[15];
if (val < 0){
val=-val;
 display.fillRect(0, 39, 12, 6, WHITE);
// drawDot(0, 39, 12);
}
  
  if (val < 10000) { // TODO : et sinon??
    dtostrf(val, 3, 1, charBuf); 
    int nbCar = strlen(charBuf);
    if (nbCar > 5) { // pas de decimal
      for (int n=0; n<4; n++){
        drawBigCar(sx+n*26, sy, charBuf[n]- '0');
      }
      if(unit != NO_SYMBOL){
        drawSymbol(108,sy, unit);
      }
    }
    else {
      drawBigCar(sx+86, sy, charBuf[nbCar-1]- '0');
        drawDot(78, sy+39, 6);
        nbCar--;
        if (--nbCar > 0) drawBigCar(sx+52, sy, charBuf[nbCar-1]- '0');
        if (--nbCar > 0) drawBigCar(sx+26, sy, charBuf[nbCar-1]- '0');
        if (--nbCar > 0) drawBigCar(sx, sy, charBuf[nbCar-1]- '0');
      if(unit != NO_SYMBOL){
        drawSymbol(112,sy, unit);
      }
    }
  }
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






































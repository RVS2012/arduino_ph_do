
//Library
#include <Arduino.h>            // (DO)
#include "DFRobot_PH.h"         // (pH)
#include <EEPROM.h>             // (pH)
#include <OneWire.h>            // (Temp)
#include <DallasTemperature.h>  // (Temp)
#include <SPI.h>                // (SD)
#include <SD.h>                 // (SD)





#define ONE_WIRE_BUS 4  //The ONE_WIRE_BUS is connected to Pin 4
#define PH_PIN A1  //pH Sensor is connected to Pin A1
#define DO_PIN A2  //DO Sensor is connected to Pin A2
#define VREF 5000     //VREF (mv)
#define ADC_RES 1024  //ADC Resolution
#define TWO_POINT_CALIBRATION 0





#define CAL1_V (1600) //mv
#define CAL1_T (25)   //℃
#define CAL2_V (1300) //mv
#define CAL2_T (15)   //℃





//Dissolved Oxygen table
const uint16_t DO_Table[41] = {                                              
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,    
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,        
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,              
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};       





uint16_t ADC_Raw;  //Analog to Digital??
uint16_t ADC_Voltage;  //???
uint16_t DO;  //???





File myFile;  //File Pointer for SD Card
const int chipSelect = 10;  //ChipSelect on SD Card is connected to Pin 10





//Uses formula to calculate DO
int16_t readDO(uint32_t voltage_mv, uint8_t temperature_c)  //Reads Dissolved Oxegyn with voltage & temprature
{
#if TWO_POINT_CALIBRATION == 0                                                                                                                   
  uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;                                               
                                                                                                                                                 
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);                                                                                  
#else                                                                                                                                            
  uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;         
  return (voltage_mv * DO_Table[temperature_c] / V_saturation);                                                                                  
#endif                                                                                                                                           
}





float voltage,phValue,temperature =0;  //Reading raw Voltage, pH, and Temprature as 0
DFRobot_PH ph;
OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);  // Pass our oneWire reference to Dallas Temperature sensor 





unsigned long RTC_Timer  = 0;  //Timer Code
unsigned long MyTestTimer;     //Timer Code
//                             //Timer Code
int RTC_Hour   = 0;            //Setting the Hour, 
int RTC_Minute = 0;            //Setting the Minute
int RTC_Second = 0;            //Setting the Second
int RTC_10nth_Seconds = 0;     //10th of a Second
int minutesOfDay;
long secondsOfDay;





boolean TimePeriodIsOver (unsigned long &periodStartTime, unsigned long TimePeriod) {  //Easy to use helper-function for non-blocking timing
  unsigned long currentMillis  = millis();                                             //Setting up RTC Timer
  if ( currentMillis - periodStartTime >= TimePeriod )                                 //Setting up RTC Timer
  {
    periodStartTime = currentMillis;                                                   //Set new expireTime
    return true;                                                                       //More time than TimePeriod) has elapsed since last time if-condition was true
  }
  else return false;  //Not expired
}





  //Printing for Serial Monitor
void PrintRTC_Data() {
  Serial.print("Software RTC time: ");                              
  myFile.print("Software RTC time: ");                              
  Serial.print(RTC_Hour);                                           
  myFile.print(RTC_Hour);                                           
  Serial.print(":");                                                
  myFile.print(":");                                                
  Serial.print(RTC_Minute);                                         
  myFile.print(RTC_Minute);                                         
  Serial.print(":");                                                
  myFile.print(":");                                                
  Serial.print(RTC_Second);                                         
  myFile.print(RTC_Second);                                         





  minutesOfDay = RTC_Hour * 60 + RTC_Minute;                      //Calculating change on unit of time
  secondsOfDay = RTC_Hour * 3600 + RTC_Minute * 60 + RTC_Second;  //Calculating change on unit of time
}





void SoftRTC() {
  if ( TimePeriodIsOver(RTC_Timer, 100) ) {  //Once every 100 milliseconds 
    RTC_10nth_Seconds ++;                    //Increase 1/10th-seconds counter
    
    if (RTC_10nth_Seconds == 10) {           //If 1/10th-seconds reaches 10
      RTC_10nth_Seconds = 0;                 //Reset 1/10th-seconds counterto zero
      RTC_Second++;                          //Increase seconds counter by 1

      if (RTC_Second == 60) {                //If seconds counter reaches 60
        RTC_Minute++;                        //Increase minutes counter by 1
        RTC_Second = 0;                      //Reset seconds counter to zero
      }
    }

    if (RTC_Minute == 60) {                  //If minutes counter reaches 60
      RTC_Hour++;                            //Increase hour counter by 1
      RTC_Minute = 0;                        //Reset minutes counter to zero
    }

    if (RTC_Hour == 24) {                    //If hours counter reaches 24 
      RTC_Hour = 0;                          //Reset hours counter to zero
    }
  }
}










void setup()
{
    Serial.begin(115200);  
    ph.begin();
    sensors.begin();
    if (!SD.begin(chipSelect)) {
      Serial.println("initialization failed!");
      while (1);
    }
    Serial.println("initialization done.");
}

void loop()
{
    static unsigned long timepoint = millis();

    /****Time********/
    SoftRTC(); // must be called very regular and repeatedly to update the 0.1 second time-ticks
    /***************/
    if(millis()-timepoint > 1000U){                  //time interval: 1s
        myFile = SD.open("datalog.txt", FILE_WRITE);
        if (!myFile)
             Serial.print("ERROR: SD open failed...");
        timepoint = millis();
        temperature = readTemperature();         // read your temperature sensor to execute temperature compensation
        voltage = analogRead(PH_PIN)/1024.0*5000;  // read the voltage
        phValue = ph.readPH(voltage,temperature);  // convert voltage to pH with temperature compensation
        Serial.print(" temperature: ");
        myFile.print(" temperature: ");

        Serial.print(temperature,1);
        myFile.print(temperature,1);

        Serial.print("^C  pH:");
        myFile.print("^C  pH:");

        Serial.print(phValue,2);
        myFile.print(phValue,2);
      
        ADC_Raw = analogRead(DO_PIN);
        ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
  
        Serial.print(" Dissolved Oxygen ");
        myFile.print(" Dissolved Oxygen ");

        Serial.println(readDO(ADC_Voltage, temperature));
        myFile.print(readDO(ADC_Voltage, temperature));

        PrintRTC_Data();
        myFile.print("\n");
        myFile.close();
    }
    ph.calibration(voltage,temperature);           // calibration process by Serail CMD
}

float readTemperature()
{
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
}


#define BLYNK_PRINT Serial
#define donePin 13
#include <WiFi.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include "HX711.h"
#include "soc/rtc.h"  // controlling esp32 speed to accomodate speed of HX711 Andreas Spiess esp32 weight
#include <Wire.h>
#include <SPI.h>
#define EEPROM_SIZE 3
int checkOnce = 0; 
int weightEprom = 0;
int pinValue = 0;
float measuredvbat = 0.0;
#define waterFactor 5 // 5% factor which machine measures and overshoots total wt to stop rapid small additions
#define VBATPIN A13  //This is the pin to measure battery level
 int bootCount = 0; //This quantifies sleep/awake cycles of machine
 float weight = 0.0; //These are retained variables from sleep/wake cycles
 int waterings = 0;
#define relayPinSet 15 //turn water on
#define relayPinUnset 32 //turn water off
#define switchLed 14    //light on switch
#define waterOut 13 
int check = 0;
HX711 scale;     //turns on scale--use the HX711 from Andreas Spiess article
float weightTwo = 0.0;
float changeWeight = 0.0;
  
char auth[] = "569452f42a48496cae14b6d555142ac0";


// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "NETGEAR45";
char pass[] = "gentleshoe219";
BLYNK_CONNECTED(){
  Blynk.syncVirtual(V1);
}
BLYNK_WRITE(V1)
{
   pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V1 Slider value is: ");
  Serial.println(pinValue);
  if(pinValue == 1){
  resetEeprom();
}
}
 

  BlynkTimer timer_temp;
  //temp function
  void myTempEvent(){
  Serial.print("Weight");
  Serial.println(EEPROM.read(1));
  Serial.print("weightTwo");
  Serial.println(weightTwo);
  Serial.print("changeWeight");
  Serial.println(changeWeight);
   Serial.print("bootCount");
  Serial.print(EEPROM.read(0));
  Serial.print("waterings");
  Serial.println(waterings);
  Serial.print("VBat: " ); Serial.println(measuredvbat); 
   Blynk.virtualWrite(V6,weightTwo);
   delay(500);
   Blynk.virtualWrite(V5,measuredvbat);
   delay(500);
   //Blynk.virtualWrite(V4,waterings);
   delay(500);
   
  //this turns off power 
 
    while (1) {
    digitalWrite(donePin, HIGH);
    delay(1);
    digitalWrite(donePin, LOW);
    delay(1);
   
  }
  
 
 
  }
  void setup()
{
  // Debug console
  Serial.begin(115200);
   

  EEPROM.begin(EEPROM_SIZE);
  pinMode(donePin, OUTPUT);
  digitalWrite(donePin, LOW);
  //resetEeprom();
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  Serial.println("total weight: " +  String(weight));
  Serial.println("waterings: " + String(waterings));
 Serial.print("this is the eeprom count");
 Serial.println(EEPROM.read(0));
 
  
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M); //slows down the clock in esp32 to accomodate weight 
  
measuredvbat = analogRead(VBATPIN);
measuredvbat *= 2;    // we divided by 2, so multiply back
measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
measuredvbat /= 4096; // convert to voltage--different from  older esp8266
Serial.print("this is measuredvbat");
Serial.println(measuredvbat);
  pinMode(relayPinSet, OUTPUT);
  pinMode(relayPinUnset, OUTPUT);
  pinMode(switchLed, OUTPUT);
  
  digitalWrite(relayPinUnset,HIGH);
  delay(100);
  digitalWrite(relayPinSet,LOW);
  digitalWrite(relayPinUnset,LOW);
  digitalWrite(switchLed, LOW);
  scale.begin(33, 27); //These correspond to clk and output on board going to your ESP32
  scale.set_scale(-97874); //This is a correction  factor for your particular load cell--see Spiess set up guide for esp32 
  checkOnce = EEPROM.read(0);
   //No torr on this wt system--interferes with the wake-up
 
  if(checkOnce == 0){
    startWeight();
    Serial.println("startwt");
  }
  if(checkOnce == 123) weight = float(EEPROM.read(1) / 10);
   
 
  weightTwo = scale.get_units(10); //Gets current weight
  Serial.println("weightTwo" + String(weightTwo));
  changeWeight = weight - weightTwo;
 Serial.println("change weight" + String(changeWeight));

  if (changeWeight > weight*(waterFactor/100)) {
    waterNow(); //if loss of water is greater than waterFactor  and there is enough water than subroutine for watering
  }
 
   Serial.println("check" + String(check)); 
 //set up blynk timer operation 
 
  Blynk.begin(auth, ssid, pass);
   timer_temp.setInterval(10000L, myTempEvent);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);
}

void loop()
{
  Blynk.run();
  timer_temp.run();

  
  // You can inject your own code or combine it with other sketches.
  // Check other examples on how to communicate with Blynk. Remember
  // to avoid delay() function!
}
void startWeight(){
  float base = scale.get_units(5);
  delay(200);
  Serial.print("place well watered plant on platform"); //subroutine for setting up initial weight of well watered plant
  while(scale.get_units(5)-base < 1.0){
    digitalWrite(switchLed, HIGH);
    delay(500);
    digitalWrite(switchLed, LOW);
    delay(500);
   }
   delay(5000);
   
   weight = scale.get_units(10);
   EEPROM.write(0,123);
   EEPROM.commit();
   weightEprom = weight * 10;
   EEPROM.write(1, weightEprom);
   EEPROM.commit();
   
}

void waterNow(){
  
  ++waterings;
  digitalWrite(relayPinSet, HIGH);
  delay(200);
  digitalWrite(relayPinSet, LOW);
  float xtraWeight =  weight*(waterFactor/100);
  while(scale.get_units(5) < weight + xtraWeight); //keeps watering until the weight is over by waterFactor
  digitalWrite(relayPinUnset, HIGH);
  delay(200); 
  digitalWrite(relayPinUnset, LOW);
  Serial.print("Base Weight");
  Serial.println(weight);
  Serial.print("Weight Now");
  Serial.println(scale.get_units(10));
  
}
void resetEeprom(){
  EEPROM.write(0, 0);
  EEPROM.commit();
  delay(100);
  EEPROM.write(1, 0);
  EEPROM.commit();
  delay(100);
  Serial.print("eeprom0=");
  Serial.println(EEPROM.read(0));
  Serial.print("eeprom1=");
  Serial.println(EEPROM.read(1));
 

    while (1) {
    digitalWrite(donePin, HIGH);
    delay(1);
    digitalWrite(donePin, LOW);
    delay(1);
   
  }
 
  
}
 
  
 

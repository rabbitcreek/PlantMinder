/*This program first turns on the scale and if on for the first time requests that the user 
 * place a well watered plant on the stand--this value is than used to add water to the plant if the 
 * weight falls below a set limit--the machine transmits the info to thingspeak and goes to sleep for a set period 
 * of time Thanks to Andreas Spiess for his work on using ESP32 with HX711
 */

#include <WiFi.h>
#include "HX711.h"
#include "soc/rtc.h"  // controlling esp32 speed to accomodate speed of HX711 Andreas Spiess esp32 weight
#include <Wire.h>
#include <SPI.h>

#define waterFactor 5 // 5% factor which machine measures and overshoots total wt to stop rapid small additions
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  3600        /* Time ESP32 will go to sleep (in seconds) */
#define VBATPIN A13  //This is the pin to measure battery level
RTC_DATA_ATTR int bootCount = 0; //This quantifies sleep/awake cycles of machine
RTC_DATA_ATTR float weight = 0.0; //These are retained variables from sleep/wake cycles
RTC_DATA_ATTR int waterings = 0;
#define relayPinSet 15 //turn water on
#define relayPinUnset 32 //turn water off
#define switchLed 14    //light on switch
#define waterOut 13      //this tells you when reservoir is empty when grounded
int check = 0;
HX711 scale;     //turns on scale--use the HX711 from Andreas Spiess article
float weightTwo = 0.0;
float changeWeight = 0.0;
// Wi-Fi Settings
const char* ssid = "xxxxxxxxx"; // your wireless network name (SSID)
const char* password = "xxxxxxxxx"; // your Wi-Fi network password

WiFiClient client;


const char* server = "api.thingspeak.com";
String channelID = "xxxxxx"; //your thingspeak channelID goes here
String apiKeyChannel = "xxxxxxxxxxxxxx"; //your thingspeak apikeyChannel code goes here
String apiKeyUser = "XXXXXXXXXXXXXXXXX";
void setup() {

  Serial.begin(115200);
  delay(1000);

   //Print the wakeup reason for ESP32
  print_wakeup_reason();
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  Serial.println("total weight: " +  String(weight));
  Serial.println("waterings: " + String(waterings));
 
 
  
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M); //slows down the clock in esp32 to accomodate weight 
  
float measuredvbat = analogRead(VBATPIN);
measuredvbat *= 2;    // we divided by 2, so multiply back
measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
measuredvbat /= 4096; // convert to voltage--different from  older esp8266
Serial.print("VBat: " ); Serial.println(measuredvbat);
  
 
  pinMode(relayPinSet, OUTPUT);
  pinMode(relayPinUnset, OUTPUT);
  pinMode(switchLed, OUTPUT);
  pinMode(waterOut, INPUT_PULLUP);
  digitalWrite(relayPinUnset,HIGH);
  delay(100);
  digitalWrite(relayPinSet,LOW);
  digitalWrite(relayPinUnset,LOW);
  digitalWrite(switchLed, LOW);
  scale.begin(33, 27); //These correspond to clk and output on board going to your ESP32
  scale.set_scale(-97874); //This is a correction  factor for your particular load cell--see Spiess set up guide for esp32 
  
   //No torr on this wt system--interferes with the wake-up
  if(bootCount == 1){
    startWeight();
  }
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
  weightTwo = scale.get_units(10); //Gets current weight
  Serial.println("weightTwo" + String(weightTwo));
  changeWeight = weight - weightTwo;
  if(digitalRead(waterOut)<1)check = 1; //This checks status of water in tank 1 = ok; 0 = low
  delay(500);
  if(digitalRead(waterOut)==1 && check == 1)check=0;  
  if( (changeWeight > weight*(waterFactor/100)) && check == 1){
    waterNow(); //if loss of water is greater than waterFactor  and there is enough water than subroutine for watering
  }
   Serial.println("change weight" + String(changeWeight));
   Serial.println("check" + String(check));
    WiFi.begin(ssid, password);
// ThingSpeak Settings
 while (WiFi.status() != WL_CONNECTED) {
    Serial.println("connecting");
    delay(1000);
  }
  if (client.connect(server, 80)) {
    
    

    // Construct API request body
String str_sensor4 = String(measuredvbat);
String str_sensor3 = String(check);
String str_sensor = String(weightTwo);
String str_sensor2 = String(waterings);
String str_sensor5 = String(bootCount);
String postStr = "api_key="+apiKeyChannel+"&field1="+str_sensor+"&field2="+str_sensor2+"&field3="+str_sensor3+"&field4="+str_sensor4+"&field5="+str_sensor5;
client.println("POST /update HTTP/1.1");
client.println("Host: api.thingspeak.com");
client.println("Connection: close");
client.println("Content-Type: application/x-www-form-urlencoded");
client.print("Content-Length: ");
client.println(postStr.length());
client.println();
client.print(postStr);
client.stop();
 }

 Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}
void loop() {
  
 
  
}
 
  void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
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
 

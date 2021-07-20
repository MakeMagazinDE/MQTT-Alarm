//**********ALARMKONTAKT 25.01.2020, Aenderungen vom 30.6.2021****************
#include <ESP8266WiFi.h> 
#include <PubSubClient.h> 
#include <EEPROM.h>
#define LED_BUILTIN 2
#define ALARMKONTAKT "Alarmkontakt02"
#define ALARMKONTAKT1 Alarmkontakt02
ADC_MODE(ADC_VCC); //Voraussetzung zum Abfragen der Betriebsspannung
const char* ssid = "***";      //hier SSID des WLAN-Netzwerks eintragen
const char* password = "***";  //hier Passwort des WLAN-Netzwerks eintragen
const char* mqtt_server = "192.168.178.42";
unsigned int cnt_off, cnt_on;
char msg[75],topic[]="out/" ALARMKONTAKT "/Topic", mqtt_client_id[]="" ALARMKONTAKT "";
WiFiClient ALARMKONTAKT1; 
PubSubClient client (ALARMKONTAKT1); 
//********************************************************************************
void setup() {
  pinMode(14, OUTPUT);
  digitalWrite(14, HIGH);
  pinMode(12, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  //Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED) {
    delay(10);
    if(millis()%500<10) digitalWrite(LED_BUILTIN, LOW);// Turn the LED on (LOW is ON !!!)
    else if(millis()%500<20) digitalWrite(LED_BUILTIN, HIGH);// LED blitzt alle 500  ms kurz auf*/
  }
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback); //Subscribe, also die Funktion zum Empfangen von MQTT-Nachrichten braucht nicht definiert werden
  EEPROM.begin(512);
  cnt_off = EEPROM.read(0)+256*EEPROM.read(1);
  cnt_on  = EEPROM.read(2)+256*EEPROM.read(3);
}
//********************************************************************************
void loop() {
  if (!client.connected()) reconnect();
  //client.loop(); //für das funktionieren der Subscribe-Funktion, die hier nicht genutzt wird
  int kontakt=digitalRead(12);
  if(kontakt==0) cnt_off++; else cnt_on++;
  snprintf (msg, 75, "{\"Kontakt\":%d,\"Spannung\":%d,\"Zeit\":%d,\"Cnt_off\":%d,\"Cnt_on\":%d}", kontakt, ESP.getVcc(), millis(), cnt_off, cnt_on);
  //Serial.print("Publish message: "); Serial.println(msg);
  client.publish(topic, msg);
  EEPROM.write(0,cnt_off%256); EEPROM.write(1,cnt_off/256);
  EEPROM.write(2,cnt_on %256); EEPROM.write(3,cnt_on /256);
  EEPROM.commit(); //eeprom write abschließen
  delay(20); //muss sein, sonst wird die mqtt-Nachricht nicht gesendet
  ESP.deepSleep(0, WAKE_NO_RFCAL);
  digitalWrite(14, LOW);
  delay(2000);
  //Serial.println("Diese Zeile wird nie ausgegeben . . .");
}
//********************************************************************************
//SUBSRCIBE Funktion, die hier nicht benötigt wird und somit auskommentiert ist
//void callback(char* topic, byte* payload, unsigned int length) {
//  Serial.print("Message arrived [");Serial.print(topic);Serial.print("]: ");
//  for (int i = 0; i < length; i++) Serial.print((char)payload[i]); 
//  Serial.println();
//}
//********************************************************************************
void reconnect() {
  while (!client.connected()) {  // Loop until we're reconnected
    //Serial.print("Attempting MQTT connection..."); // Attempt to connect
    if (client.connect(mqtt_client_id)) {
      //Serial.println("connected"); // Once connected, publish an announcement...
      //client.subscribe("inTopic/Alarmkontakt02"); // ... and resubscribe
    }
    else {
      //Serial.print("failed, rc=");Serial.print(client.state());
      //Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//********************************************************************************

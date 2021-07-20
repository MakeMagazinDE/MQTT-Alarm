//**********ALARMzentrale 13.04.2020****************
#include <ESP8266WiFi.h> 
#include <PubSubClient.h> 
#include <ArduinoJson.h> //https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#define LED_BUILTIN 2
#define TASTER 0
#define PIEZO 5
#define LED_STATUS 4
#define ALARMDAUER 120000    // Dauer des Alarms am openCollectorAusgang des Transistors, default 120000 Millisek. (2 Minuten)
#define VERZOEGERUNG 15000   // Dauer der Alarmverzoegerung fuer Alamkreise gemaess alarmverzoegerung[15], default = 15000 (15 Sekunden)
#define MINDESTSPANNUNG 2800 // Mindesspannung des Alarmkontaktes, sonst blinkt die entsprechende LED bei offenem Kontakt
#define FREQ_PIEZO 2000      // für Piezo die "Wechselstrom" (getaktet) benötigen, meist so um die 2000 Hz, bei 0 Gleichstrom am Opencolector
#define EINSCHALTVERZOEGERUNG 180000        // Einschaltverzoegerung in Millisekunden, default 180000 (3 Minuten)
const char* ssid = "xxxxxxxxx";             // Name des WLAN
const char* password = "xxxxxxxxxxxx";      // Passwort des WLAN
const char* mqtt_server = "192.168.178.42"; // Adresse des MQTT-Servers
boolean alarmverzoegerung[15]={0,0,1,0,0,0,0,0,0,0,0,0,0,0,0}, //in diesem Bsp. ist der Alarmkreis 3 verzoegert
        ein_led[15][4]={{0,0,0,1},{0,0,1,0},{0,0,1,1},{0,1,0,0},{0,1,0,1},{0,1,1,0},{0,1,1,1},{1,0,0,0},
                        {1,0,0,1},{1,0,1,0},{1,0,1,1},{1,1,0,0},{1,1,0,1},{1,1,1,0},{1,1,1,1}},//Matrix, bei welchem Alarmkontakt leuchten welche LEDs
        alarmkreise[15]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},       // Alarmkreis geschlossen = 1, offen = 0
        batterie[15]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},          // Status der Batterie von den Alarmkreisen 1=O.K.
        alarm_on_fl=false,                                     //damit auch nach rücksetzen des Ereignisses der Alarm weitergeht 
        ta_gedrueckt=false,                                    //Taster ist gedrückt
        ta_fl=false,                                           //Flag ob ein- oder ausgeschaltet
        t_einverz_fl=false,                                    //Flag Einschaltverzögerung
        alarm_fl=false,                                        //Flag Alarm ist an
        ti_ausverz_fl=false,                                   //Flag Timer für Ausschaltverzögerung ist gestartet
        ti_alarm_fl=false,                                      //Flag Timer für Alarmdauer ist gestartet  
        blink_on_fl=false,                                     //Flag blinken
        led_blink_fl,                                          //Flag blinken
        portwert=1;                                            //letzter Zustand des Tasters
byte port_led[4]={13,12,14,16};     // 13=LED1, 12= LED2, 14=LED4, 16=LED8
unsigned long ti_ausverz,                   //Timer Ausschaltverzögerung
              ti_alarm,                     //Timer Alarmdauer
              t_einverz_ti=0,               //Timer Einschaltverzögerung       
              led_blink_ti;                 //Timer LED Blinken
WiFiClient Alarmzentrale; 
PubSubClient client (Alarmzentrale); 
//********************************************************************************
void setup() {
  for(int i=0;i<4;i++) {
    pinMode(port_led[i], OUTPUT);
    digitalWrite(port_led[i],0);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,1); //invertierte Logik, LED aus
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS,0); 
  pinMode(PIEZO, OUTPUT);
  digitalWrite(PIEZO,0);
  pinMode(TASTER, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.print("\n Alarmzentrale  13.04.2020");Serial.print(", Connecting to ");Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden");
  client.setServer(mqtt_server, 1883); //MQTT Server bekanntgeben, Portnummer ist 1883 (Standard)
  client.setCallback(callback);        //Funktionsname für abnotierte Nachrichten festlegen, d.h. hier kommen die von den Alarmkontakten Nachrichten an
}
//********************************************************************************
void loop() {
  if (!client.connected()) reconnect(); //MQTT starten
  client.loop();
  taster_alarm_ein_aus();       //Taster zum Ein- und AUsschalten abfragen und ta_fl entsprechend setzen
  if(ta_fl==true) {             //wenn eingeschaltet, die
    if(t_einverz()==true) {     //Einschaltverzoegerung abwarten
      alarm_fl=true;            //Alarmanlage eingeschaltet
    }
  }
  int nr=status_alarmkreis();   //ist ein Alarmkreis offen, falls ja in nr merken
  if(nr>=0||alarm_on_fl==true) { //alarm_on_fl==true damit auch nach rücksetzen des Ereignisses der Alarm weitergeht
    if(alarm_fl==true && ausschaltverzoegerung(nr)==false) alarm(); //erst berücksichtigen, wenn der Alarm scharf ist.
  }
  yield(); // dem ESP8266 Zeit für das WLAN geben, hier eigentlich unnötig, da in Loop impliziert
}
//***************************Beispielinhalt in Topic: out/Alarmkontakt02/Topic *****
//***************************Beispielinhalt in nachricht: {"Kontakt":1,"Spannung":3137,"Zeit":3055,"Cnt_off":32,"Cnt_on":32}******
void callback(char* topic, byte* nachricht, unsigned int length) { // diese Funktion wird aufgerufen, wenn eine MQTT Nachricht empfangen wurde
  int nr;
  char buf[200]; 
  for (int i = 0; i < length; i++) buf[i]=((char)nachricht[i]); //umkopieren der Nachricht in buf 
  buf[length] ='\0';                                            //und mit \0 abschließen damit JSON das verarbeiten kann
  Serial.print("Nachricht erhalten [");Serial.print(topic);Serial.print("]: ");for (int i = 0; i < length; i++) Serial.print((char)nachricht[i]);Serial.println();
  if((nr=ersteziffer_in_string(topic,strlen(topic)))>=0) {      //Nummer des Kontaktes in nr merken
    StaticJsonBuffer<200> jsonBuffer;                           //json Aufbereitung aus der Beispieldatei JsonParserExample übernommen!
    JsonObject& root = jsonBuffer.parseObject(buf);             // Umwandlung des Inhalts von buf
    if (!root.success()) Serial.println("JSON-Problem: parseObject() failed"); //alles in Ordnung?, z.B. {} an der richtigen Stelle bzw. vorhanden
    alarmkreise[nr-1] = (boolean) root["Kontakt"];              // jetzt können die Inhalte zu den in "" stehenden Feldnamen zugewiesen werden
    int spannung = (int) root["Spannung"];                      //der Alarmkontakt liefert die Batteriespannung im Feld Spannung in Millivolt
    if(spannung < MINDESTSPANNUNG) batterie[nr-1] = 0;          //wenn die Spannung zu klein ist, wird 0 als Merker gesetzt und die LEDs zum Blinken gebracht
    else batterie[nr-1] = 1;                                    // Batteriespannung des betreffenden Kontaktes ist in Ordnung
    //könnte man auch wie folgt ohne json lösen, wenn keine Spannungsüberwachung erfolgt
    //alarmkreise[nr-1] = (boolean) ersteziffer_in_string((char*) payload,length); //0-8 deshalb -1
    //Serial.print("KontaktNr= ");Serial.print(nr);
    //Serial.print(",  ersteziffer_in_string= ");Serial.println((boolean)ersteziffer_in_string((char*) payload,length));
  }
}
//********************************************************************************
void reconnect() {
  while (!client.connected()) {                       // Loop until we're reconnected
    yield();
    Serial.print("Versuch einer MQTT Verbindung..."); // Attempt to connect
    if (client.connect("Alarmzentrale")) {            //unter diesem eindeutigem Namen führt der MQTT-Server diesen Teilnehmer
      Serial.println("verbunden");                    // Once connected, publish an announcement...
      client.publish("Alarmzentrale/Topic", "Alarmzentrale einsatzbereit"); //MQTT-Nachricht "senden"
      client.subscribe("out/#");                      // ... and resubscribe, alle Nachrichten mit out/# abonieren
      delay(500);                                     // kleine Pause
    }
    else {
      Serial.print("failed, rc=");Serial.print(client.state());
      Serial.println(" neuer Versuch in 5 Sekunden");
      delay(5000);
    }
  }
}
//*****************************************************************************************************
boolean ausschaltverzoegerung(int nr) { //die Kontakte in dem Array alarmverzoegerung[15] mit 1 gekennzeichnet sind, lösen den Alarm verzögert aus
  if(alarmverzoegerung[nr]==true) {     
    if(ti_ausverz_fl==false) {
      ti_ausverz=millis();
      ti_ausverz_fl=true;
      //Serial.print("ti_ausverz_fl= "); Serial.println(ti_ausverz_fl);
      return(true);
    }
    else {
      if((millis()-ti_ausverz)<VERZOEGERUNG) return(true); // bei verzögerten Alarmkontakten gemäss Tabelle alarmverzoegerung[15]      
    }
  }
  return(false);
}  
//*****************************************************************************************************
int status_alarmkreis() {
  for(int i=0;i<15;i++) {
    if(alarmkreise[i]==0) { // ist einer der Alarmkreise via mqtt callback-Funktion auf 0 gesetzt worden?
      if(batterie[i]==1 || (batterie[i]==0 && ((millis()/500)%2==0))) { //bei Batterie LOW LEDs im Sekundenrythmus blinken lassen
        for(int j=0;j<4;j++) {
          digitalWrite(port_led[j],ein_led[i][j]);
          /*Serial.print("i= ");Serial.print(i);
          Serial.print("j= ");Serial.print(j);Serial.print("port_led[j]= ");Serial.print(port_led[j]);
          Serial.print(", ein_led j/i: "); Serial.print(ein_led[j][i]);
          Serial.print(", ein_led i/j: "); Serial.println(ein_led[i][j]);delay(500);*/
        }
      }
      else for(int j=0;j<4;j++) digitalWrite(port_led[j],0); //bei ungeraden Sekunden alle LED's auf aus
      return(i);
    }
  }
  for(int j=0;j<4;j++) digitalWrite(port_led[j],0); //alle LED's auf aus
  return(-1);
}
//*****************************************************************************************************
boolean taster_alarm_ein_aus() {
  if(entprellread(TASTER)==LOW && ta_gedrueckt==false) {
    ta_gedrueckt=true;
    if(ta_fl==false) {
      ta_fl=true;
      client.publish("Alarmzentrale/Topic","Alarmzentrale eingeschaltet");
      Serial.println("Alarm ein ");
      return(true);
    }
    else {                      //Alarmanlage wird ausgeschaltet und alles moegliche rueckgesetzt
      ta_fl=false;
      t_einverz_fl=false;
      alarm_fl=false;
      ti_ausverz_fl=false;
      ti_alarm_fl=false;
      digitalWrite(PIEZO,0);
      blinken(0);
      blink_on_fl=false;
      alarm_on_fl=false; 
      client.publish("Alarmzentrale/Topic","Alarmzentrale ausgeschaltet");
      Serial.println("Alarm aus");
      return(false);
    }
  }
  if(entprellread(TASTER)==HIGH && ta_gedrueckt==true) { //wenn der Taster wieder losgelassen wird ...
    ta_gedrueckt=false;
  }
  return(false);
}
//*****************************************************************************************************
boolean t_einverz() {
  if(t_einverz_fl==false) {
    t_einverz_fl=true;
    t_einverz_ti=millis();
  }
  if(millis()-t_einverz_ti > EINSCHALTVERZOEGERUNG) { //scharfschalten verzögern
    blinken(1); // LED an
    return true;
  }
  else {
    blinken(2); // LED blinkt
    return false;
  }
}
//*****************************************************************************************************
boolean alarm() {
  if(ti_alarm_fl==false) {
    ti_alarm_fl=true;
    ti_alarm=millis();
  }
  if((millis()-ti_alarm) < ALARMDAUER && ti_alarm_fl==true) { //Alarm für 120 Sekunden
    if(FREQ_PIEZO>0) {
      if(((micros()/(500000/FREQ_PIEZO))%2)==0) digitalWrite(PIEZO,HIGH); 
      else digitalWrite(PIEZO,LOW); 
    }
    else digitalWrite(PIEZO,HIGH);    
    alarm_on_fl=true; //damit auch nach rücksetzen des Ereignisses der Alarm weitergeht
    return true;
  }
  digitalWrite(PIEZO,LOW);
  blinken(3); //schnell/langsam blinken signalisiert es gab einen Alarm
  return false;
}
//*****************************************************************************************************
void blinken(int w){ //w=0 LED aus, w=1 LED an, w=2 LED blinkt, w=3 LED blinkt schnell/lanmgsam
  int tempo_on=650; //Blinkrhytmus in ms
  if(w==0) {digitalWrite(LED_BUILTIN,1); digitalWrite(LED_STATUS,0);return;}
  if(w==1) {
    if(blink_on_fl==false) {
      blink_on_fl=true;
      digitalWrite(LED_BUILTIN,0); 
      digitalWrite(LED_STATUS,1);
    }
    return;
  }
  if(w==2) {
    if(status_alarmkreis()>=0 ) tempo_on=100; //ist noch ein Alarmkreis auf 0?
    led_blink(tempo_on,130);
    return;
  }
  if(w==3) {
    if(millis()% 4000 < 800) led_blink(100,120); 
    else led_blink(80,650); 
  }
}
//*****************************************************************************************************
void led_blink(int tempo, int tempo_off) {
  if(led_blink_fl==false) {
    led_blink_fl=true;
    led_blink_ti=millis();
  }
  if(digitalRead(LED_BUILTIN)==1) tempo=tempo_off;
  if(millis()-led_blink_ti>tempo) {
    digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
    digitalWrite(LED_STATUS,!digitalRead(LED_STATUS));
    led_blink_fl=false;
  }
}
//********************************************************************************
boolean entprellread(int pin) {
  int cnt=0;
  if(portwert!=digitalRead(pin)) {
    delay(20); //enttprellen;
    portwert=!portwert;
    return(portwert);
  }
  else return(portwert);
}
//*****************************************************************************************************
int ersteziffer_in_string(char* a, int length) { //es werden zwei hintereinander 
  int n=0;                                      // stehende Ziffern ausgewertet
  for(int i=0;i<length;i++) {
    if(a[i]>=0x30 && a[i]<=0x39){
      n=a[i]-0x30;
      if(a[i+1]>=0x30 && a[i+1]<=0x39) n=n*10+a[i+1]-0x30; //2.Stelle auch eine Ziffer?
      return(n);
      break;
    }
  }
  return(-1);
}
//*****************************************************************************************************




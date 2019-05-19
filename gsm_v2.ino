#define TINY_GSM_MODEM_SIM800 // указываем тип модуля

#include <avr/wdt.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>


SoftwareSerial SerialAT(11, 10);   //RX, TX
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);


String MQTT_inputtext;
const char apn[]  = "internet.mts.ru";//APN сотового оператора
const char user[] = "mts";// логин если надо
const char pass[] = "mts";// пароль если надо

const char cluser[]  = "test";//логин MQTT
const char clpass[]  = "Qwerty123";//Пароль MQTT
const char clid[]  = "mycarID";// ID MQTT
const char* broker = "postman.cloudmqtt.com";// сервер MQTT

long lastReconnectAttempt = 0;
unsigned long Time1 = 0;
int interval = 1;
int stat = 0;

unsigned long Check_time = 0;
float Taho_ChekTime = 0;

int Tahometr_impulse_count;
int RPM;

float V_brake;

void setup() {
  
  
  Check_time = millis();
  Serial.begin(9600);
  delay(10);
  SerialAT.begin(9600);
  delay(3000);

  Serial.println("Initializing modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);

  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    //wdt_enable(WDTO_1S);
    while (true);
  }
  Serial.println(" OK");

  Serial.print("Connecting to ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    //wdt_enable(WDTO_1S);
    while (true);
  }
  Serial.println(" OK");
  delay(1000);

  // MQTT Broker setup
  mqtt.setServer(broker, 18114);    //порт MQTT брокера\сервера
  mqtt.setCallback(mqttCallback);
  //wdt_enable(WDTO_8S);
}

boolean mqttConnect() {
  Serial.print("Connecting to ");
  Serial.print(broker);
  if (!mqtt.connect(clid, cluser, clpass)) {
    Serial.println(" fail");
    return false;
  }
  Serial.println(" OK");
  mqtt.subscribe("command30");
  return mqtt.connected();
}


void loop()
{
    //wdt_reset();
  if (mqtt.connected())
  {
    mqtt.loop();

  }
  else {
    // Reconnect every 10 seconds
    unsigned long t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
      else if (!modem.gprsConnect(apn, user, pass)) {
        while (true);
      }
    }
  }

}


void mqttCallback(char* topic, byte* payload, unsigned int len) {
  MQTT_inputtext = "";
  for (int i = 0; i < len; i++) {
    MQTT_inputtext += (char)payload[i];
  }
  if (String(topic) == "command30") {
    if ((MQTT_inputtext) == "start1" ) {
      Serial.println ("Команда получена");
     
    }
  }
}

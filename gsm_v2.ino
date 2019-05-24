#include <SoftwareSerial.h>
#include <DallasTemperature.h>      // https://github.com/milesburton/Arduino-Temperature-Control-Library

//  ----------------------------------------- НАЗНАЧАЕМ ВЫВОДЫ  ------------------------------ 

SoftwareSerial SIM800(10, 11);                // для старых плат начиная с версии RX,TX
#define PL_1   7                     // реле на кнопку "заблокировать дверь"
#define PL_2   6                     // реле на кнопку "разаблокировать дверь"

#define RESET_Pin    5                      // аппаратная перезагрузка модема, по сути не задействован

char timeup;

/*  ----------------------------------------- НАСТРОЙКИ MQTT брокера---------------------------------------------------------   */
const char MQTT_user[10] = "arduino";      // api.cloudmqtt.com > Details > User  
const char MQTT_pass[15] = "Rezonans2019";  // api.cloudmqtt.com > Details > Password
const char MQTT_type[15] = "MQIsdp";        // тип протокола НЕ ТРОГАТЬ !
const char MQTT_CID[15] = "Arduino_Sim800";        // уникальное имя устройства в сети MQTT
String MQTT_SERVER = "postman.cloudmqtt.com";   // api.cloudmqtt.com > Details > Server  сервер MQTT брокера
String PORT = "18114";                      // api.cloudmqtt.com > Details > Port    порт MQTT брокера НЕ SSL !
/*  ----------------------------------------- ИНДИВИДУАЛЬНЫЕ НАСТРОЙКИ !!!---------------------------------------------------------   */
String call_phone=  "+375000000000";        // телефон входящего вызова  для управления DTMF
//String call_phone2= "+375000000001";        // телефон для автосброса могут работать не корректно
String APN = "internet.mts.ru";             // тчка доступа выхода в интернет вашего сотового оператора

/*  ----------------------------------------- ДАЛЕЕ НЕ ТРОГАЕМ ---------------------------------------------------------------   */
String pin = "";                            // строковая переменная набираемого пинкода 
float Vbat,V_min;                                 // переменная хранящая напряжение бортовой сети
float m = 68.01;                            // делитель для перевода АЦП в вольты для резистров 39/11kOm
unsigned long Time1, Time2 = 0;
int Timer, count, error_CF, error_C, error_CB, error_CBL;
int interval = 4;                           // интервал тправки данных на сервер после загрузки ардуино
bool ring = false;                          // флаг момента снятия трубки
bool broker = false;                        // статус подклюлючения к брокеру
int days, hours, minutes, seconds;

void setup() {
          
  pinMode(PL_1,OUTPUT);           
  pinMode(PL_2,OUTPUT);
  digitalWrite(PL_1, HIGH);
  digitalWrite(PL_2, HIGH);          
 

 
  delay(100); 
  Serial.begin(9600);                       //скорость порта
  //Serial.setTimeout(50);
  
  SIM800.begin(9600);                       //скорость связи с модемом
  //SIM800.setTimeout(500);                 // тайм аут ожидания ответа
  
  Serial.println("MQTT |21/05/2018"); 
  delay (1000);
  SIM800_reset();
  error_CB=0;
  //delay (10000);
   
 
               }


void loop() {

if (SIM800.available())  resp_modem();                                    // если что-то пришло от SIM800 в Ардуино отправляем для разбора
if (Serial.available())  resp_serial();                                 // если что-то пришло от Ардуино отправляем в SIM800
if (millis()> Time2 + 60000) {Time2 = millis(); 
    if (Timer > 0 ) Timer--, Serial.print("Тм:"), Serial.println (Timer);}
                                               
if (millis()> Time1 + 10000) Time1 = millis(), detection();               // выполняем функцию detection () каждые 10 сек 


            }

void detection(){                                                 // условия проверяемые каждые 10 сек  
    interval--;
    
    if (interval <1) interval = 6;// Serial.println ("Команда обнвления");
    
    if ((interval == 3) && (broker == false)) {SIM800.println("AT+SAPBR=2,1"), delay (2000),Serial.println("нет подключения к брокеру");}
                                    
    if ((interval == 6) && (broker == true))               {SIM800.println("AT+CIPSEND"), delay (200);  
                                                  
                                                          MQTT_FloatPub ("C5/uptime",   millis()/1000,0); 
                                                          SIM800.write(0x1A), delay (200);}    // подключаемся к GPRS 
                   
}  

void resp_modem (){     //------------------ АНЛИЗИРУЕМ БУФЕР ВИРТУАЛЬНОГО ПОРТА МОДЕМА------------------------------
     String at = "";
 //    while (SIM800.available()) at = SIM800.readString();  // набиваем в переменную at
  int k = 0;
   while (SIM800.available()) k = SIM800.read(),at += char(k),delay(1);           
   Serial.println(at);  
 
      if (at.indexOf("+CLIP: \""+call_phone+"\",") > -1) {delay(200), SIM800.println("ATA"), ring = true;}
     
    //  } else if(at.indexOf("+CLIP: \""+call_phone2+"\",") > -1) {delay(50), SIM800.println("ATH0), enginestart();
   
     
else if (at.indexOf("+DTMF: ")  > -1)        {String key = at.substring(at.indexOf("")+9, at.indexOf("")+10);
                                                     pin = pin + key;
                                                     if (pin.indexOf("*") > -1 ) pin= ""; }
else if (at.indexOf("SMS Ready") > -1 || at.indexOf("NO CARRIER") > -1 ) {SIM800.println("AT+CLIP=1;+DDET=1");} // Активируем АОН и декодер DTMF
/*  -------------------------------------- проверяем соеденеиние с ИНТЕРНЕТ, конектимся к серверу------------------------------------------------------- */
else if (at.indexOf("+SAPBR: 1,3") > -1)                                  {SIM800.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), delay(1000);} 
else if (at.indexOf("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\r\nOK") > -1)    {SIM800.println("AT+SAPBR=3,1, \"APN\",\""+APN+"\""), delay (1000); }
else if (at.indexOf("AT+SAPBR=3,1, \"APN\",\""+APN+"\"\r\r\nOK") > -1 )   {SIM800.println("AT+SAPBR=1,1"), delay (1000);}                                // устанавливаем соеденение c интернетом
else if (at.indexOf("AT+SAPBR=2,1\r\r\nERROR") > -1 )                     {Serial.println("костыль 1"), SIM800.println("AT+CFUN=1,1"), error_C++,  delay (1000) ;}
else if (at.indexOf("AT+SAPBR=1,1\r\r\nOK") > -1 )                        {SIM800.println("AT+SAPBR=2,1"),delay (1000);}
else if (at.indexOf("AT+SAPBR=1,1\r\r\nOK") > -1 )                        {SIM800.println("AT+SAPBR=2,1"),delay (1000);}   
else if (at.indexOf("AT+CFUN=1,1\r\r\nOK") > -1 )                         {Serial.println("перезагрузка выполнена"),delay (1000);}
else if (at.indexOf("+SAPBR: 1,1") > -1 )                               {if (broker == false) {delay (200), SIM800.println("AT+CIPSTART=\"TCP\",\""+MQTT_SERVER+"\",\""+PORT+"\""), delay (1000);}
                                                                                           else Serial.println("GPRS OK, Broker OK");}    //cоединение установлено коннектимся с MQTT серверу
//else if (at.indexOf("AT+CIPSTART=\"TCP\",\""+MQTT_SERVER+"\",\""+PORT+"\"\r\r\nOK") > - 1 )  {MQTT_CONNECT();}

                                                  
//else if (at.indexOf("ERROR") > -1 )                                     {delay (200), SIM800.println("AT+CFUN=1,1"), error_CF++, delay (5000), broker = false;}

/*-+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+-Обработка ошибок-+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+--+-+-+-+--+-++-+--+-++-+--+-++-+--+-+*/

else if (at.indexOf("CONNECT FAIL") > -1 )                                {Serial.println(" Костыль 2"),SIM800.println("AT+CFUN=1,1"), error_CF++, delay (1000), broker = false;}  // костыль 1                  // Обработка ошибок 
else if (at.indexOf("CLOSED") > -1 )                                      {broker = false ;}  // костыль 2
else if (at.indexOf("AT+SAPBR=2,1\r\r\nERROR") > -1 )                     {Serial.println(" Костыль 3"),SIM800.println("AT+CFUN=1,1"), error_C++,  delay (1000);}   
//else if (at.indexOf("AT+CIPSTART=\"TCP\",\""+MQTT_SERVER+"\",\""+PORT+"\"\r\r\nERROR")) {if (error_CB == 3) {Serial.println(" Костыль 4"),SIM800.println("AT+CFUN=1,1"),error_CB = 0;} else error_CBL++, error_CB++;}
else if (at.indexOf("AT+SAPBR=2,1\r\r\nERROR") > -1 )                     {Serial.println("Костыль 4"),SIM800.println("AT+CFUN=1,1"), error_C++,  delay (1000);}                                 //Проверка выдачи IP адреса вернула ERROR
else if (at.indexOf("AT+CIPSEND\r\r\nERROR") > -1 )                       {broker = false , Serial.println("Ошибка при отправке"), broker = false ;}  
//else if (at.indexOf("+CME ERROR:") > -1 )                                 {error_CF++; if (error_CF > 5) {error_CF = 0, SIM800.println("AT+CFUN=1,1");}}   // костыль 4 
else if (at.indexOf("CONNECT OK") > -1)                                   {MQTT_CONNECT();}

/*-------------------------------------------------------окончание обработки ошибок -----------------------------------------------------------------*/

else if (at.indexOf("+CIPGSMLOC: 0,") > -1   )                            {String LOC = at.substring(26,35)+","+at.substring(16,25);
                                                                            SIM800.println("AT+CIPSEND"), delay (200);
                                                                            MQTT_PUB ("C5/ussl", LOC.c_str()),
                                                                            SIM800.write(0x1A);} 
                                                  
else if (at.indexOf("+CUSD:") > -1   )                                    {String BALANS = at.substring(13, 26);
                                                                            SIM800.println("AT+CIPSEND"), delay (200);
                                                                            MQTT_PUB ("C5/ussd", BALANS.c_str()), 
                                                                            SIM800.write(0x1A);} 
                                                  
else if (at.indexOf("+CSQ:") > -1   )                                     {String RSSI = at.substring(at.lastIndexOf(":")+1,at.lastIndexOf(","));  // +CSQ: 31,0
                                                                            SIM800.println("AT+CIPSEND"), delay (200);
                                                                            MQTT_PUB ("C5/rssi", RSSI.c_str()), 
                                                                            SIM800.write(0x1A);} 
 
//else if (at.indexOf("ALREADY CONNECT") > -1)     {SIM800.println("AT+CIPSEND"), delay (200); 
else if (at.indexOf("ALREAD") > -1)               {SIM800.println("AT+CIPSEND"), delay (200); // если не "влезает" "ALREADY CONNECT"                                     
                                                  MQTT_FloatPub ("C5/uptime",   millis()/1000,0); 
                                                  SIM800.write(0x1A);}
                     


else if (at.indexOf("C5/comandpl1_1",4) > -1 )      { digitalWrite(PL_1, LOW);   
                                                      delay(3000);                     
                                                      digitalWrite(PL_1, HIGH);   
                                                      delay(1000); 
                                                      SIM800.println("AT+CIPSEND"), delay (200); 
                                                      MQTT_PUB ("C5/comand","pl1_0"); 
                                                      SIM800.write(0x1A);}
                                                      // запрос локации
else if (at.indexOf("C5/comandpl2_1",4) > -1 )      { digitalWrite(PL_2, LOW);   
                                                      delay(1000);                       
                                                      digitalWrite(PL_2, HIGH);   
                                                      delay(1000);
                                                      SIM800.println("AT+CIPSEND"), delay (200); 
                                                      MQTT_PUB ("C5/comand","pl2_0"); 
                                                      SIM800.write(0x1A);} 
                                                      // запрос локации
else if (at.indexOf("C5/comandbalans",4) > -1 )     {SIM800.println("AT+CUSD=1,\"*100#\""); }                                     // запрос баланса
else if (at.indexOf("C5/comandrssi",4) > -1 )       {SIM800.println("AT+CSQ"); }                                                  // запрос уровня сигнала
else if (at.indexOf("C5/comandlocation",4) > -1 )   {SIM800.println("AT+CIPGSMLOC=1,1"); }                                        // запрос локации

else if (at.indexOf("C5/comandRefresh",4) > -1 )    {     
                                                          SIM800.println("AT+CIPSEND"), delay (200);                                          
                                                          MQTT_FloatPub ("C5/CB", error_CBL,0);
                                                          MQTT_FloatPub ("C5/CL", error_C,0); 
                                                          MQTT_FloatPub ("C5/CF", error_CF,0); 
                                                          MQTT_FloatPub ("C5/uptime", millis()/1000,0); 
                                                          SIM800.write(0x1A); 
                                                          
            
   at = "";      }                                                  // Возвращаем ответ модема в монитор порта , очищаем переменную

       if (pin.indexOf("123") > -1 ){ pin= "";} 
 // else if (pin.indexOf("777") > -1 ){ pin= "", SIM800.println("AT+CFUN=1,1");}           // костыль 3
  else if (pin.indexOf("789") > -1 ){ pin= "", delay(1500), SIM800.println("ATH0");} 
  else if (pin.indexOf("#")   > -1 ){ pin= "", SIM800.println("ATH0");}
                              
 } 

//void blocking (bool st) {digitalWrite(Lock_Pin, st ? HIGH : LOW), Security = st, Serial.println(st ? "На охране":"Открыто");} // функция удержания реле блокировки/разблокировки на выходе out4



 
void resp_serial (){     // ---------------- ТРАНСЛИРУЕМ КОМАНДЫ из ПОРТА В МОДЕМ ----------------------------------
     String at = "";   
 //    while (Serial.available()) at = Serial.readString();
  int k = 0;
   while (Serial.available()) k = Serial.read(),at += char(k),delay(1);
     SIM800.println(at), at = "";   }   


void  MQTT_FloatPub (const char topic[15], float val, int x) {char st[10]; dtostrf(val,0, x, st), MQTT_PUB (topic, st);}

void MQTT_CONNECT () {
  SIM800.println("AT+CIPSEND"), delay (100);
     
  SIM800.write(0x10);                                                              // маркер пакета на установку соединения
  SIM800.write(strlen(MQTT_type)+strlen(MQTT_CID)+strlen(MQTT_user)+strlen(MQTT_pass)+12);
  SIM800.write((byte)0),SIM800.write(strlen(MQTT_type)),SIM800.write(MQTT_type);   // тип протокола
  SIM800.write(0x03), SIM800.write(0xC2),SIM800.write((byte)0),SIM800.write(0x3C); // просто так нужно
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_CID)),  SIM800.write(MQTT_CID);  // MQTT  идентификатор устройства
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_user)), SIM800.write(MQTT_user); // MQTT логин
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_pass)), SIM800.write(MQTT_pass); // MQTT пароль

//  MQTT_PUB ("C5/status", "Подключено");                                          // пакет публикации
  MQTT_SUB ("C5/comand");                                                          // пакет подписки на присылаемые команды
  MQTT_SUB ("C5/settimer");                                                        // пакет подписки на присылаемые значения таймера
  SIM800.write(0x1A);                                                              // маркер завершения пакета
  broker = true;   
  }                                         

void  MQTT_PUB (const char MQTT_topic[15], const char MQTT_messege[15]) {          // пакет на публикацию

  SIM800.write(0x30), SIM800.write(strlen(MQTT_topic)+strlen(MQTT_messege)+2);
  SIM800.write((byte)0), SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic); // топик
  SIM800.write(MQTT_messege);   }                                                  // сообщение

void  MQTT_SUB (const char MQTT_topic[15]) {                                       // пакет подписки на топик
  
  SIM800.write(0x82), SIM800.write(strlen(MQTT_topic)+5);                          // сумма пакета 
  SIM800.write((byte)0), SIM800.write(0x01), SIM800.write((byte)0);                // просто так нужно
  SIM800.write(strlen(MQTT_topic)), SIM800.write(MQTT_topic);                      // топик
  SIM800.write((byte)0);  }                          


void SIM800_reset() {SIM800.println("AT+CFUN=1,1"),delay (1000);}                        // перезагрузка модема 
//void callback()     {SIM800.println("ATD"+call_phone+";"),    delay(3000);} // обратный звонок при появлении напряжения на входе IN1

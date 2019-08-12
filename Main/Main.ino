// ------------------------
//         Main.ino
//  modified by xdq and yjr
// ------------------------


#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <FlightGPS.h>
#include <RelayXBee.h>

// predefined pins and serials
#define ledData 10     // Data light
#define ledGPS 3   // GPS ready light
#define ledSD 5      // power light
#define ledXBee 6   // Xbee light
#define relayPin 7      // Use for relay (not needed)
#define chipSelect 53   // CS
#define pressure_pin A12
#define gps_ser Serial1 // hardware serial RX1, TX1 on arduino mega
#define xBee_ser Serial2// hardware serial RX2, TX2 on arduino mega
#define ONE_WIRE_BUS 25

// data strings
String ID = "YURI";     // ID for your XBee - 2-4 character string, A-Z and 0-9 only please
String data;
String command;

// function defined
OneWire oneWire1(ONE_WIRE_BUS);
DallasTemperature temperature(&oneWire1);
File datalog;                               // File object for datalogging
char filename[] = "FLYLOG00.csv";           // Template for file name to save data
bool SDactive = false;                      // used to check for SD card before attempting to log
FlightGPS gps = FlightGPS(&gps_ser);        // GPS object - connect to serial line
RelayXBee xbee = RelayXBee(&xBee_ser, ID);  // XBee object - connect to serial line
unsigned long timer = 0;                    // used to keep track of datalog cycles

void pinsetup() {
  // 启动各个引脚
  pinMode(ledSD, OUTPUT);
  pinMode(ledXBee, OUTPUT);
  pinMode(ledGPS,OUTPUT);
  pinMode(ledData, OUTPUT);
  pinMode(10, OUTPUT);        // SD 卡用

  int const pinNum = 4;
  int const pins[4] = {ledData, ledGPS, ledXBee, ledSD};

  // 跑马灯，表示初始化
  for(int k=0;k<2;k++)
  {
    for (int i=0;i<pinNum;i++) {
      digitalWrite(pins[i], HIGH);
      delay(128);
    }
    for (int i=0;i<pinNum;i++) {
      digitalWrite(pins[i], LOW);
      delay(128);
    }
  }
  
}

void swipe(int pin) {
  for(int i=0;i<3;i++){
    digitalWrite(pin, HIGH);
    delay(128);
    digitalWrite(pin, LOW);
    delay(128);
  }
}

void setup() {
  pinsetup();                   // 初始化各引脚
  Serial.begin(9600);           // start serial communication
  
  // GPS 模块启动
  gps_ser.begin(ADAFRUIT_BAUD); // start gps communication
  gps.init();                   // gps setup
  Serial.println("GPS configured");
  swipe(ledGPS);
  
  // XBee 启动
  pinMode(relayPin, INPUT_PULLUP);
  xBee_ser.begin(XBEE_BAUD);
  xbee.init('A');
  Serial.println("Xbee Initialized");
  swipe(ledXBee);
 
  // SD 卡模块启动
  Serial.print("Initializing SD card...");
  if(!SD.begin(chipSelect)) {                               // 尝试启动 SD 卡
    Serial.println("Card failed, or not present");          // 输出错误信息
  } 
  else {                                                    // 开始加载 SD 模块
    Serial.println("Card initialized.\nCreating File..."); 
    for (byte i = 0; i < 100; i++) {                        // 创建日志，最多 100 个同序列文件
      filename[6] = '0' + i/10; 
      filename[7] = '0' + i%10; 
      if (!SD.exists(filename)) {                           // 无此文件，可创建新文件
        datalog = SD.open(filename, FILE_WRITE);
        SDactive = true;                                    // SD 卡模块启动成功
        swipe(ledSD);
        Serial.println("Logging to: " + String(filename));  // 输出文件信息
        break;                                              // 退出
      }
    }
    if (!SDactive) {                                        // 启动失败，文件已满
      Serial.println("No available file names; clear SD card to enable logging");
    }
  }

  // 数据头输出
  String header = "GPS Date,GPS time,Lat,Lon,Alt (m),# Sats,Solar Voltage,temperature,pressure in psi,";
  Serial.println(header);
  swipe(ledData);
  if (SDactive) {
    datalog.println(header);
    datalog.close();
  }

}

void loop() 
{
  gps.update();     // 更新 GPS

  updateXbee();     // 更新 XBee

  if (millis() - timer > 250){
    digitalWrite(ledData, LOW);       // 数据灯关闭
    digitalWrite(ledSD, LOW);         // SD 卡关闭
    digitalWrite(ledGPS, LOW);
      
    if (millis() - timer > 1000){     // 每秒输出一次数据
      timer = millis();
      // 使用 String 函数转化、连接各个数据
      String data = String(gps.getMonth()) + "/" + String(gps.getDay()) + "/" + String(gps.getYear()) + ","
                    + String(gps.getHour()) + ":" + String(gps.getMinute()) + ":" + String(gps.getSecond()) + ","
                    + String(gps.getLat(), 4) + "," + String(gps.getLon(), 4) + "," + String(gps.getAlt_meters(), 1) + ","
                    + String(gps.getSats()) + "," + String(getvoltage())+String("V") +"," + String(gettemperature())+ ","
                    + String(getpressure()) + ",";
      digitalWrite(ledData, HIGH);    // 数据灯打开
      Serial.println(data);           // 输出在串口监视器中
      if (SDactive)                   // SD 卡可用，输出数据至 SD 卡中
      {
        digitalWrite(ledSD,HIGH);     // 同时打开 SD 灯，表示数据写入 SD 卡中
        datalog = SD.open(filename, FILE_WRITE);
        datalog.println(data);
        datalog.close();
      }
    }
  }

  if(String(gps.getSats())=="0")      // 卫星数为 0，则关闭 GPS 灯
    digitalWrite(ledGPS, LOW);
  else 
    digitalWrite(ledGPS, HIGH);  // 打开 GPS 灯
}

float getvoltage()//get output voltage of the solar panel
{
  return analogRead(A1) *(5.0/1023.0/0.1068);
}

double gettemperature()//get temperature
{
  temperature.requestTemperatures();
  return temperature.getTempCByIndex(0); 
}

float getpressure()//get pressure
{ 
  return (analogRead(pressure_pin)*(5.0/1024) - (0.1*5.0))/(4.0/15.0);
}

void updateXbee() //Xbee
{
  digitalWrite(ledXBee,LOW); //will make sure the LED is off when a cycle begins so you can look for the LED to turn on to indicate communication
  
  if(xBee_ser.available()>0){
    command = xbee.receive();
    digitalWrite(ledXBee,HIGH);}        //check xBee for incoming messages that match ID
  if (command.equals("status")){
    xbee.send(data);
    delay(100);
    digitalWrite(ledXBee,LOW);}
  else if (command.equals("temperature")){
    xbee.send("temperature = " + String(gettemperature()));
    delay(100);
    digitalWrite(ledXBee,LOW);}
  else if (command.equals("pressure")){
    xbee.send("pressure = " + String(getpressure()));
    delay(100);
    digitalWrite(ledXBee,LOW);
    }
  else if (command.equals("voltage")){
    xbee.send("voltage = " + String(getvoltage()));
    delay(100);
    digitalWrite(ledXBee,LOW);
    }
  else{xbee.send("ERROR. Cannot identify command: " + command);} // notice the delay and LED flip is not added. Look for more than a second long xbee blink to indicate message error.
}

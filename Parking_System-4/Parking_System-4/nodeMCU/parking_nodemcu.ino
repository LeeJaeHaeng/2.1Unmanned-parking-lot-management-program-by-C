#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Servo.h>

const char* ssid = ""; //WIFI SSID
const char* password = "";//WIFI PW


WiFiServer server(80);  // 포트 80에서 서버 생성
Servo inservo;          // 입차 서보모터 객체 생성
Servo outservo;         // 출차 서보모터 객체 생성

void setup() {
  Serial.begin(9600);

  // 서보모터 초기화
  outservo.attach(D3); //D3 핀에 출차 서보모터 연결
  inservo.attach(D4);  //D4 핀에 입차 서보모터 연결

  // WiFi 연결
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // 서버 시작
  server.begin();
}

void loop() {
  WiFiClient client = server.available();  
  
  if (client) {  
    while (client.connected()) { 
      if (client.available()) {  
        String data = client.readStringUntil('\n');  
        Serial.println("Server Req : " + data);
        
        if (data == "1") {  
          inservo.write(180); 
          delay(3000);
          inservo.write(90);  
          client.println("int park req");
        } else if (data == "0") {  
          outservo.write(0); 
          delay(3000);
          outservo.write(90);  
          client.println("out park req");
        }
        
        client.stop();  
      }
    }
  }
}
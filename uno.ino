/* BSD 2-Clause License

Copyright (c) 2018, Marcin Jamroz
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "uno.h"

// nrf24L01+ configuration
RF24 radio(A0, A1);
RF24Network network(radio);

const uint16_t this_node = 00;
const uint16_t remote_node = 01;


// Ethernet configuration
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
//OLEK IPAddress my_ip(192, 168, 0, 200);

EthernetClient ethernetClient;
//end


// MQTT configuration
IPAddress mqtt_server(192, 168, 0, 105);

PubSubClient mqttClient(ethernetClient);
// end




//debug
int odebrane = 0;
int wyslane = 0;
unsigned long debugTime = 0;
unsigned long debugSendTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Uno started");

  radio.begin();
  network.begin(90, this_node);

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);

  //OLEK Ethernet.begin(mac, my_ip);
  Ethernet.begin(mac);
  Serial.print(F("My IP address: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(F("."));
  }
  Serial.println();
// radio.setAutoAck(false);


  Serial.println("Uno started");

  delay(1500);
}

void loop() {

//  if (millis() - debugSendTime > 500 && wyslane < 500) {
//    sendPoUART(10, 0, NULL);
//    wyslane++;
//    debugSendTime = millis();
//  }

//  if (millis() - debugTime > 10000) {
//    //Serial.print("--------wyslano: "); Serial.println(wyslane);
//    Serial.print("--------odebrane: "); Serial.println(odebrane);
//    debugTime = millis();
//  }

  network.update();

  while (network.available()) {
    receive();
  }

  if (resend_bg() == false){
        mqttClient.publish(ERROR_TOPIC, NO_SEND_MINI, sizeof(NO_SEND_MINI));
  }

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

}

int convertToInt(int byteRead)
{
  return byteRead - '0';
}

void callback(char* topic, byte* payload, unsigned int length) {
  {
    char choice = 'a';
    uint16_t moveTime = 0;
    uint8_t moveSpeed = 0;

    //   Serial.print(F("Message arrived ["));
    //  Serial.print(topic);
    //  Serial.println(F("] "));

    choice = (char)payload[0];

    if (choice == UP || choice == DOWN || choice == LEFT || choice == RIGHT) {
      int i = 1;
      char tmp = (char)payload[1];

      while (tmp != ',') {
        moveTime = moveTime * 10 + convertToInt(payload[i]);
        i++;
        tmp = (char)payload[i];
      }

      i++;

      while (i < length) {
        moveSpeed = moveSpeed * 10 + convertToInt(payload[i]);
        i++;
      }
    }

    if (choice == UP) forward(moveTime, moveSpeed);
    else if (choice == DOWN) backward(moveTime, moveSpeed);
    else if (choice == RIGHT) right(moveTime, moveSpeed);
    else if (choice == LEFT) left(moveTime, moveSpeed);
    else if (choice == CHECK_PROGRESS) checkProgress();
    else if (choice == STOP) stop();
  }
}



void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("arduinoGateway")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.subscribe(SEND_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      switch (mqttClient.state()) {
        case MQTT_CONNECTION_TIMEOUT:
          Serial.println(F("Error: MQTT_CONNECTION_TIMEOUT!!!"));
          break;
        case MQTT_CONNECTION_LOST:
          Serial.println(F("Error: MQTT_CONNECTION_LOST!!!"));
          break;
        case MQTT_CONNECT_FAILED:
          Serial.println(F("Error: MQTT_CONNECT_FAILED!!!"));
          break;
        case MQTT_DISCONNECTED:
          Serial.println(F("Error: MQTT_DISCONNECTED!!!"));
          break;
        case MQTT_CONNECTED://???
        case MQTT_CONNECT_BAD_PROTOCOL:
          Serial.println(F("Error: MQTT_CONNECT_BAD_PROTOCOL!!!"));
          break;
        case MQTT_CONNECT_BAD_CLIENT_ID:
          Serial.println(F("Error: MQTT_CONNECT_BAD_CLIENT_ID!!!"));
          break;
        case MQTT_CONNECT_UNAVAILABLE:
          Serial.println(F("Error: MQTT_CONNECT_UNAVAILABLE!!!"));
          break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
          Serial.println(F("Error: MQTT_CONNECT_BAD_CREDENTIALS!!!"));
          break;
        case MQTT_CONNECT_UNAUTHORIZED:
          Serial.println(F("Error: MQTT_CONNECT_UNAUTHORIZED!!!"));
          break;
        default:
          Serial.println(F("Error: UNKNOWN ("));
          Serial.print(mqttClient.state()); Serial.println(F(")"));
      }
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Methods to handle returns from mini pro
void forwardSrvRet(uint8_t a) {
  if (a == 1) {
    mqttClient.publish(ANSWER_TOPIC, COMM, sizeof(COMM));
  //  Serial.println(F("WysĹ‚ano zlecenie forward"));
  }
  else
    mqttClient.publish(ERROR_TOPIC, NOCOMM, sizeof(NOCOMM));
}
void backwardSrvRet(uint8_t a) {
  if (a == 1) {
    mqttClient.publish(ANSWER_TOPIC, COMM, sizeof(COMM));
    Serial.println(F("WysĹ‚ano zlecenie backward"));
  } else
    mqttClient.publish(ERROR_TOPIC, NOCOMM, sizeof(NOCOMM));
}
void leftSrvRet(uint8_t a) {
  if (a == 1) {
    mqttClient.publish(ANSWER_TOPIC, COMM, sizeof(COMM));
    Serial.println(F("WysĹ‚ano zlecenie left"));
  } else
    mqttClient.publish(ERROR_TOPIC, NOCOMM, sizeof(NOCOMM));
}
void rightSrvRet(uint8_t a) {
  if (a == 1) {
    mqttClient.publish(ANSWER_TOPIC, COMM, sizeof(COMM));
    Serial.println(F("WysĹ‚ano zlecenie right"));
  } else
    mqttClient.publish(ERROR_TOPIC, NOCOMM, sizeof(NOCOMM));
}
void checkProgressSrvRet(uint16_t a) {
  char tmp[sizeof(uint16_t)];
  dtostrf(a, sizeof(uint16_t), 0, tmp);

  mqttClient.publish(ANSWER_TOPIC, tmp, sizeof(tmp));
  Serial.println(F("WysĹ‚ano zlecenie check"));
}
void stopSrvRet(uint8_t a) {
  if (a == 1) {
    mqttClient.publish(ANSWER_TOPIC, COMM, sizeof(COMM));
    Serial.println(F("WysĹ‚ano zlecenie stop"));
  } else
    mqttClient.publish(ERROR_TOPIC, NOCOMM, sizeof(NOCOMM));
}


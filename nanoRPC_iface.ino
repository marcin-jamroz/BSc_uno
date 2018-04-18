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

#ifndef __NANORPC_IFACE__
#define __NANORPC_IFACE__

#define SYN_MSG   0x00
#define SEND_MSG  0x01
#define ACK_MSG   0x02

#define POUART_HEADER_SIZE  2
#define POUART_RELIABLE_HEADER_SIZE  (POUART_HEADER_SIZE+1)

#define ACK_POUART_CHANNEL 10
#define MSG_MAX_BUFFER 50

const unsigned long reSendTimer = 100;
uint8_t max_resend = 5;
unsigned long sendTime = 0;
byte resendCounter = 0;
uint8_t msg_copy[MSG_MAX_BUFFER];
uint8_t msg_copy_size = 0;

uint8_t seqCur = 0;
uint8_t seqPrev = 0;

boolean isAck = true;


void receive()
{
  RF24NetworkHeader header;
  uint8_t PoUARTframe[MSG_MAX_BUFFER];
  uint8_t frameSize = network.read(header, &PoUARTframe, MSG_MAX_BUFFER);

  uint8_t tmpType = 0;
  uint8_t tmpSeq = 0;

  tmpType = PoUARTframe[0] & 0x03;
  tmpSeq = PoUARTframe[0] >> 2;
  
  uint8_t channelID = PoUARTframe[1];

  if (tmpType == SEND_MSG && seqPrev != tmpSeq) {
    seqPrev = tmpSeq;
    sendPoUART_ACK(ACK_POUART_CHANNEL, 0, NULL);
    isAck = false;

    uint8_t len = PoUARTframe[2];
    uint8_t payload[len];

    for (int i = 0; i < len; i++) {
      payload[i] = PoUARTframe[i + 3];
    }

    recvPoUART(channelID, len, payload);

    //    Serial.print(F("ChannelID=")); Serial.println(channelID);
    //    Serial.print(F("Len=")); Serial.println(len);
    //    Serial.print(F("Choice=")); Serial.println(choice);
    //    Serial.print(F("MoveSpeed=")); Serial.println(moveSpeed);
    //    Serial.print(F("MoveTime=")); Serial.println(moveTime);

  } else if (tmpType == ACK_MSG && tmpSeq == seqCur && channelID == ACK_POUART_CHANNEL) {
    Serial.println("Otrzymalem potwierdzenie");
    isAck = true;
    //   Serial.println("Dostalem potwierdzenie od UNO");
  } else if (seqPrev == tmpSeq) {
    sendPoUART_ACK(10, 0, NULL);
  }
}

uint8_t inc(uint8_t tmp) {
  tmp++;
  if (tmp > 63) {
    tmp = 0;
  }
  return tmp;
}


boolean resend_bg() {
  if (millis() - sendTime > reSendTimer && isAck == false && resendCounter < max_resend) {
    uint8_t tmp_msg[msg_copy_size];
    memcpy(&tmp_msg, &msg_copy, msg_copy_size);
    RF24NetworkHeader header(remote_node);
    sendTime = millis();
    network.write(header, &tmp_msg, sizeof(tmp_msg));
    resendCounter++;
    return true;
    Serial.print("Wysylam resend nr: "); Serial.println(resendCounter);
  } else if(resendCounter == max_resend && isAck == false){
    isAck = true;
    return false;
  } else {
    return true;
  }
}

void sendPoUART_ACK(uint8_t channel, uint8_t payload_len, uint8_t *payload) {
  uint8_t PoUARTframe[POUART_RELIABLE_HEADER_SIZE + payload_len];

  PoUARTframe[0] = (uint8_t)(ACK_MSG);
  PoUARTframe[0] = (PoUARTframe[0] & 0x03) | (seqPrev << 2);

  for (int i = 0; i < payload_len; i++) {
    PoUARTframe[i + POUART_RELIABLE_HEADER_SIZE] = payload[i];
  }

  PoUARTframe[1] = channel;
  PoUARTframe[2] = payload_len;

  RF24NetworkHeader header(remote_node);
  network.write(header, &PoUARTframe, sizeof(PoUARTframe));

  ////////////////// DEBUG /////////////////////////////
  Serial.println("Wysylam wiadomosc ACK: ");
  // wyslane++;
}

// //Communication support in external functions:
void sendPoUART(uint8_t channel, uint8_t payload_len, uint8_t *payload) {

  uint8_t PoUARTframe[POUART_RELIABLE_HEADER_SIZE + payload_len];

  seqCur = inc(seqCur);
  PoUARTframe[0] = (uint8_t)(SEND_MSG);
  PoUARTframe[0] = (PoUARTframe[0] & 0x03) | (seqCur << 2);

  for (int i = 0; i < payload_len; i++) {
    PoUARTframe[i + POUART_RELIABLE_HEADER_SIZE] = payload[i];
  }
  sendTime = millis();

  PoUARTframe[1] = channel;
  PoUARTframe[2] = payload_len;

  memcpy(&msg_copy, &PoUARTframe, sizeof(PoUARTframe));
  msg_copy_size = sizeof(PoUARTframe);

  RF24NetworkHeader header(remote_node);
  network.write(header, &PoUARTframe, sizeof(PoUARTframe));

  isAck = false;
  resendCounter = 0;

  /////////////////// DEBUG /////////////////////
  // Serial.println("Wysylam wiadomosc do MINI: ");
  // Serial.print("seqCur: "); Serial.println(seqCur);
  // Serial.print("HEX ramka: "); Serial.println(PoUARTframe[0], HEX);
  // Serial.print("BIN ramka: "); Serial.println(PoUARTframe[0], BIN);
  // wyslane++;
}

// Error support in external functions:
void NANORPC_FRAME_ERROR(uint8_t error_val) {

};

//Big-little endiannes in support external functions:
uint32_t MHTONL(uint32_t tmp) {
  return tmp;
};

uint32_t MNTOHL(uint32_t tmp) {
  return tmp;
};

uint16_t MHTONS(uint16_t tmp) {
  return tmp;
};

uint16_t MNTOHS(uint16_t tmp) {
  return tmp;
};

#endif //__NANORPC_IFACE__



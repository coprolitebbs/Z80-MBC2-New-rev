//
//   Retro WiFi modem: an ESP8266 based RS232<->WiFi modem
//   with Hayes style AT commands and blinking LEDs.
//
//   Originally based on
//   Original Source Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>
//   Additions (C) 2018 Daniel Jameson, Stardot Contributors
//   Additions (C) 2018 Paul Rickards <rickards@gmail.com>
//
//   This program is free software: you can redistribute it and/or modify
//   it under the tertms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

//#include <ESP8266WiFi.h>
//#include <ESP_EEPROM.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//#include <uart_register.h>
//#include "uart_reg_esp32.h"
#include <string.h>
#include <WiFiClientSecure.h>
#include "RetroWiFiModem.h"
#include "globals.h"
#include "support.h"
#include "at_basic.h"
#include "at_extended.h"
#include "at_proprietary.h"
#include <Wire.h>

#define SLAVE_ADDR 0x08



void receiveEvent(int numBytes) {
  StopFlag = true;
  if (numBytes == 1) { //if received 1 byte then get it to buffer
    //while (Wire.available()) {
    byte x = Wire.read();
    AddToi2cInBuff((byte)x);
  } else if (numBytes == 2) { //two bytes is getStatusMode
    byte x = Wire.read();
    byte y = Wire.read();
    if ((x == 1) && (y == 2)) getStatusMode = true;
  }

  StopFlag = false;
}

void requestEvent() {
  StopFlag = true;
  uint8_t pack[2];
  pack[0] = 0;//0x1A;  //Ctl-z для указания, что в порту пусто
  pack[1] = 2;    //Атрибут байта установим по умолчанию в 2. Это значит, что с модема ничего не читалось,

  if (getStatusMode) {  //Получить статус буфера приема
    pack[1] = 2; //Атрибут байта установим по умолчанию в 2. Это значит, что буфер чтения пустой
    if (i2coutbuflen > 1) {
      pack[1] = 1;
    } else if (i2coutbuflen == 1) {
      pack[1] = 0;
    }
    Wire.write(pack, 2);
    getStatusMode = false;
  } else {
    if (StopFlag2 == false) {
      //и нам нечего возвращать на шину I2C компьютеру
      if (i2coutbuflen > 0) {
        pack[0] = (uint8_t)GetFromOutBuf();
        pack[1] = 1;
        if (i2coutbuflen == 0) {
          pack[1] = 0;
        }
      }
      StopFlag3 = false;
    }
    Wire.write(pack, 2);
  }
  StopFlag = false;
  //delayMicroseconds(10);
}



// =============================================================
void setup(void) {
  //bool ok = true;

  EEPROM.begin(sizeof(struct Settings));
  delay(100);
  EEPROM.get(0, settings);
  delay(100);
  Serial.println("EEPROM Read");


  //Wire.is_slave = true;
  //Wire.i2cSlaveInit();


  /*pinMode(RI, OUTPUT);
    pinMode(DCD, OUTPUT);
    pinMode(DSR, OUTPUT);
    digitalWrite(TXEN, HIGH);     // continue disabling TX until
    pinMode(TXEN, OUTPUT);        // we have set up the Serial port

    digitalWrite(RI, !ACTIVE);    // not ringing
    digitalWrite(DCD, !ACTIVE);   // not connected
    digitalWrite(DSR, !ACTIVE);   // modem is not ready
  */
  Serial.begin(115200/*settings.serialSpeed*/);//, getSerialConfig());
  delay(1000);
  Serial.println("Started");
  //digitalWrite(TXEN, LOW);      // enable the TX output
  /*if ( settings.rtsCts ) {
    setHardwareFlow();
    }
    if ( settings.startupWait ) {
    while ( true ) {           // wait for a CR
      yield();
      if ( Serial.available() ) {
        if ( Serial.read() == CR ) {
          break;
        }
      }
    }
    }
  */

  delay(100);
  Wire.begin(SLAVE_ADDR);//, SDA, SCL);
  delay(10);
  Wire.onRequest(requestEvent);
  delay(10);
  Wire.onReceive(receiveEvent);
  Serial.println("Wired");
  // Function to run when data requested from master

  // Function to run when data received from master



  if ( settings.magicNumber != MAGIC_NUMBER ) {
    // no valid data in EEPROM/NVRAM, populate with defaults
    factoryDefaults(NULL);
  }
  sessionTelnetType = settings.telnet;


  
   //httpsClient.setCACert(test_root_ca);


  WiFi.begin();
  if ( settings.ssid[0] ) {
    WiFi.waitForConnectResult();
    WiFi.mode(WIFI_STA);
  }

  if ( settings.listenPort ) {
    tcpServer.begin(settings.listenPort);
  }

  if ( settings.ssid[0] && WiFi.status() == WL_CONNECTED ) {
    setupOTAupdates();
  }

  if ( WiFi.status() == WL_CONNECTED || !settings.ssid[0] ) {
    if ( WiFi.status() == WL_CONNECTED ) {
      //digitalWrite(DSR, ACTIVE);  // modem is finally ready or SSID not configured
    }
    if ( settings.autoExecute[0] ) {
      strncpy(atCmd, settings.autoExecute, MAX_CMD_LEN);
      atCmd[MAX_CMD_LEN] = NUL;
      if ( settings.echo ) {
        out_i2c_ln(atCmd);
        //Serial.println(atCmd);
      }
      doAtCmds(atCmd);                  // auto execute command
    } else {
      sendResult(mR_OK);
    }
  } else {
    sendResult(R_ERROR);           // SSID configured, but not connected
  }
}

// =============================================================
void loop(void) {

  checkForIncomingCall();

  switch ( state ) {

    case CMD_NOT_IN_CALL:
      if ( WiFi.status() == WL_CONNECTED ) {
        ArduinoOTA.handle();
      }
      inAtCommandMode();
      break;

    case CMD_IN_CALL:
      inAtCommandMode();
      if (secure_connected) {
        if ( state == CMD_IN_CALL && !httpsClient.connected() ) {
          endCall();                    // hang up if not in a call
        }
      } else {
        if ( state == CMD_IN_CALL && !tcpClient.connected() ) {
          endCall();                    // hang up if not in a call
        }
      }
      break;

    case PASSWORD:
      inPasswordMode();
      break;

    case ONLINE:
      /*if ( Serial.available() ) {      // data from RS-232 to Wifi
        sendSerialData();
        }*/
      if (i2cinbuflen > 0) {
        sendSerialData();
      }

      if (secure_connected) {
        while ( httpsClient.available() && (i2coutbuflen < I2CBUFFLEN) && (StopFlag == false) && (StopFlag3 == false)/*&& !Serial.available()*/ ) { // data from WiFi to RS-232
          int c = receiveHttpsData();
          if ( c != -1 ) {
            AddToi2cOutBuff((char)c);
            yield();
            //Serial.write((char)c);
          }
        }
      } else {
        while ( tcpClient.available() && (i2coutbuflen < I2CBUFFLEN) && (StopFlag == false) && (StopFlag3 == false)/*&& !Serial.available()*/ ) { // data from WiFi to RS-232
          int c = receiveTcpData();
          if ( c != -1 ) {
            AddToi2cOutBuff((char)c);
            yield();
            //Serial.write((char)c);
          }
        }
      }


      if ( escCount == ESC_COUNT && millis() > guardTime ) {
        state = CMD_IN_CALL;          // +++ detected, back to command mode
        sendResult(mR_OK);
        escCount = 0;
      }
      if (secure_connected) {
        if ( !httpsClient.connected() && !httpsClient.available()) {  // no client?
          endCall();                    // then hang up
        }
      } else {
        if ( !tcpClient.connected() && !tcpClient.available()) {  // no client?
          endCall();                    // then hang up
        }
      }
      break;
  }
}

// =============================================================
void doAtCmds(char *atCmd) {
  size_t len;

  trim(atCmd);               // get rid of leading and trailing spaces
  if ( atCmd[0] ) {
    // is it an AT command?
    if ( strncasecmp(atCmd, "AT", 2) ) {
      sendResult(R_ERROR); // nope, time to die
    } else {
      // save command for possible future A/
      strncpy(lastCmd, atCmd, MAX_CMD_LEN);
      lastCmd[MAX_CMD_LEN] = NUL;
      atCmd += 2;          // skip over AT prefix
      len = strlen(atCmd);

      if ( !atCmd[0] ) {
        // plain old AT
        sendResult(mR_OK);
      } else {
        trim(atCmd);
        while ( atCmd[0] ) {
          if ( !strncasecmp(atCmd, "?", 1)  ) { // help message
            // help
            atCmd = showHelp(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "$SB", 3) ) {
            // query/set serial speed
            atCmd = doSpeedChange(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "$SU", 3) ) {
            // query/set serial data configuration
            atCmd = doDataConfig(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "$SSID", 5) ) {
            // query/set WiFi SSID
            atCmd = doSSID(atCmd + 5);
          } else if ( !strncasecmp(atCmd, "$PASS", 5) ) {
            // query/set WiFi password
            atCmd = doWiFiPassword(atCmd + 5);
          } else if ( !strncasecmp(atCmd, "C", 1) ) {
            // connect/disconnect to WiFi
            atCmd = wifiConnection(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "D", 1) && len > 2 && strchr("TPI", toupper(atCmd[1])) ) {
            // dial a number
            atCmd = dialNumber(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "DS", 2) && len == 3 ) {
            // speed dial a number
            atCmd = speedDialNumber(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "H", 1) || !strncasecmp(atCmd, "H0", 2) ) {
            // hang up call
            atCmd = hangup(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "&Z", 2) && isDigit(atCmd[2]) ) {
            // speed dial query or set
            atCmd = doSpeedDialSlot(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "O", 1) ) {
            // go online
            atCmd = goOnline(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "GET", 3) ) {
            // get a web page (http only, no https)
            atCmd = httpsGet(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "SGT", 3) ) {
            // get a web page (https or http)
            atCmd = httpGet(atCmd + 3);
          } else if ( settings.listenPort && !strncasecmp(atCmd, "A", 1) && tcpServer.hasClient() ) {
            // manually answer incoming connection
            atCmd = answerCall(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "S0", 2) ) {
            // query/set auto answer
            atCmd = doAutoAnswerConfig(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "$SP", 3) ) {
            // query set inbound TCP port
            atCmd = doServerPort(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "$BM", 3) ) {
            // query/set busy message
            atCmd = doBusyMessage(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "&R", 2) ) {
            // query/set require password
            atCmd = doServerPassword(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "I", 1) ) {
            // show network information
            atCmd = showNetworkInfo(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "Z", 1) ) {
            // reset to NVRAM
            atCmd = resetToNvram(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "&V", 2) ) {
            // display current and stored settings
            atCmd = displayAllSettings(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "&W", 2) ) {
            // write settings to EEPROM
            atCmd = updateNvram(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "&F", 2) ) {
            // factory defaults
            atCmd = factoryDefaults(atCmd);
          } else if ( !strncasecmp(atCmd, "E", 1) ) {
            // query/set command mode echo
            atCmd = doEcho(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "Q", 1) ) {
            // query/set quiet mode
            atCmd = doQuiet(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "RD", 2)
                      || !strncasecmp(atCmd, "RT", 2) ) {
            // read time and date
            atCmd = doDateTime(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "V", 1) ) {
            // query/set verbose mode
            atCmd = doVerbose(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "X", 1) ) {
            // query/set extended result codes
            atCmd = doExtended(atCmd + 1);
          } else if ( !strncasecmp(atCmd, "$W", 2) ) {
            // query/set startup wait
            atCmd = doStartupWait(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "NET", 3) ) {
            // query/set telnet mode
            atCmd = doTelnetMode(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "$AE", 3) ) {
            // do auto execute commands
            atCmd = doAutoExecute(atCmd + 3);
          } else if ( !strncasecmp(atCmd, "$TTY", 4) ) {
            // do telnet terminal type
            atCmd = doTerminalType(atCmd + 4);
          } else if ( !strncasecmp(atCmd, "$TTL", 4) ) {
            // do telnet location
            atCmd = doLocation(atCmd + 4);
          } else if ( !strncasecmp(atCmd, "$TTS", 4) ) {
            // do telnet location
            atCmd = doWindowSize(atCmd + 4);
          } else if ( !strncasecmp(atCmd, "&K", 2) ) {
            // do RTS/CTS flow control
            atCmd = doFlowControl(atCmd + 2);
          } else if ( !strncasecmp(atCmd, "$MDNS", 5) ) {
            // handle mDNS name
            atCmd = doMdnsName(atCmd + 5);
          } else if ( !strncasecmp(atCmd, "$I2C", 4) ) {
            atCmd = doI2Cprint(atCmd + 4);
          } else {
            // unrecognized command
            sendResult(R_ERROR);
          }
          trim(atCmd);
        }
      }
    }
  }
}

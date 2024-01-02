void doAtCmds(char *atCmd);             // forward delcaration

//
// We're in local command mode. Assemble characters from the
// serial port into a buffer for processing.
//


void AddToi2cInBuff(char c) {
  if (i2cinbuflen == I2CBUFFLEN) return;
  i2cinbuf[i2cinbuflen] = c;
  i2cinbuflen++;
}

void AddToi2cOutBuff(char c) {
  if (i2coutbuflen == I2CBUFFLEN) return;
  StopFlag2 = true;
  i2coutbuf[i2coutbuflen] = c;
  i2coutbuflen++;
  StopFlag2 = false;
  //if(i2coutbuflen>20) StopFlag3 = true;
}

void out_i2c(char *at) {
  for (int i = 0; i < strlen(at); i++) {
    AddToi2cOutBuff(at[i]);
  }
}

void out_i2c_ln(char *at) {
  for (int i = 0; i < strlen(at); i++) {
    AddToi2cOutBuff(at[i]);
  }
  AddToi2cOutBuff('\r');
  AddToi2cOutBuff('\n');
}

void out_i2c_str(String at) {
  for (int i = 0; i < at.length(); i++) {
    AddToi2cOutBuff(at.charAt(i));
  }
}

void out_i2c_strln(String at) {
  out_i2c_str(at);
  AddToi2cOutBuff('\r');
  AddToi2cOutBuff('\n');
}



char GetFromInBuf() {
  if (i2cinbuflen == 0) return '0';
  char outch = i2cinbuf[0];
  if (i2cinbuflen > 1) {
    for (int i = 1; i < i2cinbuflen; i++) {
      i2cinbuf[i - 1] = i2cinbuf[i];
    }
  }
  i2cinbuflen--;
  return outch;
}

char GetFromOutBuf() {
  if (i2coutbuflen == 0) return '0';
  char outch = i2coutbuf[0];
  if (i2coutbuflen > 1) {
    for (int i = 1; i < i2coutbuflen; i++) {
      i2coutbuf[i - 1] = i2coutbuf[i];
    }
  }
  i2coutbuflen--;
  return outch;
}



void inAtCommandMode() {
  char c;
  bool rdd = false;

  if ( Serial.available() ) {
    c = Serial.read();
    rdd = true;
  }

  if (i2cinbuflen > 0) {
    c = GetFromInBuf();
    rdd = true;
  }

  if (rdd == true) {

    // get AT command
    //if ( Serial.available() ) {
    //c = Serial.read();

    if ( c == LF || c == CR ) {      // command finished?
      if ( settings.echo ) {
        out_i2c(endl);
        //Serial.println();
      }
      doAtCmds(atCmd);               // yes, then process it
      atCmd[0] = NUL;
      atCmdLen = 0;
    } else if ( (c == BS || c == DEL) && atCmdLen > 0 ) {
      atCmd[--atCmdLen] = NUL;      // remove last character
      if ( settings.echo ) {
        sprintf(emas, "\b \b");
        out_i2c(emas);
        //Serial.print(F("\b \b"));
      }
    } else if ( c == '/' && atCmdLen == 1 && toupper(atCmd[0]) == 'A' && lastCmd[0] != NUL ) {
      if ( settings.echo ) {
        AddToi2cOutBuff(c);
        out_i2c(endl);
        //Serial.println(c);
      }
      strncpy(atCmd, lastCmd, sizeof atCmd);
      atCmd[MAX_CMD_LEN] = NUL;
      doAtCmds(atCmd);                  // repeat last command
      atCmd[0] = NUL;
      atCmdLen = 0;
    } else if ( c >= ' ' && c <= '~' ) { // printable char?
      if ( atCmdLen < MAX_CMD_LEN ) {
        atCmd[atCmdLen++] = c;        // add to command string
        atCmd[atCmdLen] = NUL;
      }
      if ( settings.echo ) {
        AddToi2cOutBuff(c);
        //Serial.print(c);
      }
    }
  }
}

//
// send serial data to the TCP client
//
void sendSerialData() {
  static unsigned long lastSerialData = 0;
  // in telnet mode, we might have to escape every single char,
  // so don't use more than half the buffer
  size_t maxBufSize = (sessionTelnetType != NO_TELNET) ? TX_BUF_SIZE / 2 : TX_BUF_SIZE;

  /*size_t len = Serial.available();
    if ( len > maxBufSize) {
    len = maxBufSize;
    }
    if (len > 0) {
    Serial.readBytes(txBuf, len);
    } else
  */
  size_t len = 0;
  if (i2cinbuflen > 0) {
    len = i2cinbuflen;
    if ( len > maxBufSize) {
      len = maxBufSize;
    }
    for (int i = 0; i < len; i++) txBuf[i] = GetFromInBuf();
  }

  if ( escCount || (millis() - lastSerialData >= GUARD_TIME) ) {
    // check for the online escape sequence
    // +++ with a 1 second pause before and after
    for ( size_t i = 0; i < len; ++i ) {
      if ( txBuf[i] == settings.escChar ) {
        if ( ++escCount == ESC_COUNT ) {
          guardTime = millis() + GUARD_TIME;
        } else {
          guardTime = 0;
        }
      } else {
        escCount = 0;
      }
    }
  } else {
    escCount = 0;
  }
  lastSerialData = millis();

  // in Telnet mode, escape every IAC (0xff) by inserting another
  // IAC after it into the buffer (this is why we only read up to
  // half of the buffer in Telnet mode)
  //
  // also in Telnet mode, escape every CR (0x0D) by inserting a NUL
  // after it into the buffer
  if ( sessionTelnetType != NO_TELNET ) {
    for ( int i = len - 1; i >= 0; --i ) {
      if ( txBuf[i] == IAC ) {
        memmove( txBuf + i + 1, txBuf + i, len - i);
        ++len;
      } else if ( txBuf[i] == CR && sessionTelnetType == REAL_TELNET ) {
        memmove( txBuf + i + 1, txBuf + i, len - i);
        txBuf[i + 1] = NUL;
        ++len;
      }
    }
  }
  bytesOut += tcpClient.write(txBuf, len);
  yield();
}

//
// Receive data from the TCP client
//
// We do some limited processing of in band Telnet commands.
// Specifically, we handle the following commanads: BINARY,
// ECHO, SUP_GA (suppress go ahead), TTYPE (terminal type),
// TSPEED (terminal speed), LOC (terminal location) and
// NAWS (terminal columns and rows).
//
int receiveTcpData() {
  static char lastc = 0;
  int rxByte = tcpClient.read();
  ++bytesIn;

  if ( sessionTelnetType != NO_TELNET && rxByte == IAC ) {
    rxByte = tcpClient.read();
    ++bytesIn;
    if ( rxByte != IAC ) { // 2 times 0xff is just an escaped real 0xff
      // rxByte has now the first byte of the actual non-escaped control code
#if DEBUG
      Serial.print('[');
      Serial.print(rxByte);
      Serial.print(',');
#endif
      uint8_t cmdByte1 = rxByte;
      rxByte = tcpClient.read();
      ++bytesIn;
      uint8_t cmdByte2 = rxByte;
#if DEBUG
      Serial.print(rxByte);
#endif
      switch ( cmdByte1 ) {
        case DO:
          switch ( cmdByte2 ) {
            case BINARY:
            case ECHO:
            case SUP_GA:
            case TTYPE:
            case TSPEED:
              bytesOut += tcpClient.write(IAC);      // we will say what
              bytesOut += tcpClient.write(WILL);     // the terminal is
              bytesOut += tcpClient.write(cmdByte2);
              break;
            case LOC:
            case NAWS:
              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(WILL);
              bytesOut += tcpClient.write(cmdByte2);

              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(SB);
              bytesOut += tcpClient.write(cmdByte2);
              switch ( cmdByte2 ) {
                case NAWS:     // window size
                  bytesOut += tcpClient.write((uint8_t)0);
                  bytesOut += tcpClient.write(settings.width);
                  bytesOut += tcpClient.write((uint8_t)0);
                  bytesOut += tcpClient.write(settings.height);
                  break;
                case LOC:      // terminal location
                  bytesOut += tcpClient.print(settings.location);
                  break;
              }
              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(SE);
              break;
            default:
              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(WONT);
              bytesOut += tcpClient.write(cmdByte2);
              break;
          }
          break;
        case WILL:
          // Server wants to do option, allow most
          bytesOut += tcpClient.write(IAC);
          switch ( cmdByte2 ) {
            case LINEMODE:
            case NAWS:
            case LFLOW:
            case NEW_ENVIRON:
            case XDISPLOC:
              bytesOut += tcpClient.write(DONT);
              break;
            default:
              bytesOut += tcpClient.write(DO);
              break;
          }
          bytesOut += tcpClient.write(cmdByte2);
          break;
        case SB:
          switch ( cmdByte2 ) {
            case TTYPE:
            case TSPEED:
              while ( tcpClient.read() != SE ) { // discard rest of cmd
                ++bytesIn;
              }
              ++bytesIn;
              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(SB);
              bytesOut += tcpClient.write(cmdByte2);
              bytesOut += tcpClient.write(VLSUP);
              switch ( cmdByte2 ) {
                case TTYPE:    // terminal type
                  bytesOut += tcpClient.print(settings.terminal);
                  break;
                case TSPEED:   // terminal speed
                  bytesOut += tcpClient.print(settings.serialSpeed);
                  bytesOut += tcpClient.print(',');
                  bytesOut += tcpClient.print(settings.serialSpeed);
                  break;
              }
              bytesOut += tcpClient.write(IAC);
              bytesOut += tcpClient.write(SE);
              break;
            default:
              break;
          }
          break;
      }
      rxByte = -1;
    }
#if DEBUG
    Serial.print(']');
#endif
  }
  // Telnet sends <CR> as <CR><NUL>
  // We filter out that <NUL> here
  if ( lastc == CR && (char)rxByte == 0 && sessionTelnetType == REAL_TELNET ) {
    rxByte = -1;
  }
  lastc = (char)rxByte;
  return rxByte;
}




//
// Receive data from the HTTPS client
//
// We do some limited processing of in band Telnet commands.
// Specifically, we handle the following commanads: BINARY,
// ECHO, SUP_GA (suppress go ahead), TTYPE (terminal type),
// TSPEED (terminal speed), LOC (terminal location) and
// NAWS (terminal columns and rows).
//
int receiveHttpsData() {
  static char lastc = 0;
  int rxByte = httpsClient.read();
  ++bytesIn;

  if ( sessionTelnetType != NO_TELNET && rxByte == IAC ) {
    rxByte = httpsClient.read();
    ++bytesIn;
    if ( rxByte != IAC ) { // 2 times 0xff is just an escaped real 0xff
      // rxByte has now the first byte of the actual non-escaped control code
#if DEBUG
      Serial.print('[');
      Serial.print(rxByte);
      Serial.print(',');
#endif
      uint8_t cmdByte1 = rxByte;
      rxByte = httpsClient.read();
      ++bytesIn;
      uint8_t cmdByte2 = rxByte;
#if DEBUG
      Serial.print(rxByte);
#endif
      switch ( cmdByte1 ) {
        case DO:
          switch ( cmdByte2 ) {
            case BINARY:
            case ECHO:
            case SUP_GA:
            case TTYPE:
            case TSPEED:
              bytesOut += httpsClient.write(IAC);      // we will say what
              bytesOut += httpsClient.write(WILL);     // the terminal is
              bytesOut += httpsClient.write(cmdByte2);
              break;
            case LOC:
            case NAWS:
              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(WILL);
              bytesOut += httpsClient.write(cmdByte2);

              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(SB);
              bytesOut += httpsClient.write(cmdByte2);
              switch ( cmdByte2 ) {
                case NAWS:     // window size
                  bytesOut += httpsClient.write((uint8_t)0);
                  bytesOut += httpsClient.write(settings.width);
                  bytesOut += httpsClient.write((uint8_t)0);
                  bytesOut += httpsClient.write(settings.height);
                  break;
                case LOC:      // terminal location
                  bytesOut += httpsClient.print(settings.location);
                  break;
              }
              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(SE);
              break;
            default:
              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(WONT);
              bytesOut += httpsClient.write(cmdByte2);
              break;
          }
          break;
        case WILL:
          // Server wants to do option, allow most
          bytesOut += httpsClient.write(IAC);
          switch ( cmdByte2 ) {
            case LINEMODE:
            case NAWS:
            case LFLOW:
            case NEW_ENVIRON:
            case XDISPLOC:
              bytesOut += httpsClient.write(DONT);
              break;
            default:
              bytesOut += httpsClient.write(DO);
              break;
          }
          bytesOut += httpsClient.write(cmdByte2);
          break;
        case SB:
          switch ( cmdByte2 ) {
            case TTYPE:
            case TSPEED:
              while ( httpsClient.read() != SE ) { // discard rest of cmd
                ++bytesIn;
              }
              ++bytesIn;
              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(SB);
              bytesOut += httpsClient.write(cmdByte2);
              bytesOut += httpsClient.write(VLSUP);
              switch ( cmdByte2 ) {
                case TTYPE:    // terminal type
                  bytesOut += httpsClient.print(settings.terminal);
                  break;
                case TSPEED:   // terminal speed
                  bytesOut += httpsClient.print(settings.serialSpeed);
                  bytesOut += httpsClient.print(',');
                  bytesOut += httpsClient.print(settings.serialSpeed);
                  break;
              }
              bytesOut += httpsClient.write(IAC);
              bytesOut += httpsClient.write(SE);
              break;
            default:
              break;
          }
          break;
      }
      rxByte = -1;
    }
#if DEBUG
    Serial.print(']');
#endif
  }
  // Telnet sends <CR> as <CR><NUL>
  // We filter out that <NUL> here
  if ( lastc == CR && (char)rxByte == 0 && sessionTelnetType == REAL_TELNET ) {
    rxByte = -1;
  }
  lastc = (char)rxByte;
  return rxByte;
}




//
// return a pointer to a string containing the connect time of the last session
//
char *connectTimeString(void) {
  unsigned long now = millis();
  int hours, mins, secs;
  static char result[9];

  if ( connectTime ) {
    secs = (now - connectTime) / 1000;
    mins = secs / 60;
    hours = mins / 60;
    secs %= 60;
    mins %= 60;
  } else {
    hours = mins = secs = 0;
  }
  result[0] = (char)(hours / 10 + '0');
  result[1] = (char)(hours % 10 + '0');
  result[2] = ':';
  result[3] = (char)(mins / 10 + '0');
  result[4] = (char)(mins % 10 + '0');
  result[5] = ':';
  result[6] = (char)(secs / 10 + '0');
  result[7] = (char)(secs % 10 + '0');
  result[8] = NUL;
  return result;
}

//
// print a result code/string to the serial port
//
void sendResult(int resultCode) {
  if ( !settings.quiet ) {            // quiet mode on?
    out_i2c(endl);
    //Serial.println();                // no, we're going to display something
    if ( !settings.verbose ) {
      if ( resultCode == R_RING_IP ) {
        resultCode = R_RING;
      }
      sprintf(emas, "%d\r\n", resultCode);
      out_i2c(emas);
      //Serial.println(resultCode);   // not verbose, just print the code #
    } else {
      switch ( resultCode ) {       // possible extra info for CONNECT and
        // NO CARRIER if extended codes are
        case R_CONNECT:            // enabled
          out_i2c_str(FPSTR(connectStr));
          //Serial.print(FPSTR(connectStr));
          if ( settings.extendedCodes ) {
            AddToi2cOutBuff(' ');
            sprintf(emas, "%d", settings.serialSpeed);
            out_i2c(emas);
            //Serial.print(' ');
            //Serial.print(settings.serialSpeed);
          }
          out_i2c(endl);
          //Serial.println();
          break;

        case R_NO_CARRIER:
          out_i2c_str(FPSTR(noCarrierStr));
          //Serial.print(FPSTR(noCarrierStr));
          if ( settings.extendedCodes ) {
            sprintf(emas, " (%s)", connectTimeString());
            out_i2c(emas);
            //Serial.printf(" (%s)", connectTimeString());
          }
          out_i2c(endl);
          //Serial.println();
          break;

        case R_ERROR:
          out_i2c_strln(FPSTR(errorStr));
          //Serial.println(FPSTR(errorStr));
          lastCmd[0] = NUL;
          memset(atCmd, 0, sizeof atCmd);
          break;

        case R_RING_IP:
          out_i2c_str(FPSTR(ringStr));
          //Serial.print(FPSTR(ringStr));
          if ( settings.extendedCodes ) {
            AddToi2cOutBuff(' ');
            out_i2c_str(tcpClient.remoteIP().toString());
            //Serial.print(' ');
            //Serial.print(tcpClient.remoteIP().toString());
          }
          out_i2c(endl);
          //Serial.println();
          break;

        case R_ERROR_IN_HOST_STR:
          out_i2c_strln(FPSTR(httpsErrH));
          lastCmd[0] = NUL;
          memset(atCmd, 0, sizeof atCmd);
          break;
        case R_NO_CARRIER_SSL:
          out_i2c_strln(FPSTR(httpsSSLErr));
          lastCmd[0] = NUL;
          memset(atCmd, 0, sizeof atCmd);
          break;
        case R_CONNECT_SSL:
          out_i2c_strln(FPSTR(httpsSSLConn));
          lastCmd[0] = NUL;
          memset(atCmd, 0, sizeof atCmd);
          break;
        case R_NO_CARRIER_SSL2:
          out_i2c_strln(FPSTR(httpsSSLErr2));
          lastCmd[0] = NUL;
          memset(atCmd, 0, sizeof atCmd);
          break;


        default:
          out_i2c_strln(FPSTR(resultCodes[resultCode]));
          //Serial.println(FPSTR(resultCodes[resultCode]));
          break;
      }
    }
  } else if ( resultCode == R_ERROR ) {
    lastCmd[0] = NUL;
    memset(atCmd, 0, sizeof atCmd);
  }
  if ( resultCode == R_NO_CARRIER || resultCode == R_NO_ANSWER ) {
    sessionTelnetType = settings.telnet;
  }
}

//
// terminate an active call
//
void endCall() {
  state = CMD_NOT_IN_CALL;
  if (secure_connected) {
    httpsClient.stop();    
    sendResult(R_NO_CARRIER_SSL2);
  } else {
    sendResult(R_NO_CARRIER);
  }
  tcpClient.stop();
  secure_connected = false;
  connectTime = 0;
  //digitalWrite(DCD, !ACTIVE);
  escCount = 0;
}

//
// Check for an incoming TCP session. There are 3 scenarios:
//
// 1. We're already in a call, or auto answer is disabled and the
//    ring count exceeds the limit: tell the caller we're busy.
// 2. We're not in a call and auto answer is disabled, or the #
//    of rings is less than the auto answer count: either start
//    or continue ringing.
// 3. We're no in a call, auto answer is enabled and the # of rings
//    is at least the auto answer count: answer the call.
//
void checkForIncomingCall() {
  if ( settings.listenPort && tcpServer.hasClient() ) {
    if ( state != CMD_NOT_IN_CALL || (!settings.autoAnswer && ringCount > MAGIC_ANSWER_RINGS) ) {
      WiFiClient droppedClient = tcpServer.available();
      if ( settings.busyMsg[0] ) {
        droppedClient.println(settings.busyMsg);
        droppedClient.print(F("Current call length: "));
        droppedClient.println(connectTimeString());
      } else {
        droppedClient.println(F("BUSY"));
      }
      droppedClient.println();
      droppedClient.flush();
      droppedClient.stop();
      //digitalWrite(RI, !ACTIVE);
      ringCount = 0;
      ringing = false;
    } else if ( !settings.autoAnswer || ringCount < settings.autoAnswer ) {
      if ( !ringing ) {
        ringing = true;            // start ringing
        ringCount = 1;
        //digitalWrite(RI, ACTIVE);
        if ( !settings.autoAnswer || ringCount < settings.autoAnswer ) {
          sendResult(R_RING);     // only show RING if we're not just
        }                          // about to answer
        nextRingMs = millis() + RING_INTERVAL;
      } else if ( millis() > nextRingMs ) {
        /*if ( digitalRead(RI) == ACTIVE ) {
          digitalWrite(RI, !ACTIVE);
          } else {*/
        ++ringCount;
        //digitalWrite(RI, ACTIVE);
        if ( !settings.autoAnswer || ringCount < settings.autoAnswer ) {
          sendResult(R_RING);
        }
        //}
        nextRingMs = millis() + RING_INTERVAL;
      }
    } else if ( settings.autoAnswer && ringCount >= settings.autoAnswer ) {
      //digitalWrite(RI, !ACTIVE);
      tcpClient = tcpServer.available();
      if ( settings.telnet != NO_TELNET ) {
        tcpClient.write(IAC);      // incantation to switch
        tcpClient.write(WILL);     // from line mode to
        tcpClient.write(SUP_GA);   // character mode
        tcpClient.write(IAC);
        tcpClient.write(WILL);
        tcpClient.write(ECHO);
        tcpClient.write(IAC);
        tcpClient.write(WONT);
        tcpClient.write(LINEMODE);
      }
      sendResult(R_RING_IP);
      if ( settings.serverPassword[0]) {
        tcpClient.print(F("\r\nPassword: "));
        state = PASSWORD;
        passwordTries = 0;
        passwordLen = 0;
        password[0] = NUL;
      } else {
        delay(1000);
        state = ONLINE;
        sendResult(R_CONNECT);
      }
      connectTime = millis();
      //digitalWrite(DCD, ACTIVE);
    }
  } else if ( ringing ) {
    //digitalWrite(RI, !ACTIVE);
    ringing = false;
    ringCount = 0;
  }
}

//
// setup for OTA sketch updates
//
void setupOTAupdates() {
  ArduinoOTA.setHostname(settings.mdnsName);

  ArduinoOTA.onStart([]() {
    out_i2c_strln("OTA upload start");
    //Serial.println(F("OTA upload start"));
    //digitalWrite(DSR, !ACTIVE);
  });

  ArduinoOTA.onEnd([]() {
    out_i2c_strln("OTA upload end - programming");
    //Serial.println(F("OTA upload end - programming"));
    //Serial.flush();                  // allow serial output to finish
    //digitalWrite(TXEN, HIGH);        // before disabling the TX output
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int pct = progress / (total / 100);
    static unsigned int lastPct = 999;
    if ( pct != lastPct ) {
      lastPct = pct;
      if ( settings.serialSpeed >= 4800 || pct % 10 == 0 ) {
        sprintf(emas, "Progress: %u%%\r", pct);
        out_i2c(emas);
        //Serial.printf("Progress: %u%%\r", pct);
      }
    }
  });

  ArduinoOTA.onError([](ota_error_t errorno) {
    out_i2c_str("OTA Error - ");
    //Serial.print(F("OTA Error - "));
    switch ( errorno ) {
      case OTA_AUTH_ERROR:
        out_i2c_str("Auth failed");
        //Serial.println(F("Auth failed"));
        break;
      case OTA_BEGIN_ERROR:
        out_i2c_str("Begin failed");
        //Serial.println(F("Begin failed"));
        break;
      case OTA_CONNECT_ERROR:
        out_i2c_str("Connect failed");
        //Serial.println(F("Connect failed"));
        break;
      case OTA_RECEIVE_ERROR:
        out_i2c_str("Receive failed");
        //Serial.println(F("Receive failed"));
        break;
      case OTA_END_ERROR:
        out_i2c_str("End failed");
        //Serial.println(F("End failed"));
        break;
      default:
        sprintf(emas, "Unknown (%u)\r\n", errorno);
        out_i2c(emas);
        //Serial.printf("Unknown (%u)\r\n", errorno);
        break;
    }
    sendResult(R_ERROR);
  });
  ArduinoOTA.begin();
}

//
// Return the SerialConfig value for the current data bits/parity/stop bits
// setting.
//
/*
  SerialConfig getSerialConfig(void) {
   uint8_t serialConfig = 0;
   switch( settings.dataBits ) {
      case 5:
         serialConfig = serialConfig = UART_NB_BIT_5 | (~UART_NB_BIT_MASK & serialConfig);
         break;
      case 6:
         serialConfig = serialConfig = UART_NB_BIT_6 | (~UART_NB_BIT_MASK & serialConfig);
         break;
      case 7:
         serialConfig = serialConfig = UART_NB_BIT_7 | (~UART_NB_BIT_MASK & serialConfig);
         break;
      case 8:
      default:
         serialConfig = serialConfig = UART_NB_BIT_8 | (~UART_NB_BIT_MASK & serialConfig);
         break;
   }
   switch( settings.parity ) {
      case 'E':
         serialConfig = UART_PARITY_EVEN | (~UART_PARITY_MASK & serialConfig);
         break;
      case 'O':
         serialConfig = UART_PARITY_ODD | (~UART_PARITY_MASK & serialConfig);
         break;
      case 'N':
      default:
         serialConfig = UART_PARITY_NONE | (~UART_PARITY_MASK & serialConfig);
         break;
   }
   switch( settings.stopBits ) {
      case '2':
         serialConfig = UART_NB_STOP_BIT_2 | (~UART_NB_STOP_BIT_MASK & serialConfig);
         break;
      case '1':
      default:
         serialConfig = UART_NB_STOP_BIT_1 | (~UART_NB_STOP_BIT_MASK & serialConfig);
         break;
   }
   return (SerialConfig)serialConfig;
  }
*/
// As the ESP8266 is being used as a modem (DCE) in this application,
// I've reversed the naming of RTS/CTS to match what they'd be on
// a modem. The usual naming is correct if the ESP8266 is wired up as
// a DTE, but kept confusing me here as it's wired up as a DCE.
void setHardwareFlow(void) {
  // Enable flow control of DTE -> ESP8266 data with CTS
  // CTS on the EPS8266 is pin GPIO15 which is physical pin 16
  // CTS is an output and should be connected to CTS on the RS232
  // The ESP8266 has a 128 byte receive buffer,
  // so a threshold of 64 is half full
  //pinMode(CTS, OUTPUT /*FUNCTION_4*/); // make pin U0CTS
  //SET_PERI_REG_BITS(UART_CONF1(0), UART_RX_FLOW_THRHD, 64, UART_RX_FLOW_THRHD_S);
  //SET_PERI_REG_MASK(UART_CONF1(0), UART_RX_FLOW_EN);

  // Enable flow control of ESP8266 -> DTE data with RTS
  // RTS on the EPS8266 is pin GPIO13 which is physical pin 7
  // RTS is an input and should be connected to RTS on the RS232
  //pinMode(RTS, /*FUNCTION_4*/INPUT); // make pin U0RTS
  //SET_PERI_REG_MASK(UART_CONF0(0), UART_TX_FLOW_EN);
}

// trim leading and trailing blanks from a string
void trim(char *str) {
  char *trimmed = str;
  // find first non blank character
  while ( *trimmed && isSpace(*trimmed) ) {
    ++trimmed;
  }
  if ( *trimmed ) {
    // trim off any trailing blanks
    for ( int i = strlen(trimmed) - 1; i >= 0; --i ) {
      if ( isSpace(trimmed[i]) ) {
        trimmed[i] = NUL;
      } else {
        break;
      }
    }
  }
  // shift string only if we had leading blanks
  if ( str != trimmed ) {
    int i, len = strlen(trimmed);
    for ( i = 0; i < len; ++i ) {
      str[i] = trimmed[i];
    }
    str[i] = NUL;
  }
}

//
// Parse a string in the form "hostname[:port]" and return
//
// 1. A pointer to the hostname
// 2. A pointer to the optional port
// 3. The numeric value of the port (if not specified, 23)
//
void getHostAndPort(char *number, char* &host, char* &port, int &portNum) {
  char *ptr;

  port = strrchr(number, ':');
  if ( !port ) {
    portNum = TELNET_PORT;
  } else {
    *port++ = NUL;
    portNum = atoi(port);
  }
  host = number;
  while ( *host && isSpace(*host) ) {
    ++host;
  }
  ptr = host;
  while ( *ptr && !isSpace(*ptr) ) {
    ++ptr;
  }
  *ptr = NUL;
}

//
// Display the operational settings
//
void displayCurrentSettings(void) {
  out_i2c_strln("Active Profile:"); yield();
  sprintf(emas, "Baud.......: %u\r\n", settings.serialSpeed);
  out_i2c(emas); yield();
  sprintf(emas, "SSID.......: %s\r\n", settings.ssid);
  out_i2c(emas); yield();
  sprintf(emas, "Pass.......: %s\r\n", settings.wifiPassword);
  out_i2c(emas); yield();
  sprintf(emas, "mDNS name..: %s.local\r\n", settings.mdnsName);
  out_i2c(emas); yield();
  sprintf(emas, "Server port: %u\r\n", settings.listenPort);
  out_i2c(emas); yield();
  sprintf(emas, "Busy msg...: %s\r\n", settings.busyMsg);
  out_i2c(emas); yield();
  sprintf(emas, "E%u Q%u V%u X%u &K%u NET%u S0=%u\r\n",
          settings.echo, settings.quiet, settings.verbose,
          settings.extendedCodes, settings.rtsCts, settings.telnet,
          settings.autoAnswer);
  out_i2c(emas); yield();


  /*Serial.println(F("Active Profile:")); yield();
    //Serial.printf("Baud.......: %lu\r\n", settings.serialSpeed); yield();
    Serial.printf("Baud.......: %u\r\n", settings.serialSpeed); yield();
    Serial.printf("SSID.......: %s\r\n", settings.ssid); yield();
    Serial.printf("Pass.......: %s\r\n", settings.wifiPassword); yield();
    Serial.printf("mDNS name..: %s.local\r\n", settings.mdnsName); yield();
    Serial.printf("Server port: %u\r\n", settings.listenPort); yield();
    Serial.printf("Busy msg...: %s\r\n", settings.busyMsg); yield();
    Serial.printf("E%u Q%u V%u X%u &K%u NET%u S0=%u\r\n",
                settings.echo, settings.quiet, settings.verbose,
                settings.extendedCodes, settings.rtsCts, settings.telnet,
                settings.autoAnswer); yield();
  */
  out_i2c_strln("Speed dial:");
  //Serial.println(F("Speed dial:"));
  for ( int i = 0; i < SPEED_DIAL_SLOTS; ++i ) {
    if ( settings.speedDial[i][0] ) {
      sprintf(emas, "%u: %s,%s\r\n", i, settings.speedDial[i], settings.alias[i]);
      out_i2c(emas);
      //Serial.printf("%u: %s,%s\r\n",
      //              i, settings.speedDial[i], settings.alias[i]);
      yield();
    }
  }
}

//
// Display the settings stored in flash (NVRAM).
//
void displayStoredSettings(void) {
  bool v_bool = false;
  uint8_t v_uint8 = 0;
  uint16_t v_uint16 = 0;
  uint32_t v_uint32 = 0;
  char v_char16[16 + 1];
  char v_char32[32 + 1];
  char v_char50[50 + 1];
  char v_char64[64 + 1];
  char v_char80[80 + 1];
  out_i2c_strln("Stored Profile:"); yield();
  sprintf(emas, "Baud.......: %u\r\n", EEPROM.get(offsetof(struct Settings, serialSpeed), v_uint32));
  out_i2c(emas); yield();
  sprintf(emas, "SSID.......: %s\r\n", EEPROM.get(offsetof(struct Settings, ssid), v_char32));
  out_i2c(emas); yield();
  sprintf(emas, "Pass.......: %s\r\n", EEPROM.get(offsetof(struct Settings, wifiPassword), v_char64));
  out_i2c(emas); yield();
  sprintf(emas, "mDNS name..: %s.local\r\n", EEPROM.get(offsetof(struct Settings, mdnsName), v_char80));
  out_i2c(emas); yield();
  sprintf(emas, "Server port: %u\r\n", EEPROM.get(offsetof(struct Settings, listenPort), v_uint16));
  out_i2c(emas); yield();
  sprintf(emas, "Busy Msg...: %s\r\n", EEPROM.get(offsetof(struct Settings, busyMsg), v_char80));
  out_i2c(emas); yield();
  sprintf(emas, "E%u Q%u V%u X%u &K%u NET%u S0=%u\r\n",
          EEPROM.get(offsetof(struct Settings, echo), v_bool),
          EEPROM.get(offsetof(struct Settings, quiet), v_bool),
          EEPROM.get(offsetof(struct Settings, verbose), v_bool),
          EEPROM.get(offsetof(struct Settings, extendedCodes), v_bool),
          EEPROM.get(offsetof(struct Settings, rtsCts), v_bool),
          EEPROM.get(offsetof(struct Settings, telnet), v_bool),
          EEPROM.get(offsetof(struct Settings, autoAnswer), v_uint8)
         );
  out_i2c(emas); yield();


  /*Serial.println("Stored Profile:"); yield();
    //Serial.printf("Baud.......: %lu\r\n", EEPROM.get(offsetof(struct Settings, serialSpeed),v_uint32)); yield();
    Serial.printf("Baud.......: %u\r\n", EEPROM.get(offsetof(struct Settings, serialSpeed), v_uint32)); yield();
    Serial.printf("SSID.......: %s\r\n", EEPROM.get(offsetof(struct Settings, ssid), v_char32)); yield();
    Serial.printf("Pass.......: %s\r\n", EEPROM.get(offsetof(struct Settings, wifiPassword), v_char64)); yield();
    Serial.printf("mDNS name..: %s.local\r\n", EEPROM.get(offsetof(struct Settings, mdnsName), v_char80)); yield();
    Serial.printf("Server port: %u\r\n", EEPROM.get(offsetof(struct Settings, listenPort), v_uint16)); yield();
    Serial.printf("Busy Msg...: %s\r\n", EEPROM.get(offsetof(struct Settings, busyMsg), v_char80)); yield();
    Serial.print("E");
    Serial.print(EEPROM.get(offsetof(struct Settings, echo), v_bool), DEC);
    Serial.print(" Q");
    Serial.print(EEPROM.get(offsetof(struct Settings, quiet), v_bool), DEC);
    Serial.print(" V");
    Serial.print(EEPROM.get(offsetof(struct Settings, verbose), v_bool), DEC);
    Serial.print(" X");
    Serial.print(EEPROM.get(offsetof(struct Settings, extendedCodes), v_bool), DEC);
    Serial.print(" &K");
    Serial.print(EEPROM.get(offsetof(struct Settings, rtsCts), v_bool), DEC);
    Serial.print(" NET");
    Serial.print(EEPROM.get(offsetof(struct Settings, telnet), v_bool), DEC);
    Serial.print(" S0=");
    Serial.println(EEPROM.get(offsetof(struct Settings, autoAnswer), v_uint8), DEC);
    Serial.println("\r\n");*/

  /*Serial.printf("E%u Q%u V%u X%u &K%u NET%u S0=%u\r\n",
                EEPROM.get(offsetof(struct Settings, echo), v_bool)
                EEPROM.get(offsetof(struct Settings, quiet), v_bool),
                EEPROM.get(offsetof(struct Settings, verbose), v_bool),
                EEPROM.get(offsetof(struct Settings, extendedCodes), v_bool),
                EEPROM.get(offsetof(struct Settings, rtsCts), v_bool),
                EEPROM.get(offsetof(struct Settings, telnet), v_bool),
                EEPROM.get(offsetof(struct Settings, autoAnswer), v_uint8));*/
  //yield();

  out_i2c_strln("Speed dial:");
  //Serial.println(F("Speed dial:"));
  int speedDialOffset = offsetof(struct Settings, speedDial);
  int aliasOffset = offsetof(struct Settings, alias);
  for (int i = 0; i < SPEED_DIAL_SLOTS; i++) {
    EEPROM.get(
      speedDialOffset + i * (MAX_SPEED_DIAL_LEN + 1),
      v_char50
    );
    if ( v_char50[0] ) {
      sprintf(emas, "%u: %s,%s\r\n",
              i,
              v_char50,
              EEPROM.get(aliasOffset + i * (MAX_ALIAS_LEN + 1), v_char16));
      out_i2c(emas); yield();
      /*Serial.printf("%u: %s,%s\r\n",
                    i,
                    v_char50,
                    EEPROM.get(aliasOffset + i * (MAX_ALIAS_LEN + 1), v_char16));
        yield();*/
    }
  }
}

//
// Password is set for incoming connections.
// Allow 3 tries or 60 seconds before hanging up.
//
void inPasswordMode() {
  if ( tcpClient.available() ) {
    int c = receiveTcpData();
    switch ( c ) {
      case -1:    // telnet control sequence: no data returned
        break;

      case LF:
      case CR:
        tcpClient.println();
        if ( strcmp(settings.serverPassword, password) ) {
          ++passwordTries;
          password[0] = NUL;
          passwordLen = 0;
          tcpClient.print(F("\r\nPassword: "));
        } else {
          state = ONLINE;
          sendResult(R_CONNECT);
          tcpClient.println(F("Welcome"));
        }
        break;

      case BS:
      case DEL:
        if ( passwordLen ) {
          password[--passwordLen] = NUL;
          tcpClient.print(F("\b \b"));
        }
        break;

      default:
        if ( isprint((char)c) && passwordLen < MAX_PWD_LEN ) {
          tcpClient.print('*');
          password[passwordLen++] = (char)c;
          password[passwordLen] = 0;
        }
        break;
    }
  }
  if ( millis() - connectTime > PASSWORD_TIME || passwordTries >= PASSWORD_TRIES ) {
    tcpClient.println(F("Good-bye"));
    endCall();
  } else if ( !tcpClient.connected() ) {  // no client?
    endCall();                           // then hang up
  }
}

//
// Paged text output: using the terminal rows defined in
// settings.height, these routines pause the output when
// a screen full of text has been shown.
//
// Call with PagedOut("text", true); to initialise the
// line counter.
//
static uint8_t numLines = 0;

static bool PagedOut(char *str, bool reset = false) {
  char c = ' ';

  if ( reset ) {
    numLines = 0;
  }
  if ( numLines >= settings.height - 1 ) {
    out_i2c_strln("[More]");
    //Serial.print(F("[More]"));

    //while ( !Serial.available() );
    //c = Serial.read();
    if (i2cinbuflen > 0) {
      c = GetFromInBuf();
      i2cinbuflen = 0;
    }
    out_i2c_strln("\r      \r");
    //Serial.print(F("\r      \r"));
    numLines = 0;
  }
  if ( c != CTLC ) {
    out_i2c_ln(str);
    //Serial.println(str);
    yield();
    ++numLines;
  }
  return c == CTLC;
}

static bool PagedOut(const __FlashStringHelper *flashStr, bool reset = false) {
  char str[80];

  strncpy_P(str, (PGM_P)flashStr, sizeof str);
  str[(sizeof str) - 1] = 0;
  return PagedOut(str, reset);
}

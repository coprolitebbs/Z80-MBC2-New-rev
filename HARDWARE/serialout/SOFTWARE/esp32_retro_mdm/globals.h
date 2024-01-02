#ifndef _GLOBALS_H
   #define _GLOBALS_H

   /*int mR_OK = 0;
   int R_CONNECT = 1;
   int R_RING = 2;
   int R_NO_CARRIER = 3;
   int R_ERROR = 4;
   int R_NO_ANSWER = 5;
   int R_RING_IP = 6;*/


   

   // globals
   const char okStr[] PROGMEM = {"OK"};
   const char connectStr[] PROGMEM = {"CONNECT"};
   const char ringStr[] PROGMEM = {"RING"};
   const char noCarrierStr[] PROGMEM = {"NO CARRIER"};
   const char errorStr[] PROGMEM = {"ERROR"};
   const char noAnswerStr[] PROGMEM = {"NO ANSWER"};
   const char httpsErrH[] PROGMEM = {"HTTPS HOST ERR"};
   const char httpsSSLErr[] PROGMEM = {"HTTPS NOT CONN"};
   const char httpsSSLConn[] PROGMEM = {"HTTPS CONN"};
   const char httpsSSLErr2[] PROGMEM = {"HTTPS NOT CONN DL"};
   enum ResultCodes { mR_OK, R_CONNECT, R_RING, R_NO_CARRIER, R_ERROR, R_NO_ANSWER, R_RING_IP, R_ERROR_IN_HOST_STR, R_NO_CARRIER_SSL, R_CONNECT_SSL, R_NO_CARRIER_SSL2 };
   const char * const resultCodes[] PROGMEM = { okStr, connectStr, ringStr, noCarrierStr, errorStr, noAnswerStr, ringStr};
   //enum ResultCodes {  R_CONNECT, R_ERROR, R_RING, R_NO_CARRIER, R_OK, R_NO_ANSWER, R_RING_IP };
   //const char * const resultCodes[] PROGMEM = { okStr, connectStr, ringStr, noCarrierStr, okStr, noAnswerStr, ringStr};

   WiFiClient tcpClient;
   WiFiClientSecure httpsClient;
   uint32_t bytesIn = 0, bytesOut = 0;
   unsigned long connectTime = 0;

   WiFiServer tcpServer(0);

   struct Settings {
      uint16_t  magicNumber;
      char      ssid[MAX_SSID_LEN + 1];
      char      wifiPassword[MAX_WIFI_PWD_LEN + 1];
      uint32_t  serialSpeed;
      uint8_t   dataBits;
      char      parity;
      uint8_t   stopBits;
      bool      rtsCts;
      uint8_t   width, height;
      char      escChar;
      char      alias[SPEED_DIAL_SLOTS][MAX_ALIAS_LEN + 1];
      char      speedDial[SPEED_DIAL_SLOTS][MAX_SPEED_DIAL_LEN + 1];
      char      mdnsName[MAX_MDNSNAME_LEN + 1];
      uint8_t   autoAnswer;
      uint16_t  listenPort;
      char      busyMsg[MAX_BUSYMSG_LEN + 1];
      char      serverPassword[MAX_PWD_LEN + 1];
      bool      echo;
      uint8_t   telnet;
      char      autoExecute[MAX_AUTOEXEC_LEN + 1];
      char      terminal[MAX_TERMINAL_LEN + 1];
      char      location[MAX_LOCATION_LEN + 1];
      bool      startupWait;
      bool      extendedCodes;
      bool      verbose;
      bool      quiet;
   } settings;

   char atCmd[MAX_CMD_LEN + 1], lastCmd[MAX_CMD_LEN + 1];
   unsigned atCmdLen = 0;

   enum {CMD_NOT_IN_CALL, CMD_IN_CALL, ONLINE, PASSWORD} state = CMD_NOT_IN_CALL;
   
   bool secure_connected = false;
  
   bool     ringing = false;     // no incoming call
   uint8_t  ringCount = 0;       // current incoming call ring count
   uint32_t nextRingMs = 0;      // time of mext RING result
   uint8_t  escCount = 0;        // Go to AT mode at "+++" sequence, that has to be counted
   uint32_t guardTime = 0;       // When did we last receive a "+++" sequence
   char     password[MAX_PWD_LEN + 1];
   uint8_t  passwordTries = 0;   // # of unsuccessful tries at incoming password
   uint8_t  passwordLen = 0;
   uint8_t  txBuf[TX_BUF_SIZE];  // Transmit Buffer
   uint8_t  sessionTelnetType;

   char i2coutbuf[I2CBUFFLEN]; 
   uint32_t i2coutbuflen = 0;
   char i2cinbuf[I2CBUFFLEN]; 
   uint32_t i2cinbuflen = 0;

   char endl[]={'\r','\n'};
   char emas[255];
  
   bool StopFlag = false;
   bool StopFlag2 = false;
   bool StopFlag3 = false;
   bool getStatusMode=false; 



   
#endif

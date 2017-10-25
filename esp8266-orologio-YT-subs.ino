/*
   Autore: AT Lab
   Descrizione: Sveglia/contatore iscritti, termometro per stanza realizzato con l'esp8266
   Link: (link video youtube)

   CONNECTIONS:
   pin 15/D8 -> MAX7219 DataIn
   pin 13/D7 -> MAX7219 LOAD/CS
   pin 12/D6 -> MAX7219 CLK
   pin D1 - bottone (non so dove lo avete attaccato)
   pin 4 - audio to base via 220ohm, emiter to GND, speaker to emiter and resistor 10ohm to VCC
   pin A0 - NTC e LUX sensori a 3.3V
*/

// NTP e orologio
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// YT api
#include <YoutubeApi.h>
#define API_YT_REFRESH  5000       /* Frequency in second of reading */
#define API_KEY         ""
#define CHANNEL_ID      ""

//Blynk
#include <BlynkSimpleEsp8266.h>
#define BLYNK_PRINT Serial

// Openweathermap
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// drive display
#include "max7219.h"
#include "audio.h"

#define DEBUG_SERIAL
// #define DEBUG_SETUP
#define DEBUG_TEMP
// #define DEBUG_LUX

// Numero di display
#define NUM_MAX       4
// MAX7219 matrices pins
#define DIN_PIN 15 // D8
#define CS_PIN  13 // D7
#define CLK_PIN 12 // D6
#define PIN_SENSORS 14 // D5

#define ANALOG_PIN A0

#define UPDATE_NTP  1000*60 // 1 ora

#define NIGHT_START   22
#define NIGHT_END     6

// Display mode
#define CLOCKBIG    1
#define CLOCKMED    2
#define CLOCK       3
#define DATE        4
#define DATEFULL    5
#define DATEMONTH   6
#define TEMP        7
#define CLOCKBIGJMP 8
#define SPECIALDAY  9
#define ALARM      10
#define YTSUBS     11
#define STRING_SUB 12

#define SVEGLIA     14
#define DISPMAX     15

// CREDENZIALI d'accesso
// Wi-Fi credenziali
const char ssid[] = "";  //  your network SSID (name)
const char pass[] = "";       // your network password

// Blynk api auth
const char auth[] = "";

// Inizializzazione wifi, MAX7219, time class, ecc
WiFiUDP Udp;

WiFiClientSecure client;
YoutubeApi YTApi(API_KEY, client);

time_t getNtpTime();
time_t prevTimeData = 0; // when the digital clock was displayed

// creazione classe display max7219
MAX7219 max7219(NUM_MAX, DIN_PIN, CS_PIN, CLK_PIN);


// NTP Servers:
static const char ntpServerName[] = "2.it.pool.ntp.org";
unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
const int timeZone = 2;     // Central European Time
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

// Lux sensors mean
const uint8_t luxNumReadings = 50;
int luxReadings[luxNumReadings];      // the readings from the analog input
uint8_t readIndexLux = 0;              // the index of the current reading
int luxTotal = 0;                  // the running total
uint8_t luxAverage = 0;                // the luxAverage

// YT
long YTSubs = 0;
long api_lasttime;  /* last time api request has been done */


// Ora di default in caso di fallimento NTP
int hour_local = 0, minute_local = 0, second_local = 0;
int year_local = 2017, month_local = 10, day_local = 20, dayOfWeek = 1;

// Variabili per l'orologio e le animazioni
int alarmCnt = 0;
int pos = 8;
int cnt = -1;
int h_tens, h_unity, m_tens, m_unity, s_tens, s_unity, secFr, lastSec = -1, lastDay = -1;
int d_tens, d_unity, mn_tens, mn_unity, y_tens, y_unity, dw;
int currMode = 0, prevMode = 0;
int stx = 1;
int sty = 1;
int st = 1;
int disp = 1, prevDisp = 1;
int tr1 = 0, tr2 = 0;
int trdisp1 = 1, trdisp2 = 1;
int trans = 0, prevTrans = 0;
int dots = 0;
int del = 40;
int commandMode = 0;
int charCnt = 0;
uint32_t startTime, diffTime, zeroTime;
char charBuf[7];

// Allarme di default
int hourAlarm = -1, minuteAlarm = -1;
int alarmtoday = 0;

// YT iscritti divisi in singole cifre per mostrarli con un font custom
int  YTSubs_unita;
int  YTSubs_decine;
int  YTSubs_centinaia;
int  YTSubs_migliaia;
int  YTSubs_dmigliaia;

// Temperatura ottenuta da OpenWeatherMap
float temp_owm;
float temp = 20;

// Oridine con cui mostrare i messaggi sul display, 3 sec per step
// Array di 20 per riempire un intero minuto

byte dispTab[20] = {
  CLOCKMED, CLOCKMED, CLOCKMED, CLOCKMED,
  DATE, DATE, DATE, DATE, DATE,
  STRING_SUB, YTSUBS, YTSUBS, YTSUBS, YTSUBS,
  DATEMONTH, DATEMONTH, DATEMONTH,
  TEMP, TEMP, TEMP
};


// Function header
// Lettura sensori
int readTherm();
int readLux();

// Aggiorna il conteggio degli iscritti
void UpdateSubs();

// Legge la temperatura da OpenWeatherMap
void readThermOWM();

// Cosa scrivere sul display
int ascii2int(char *buf);
void render(int displayMode);
void stringSub();
void setClock();
void autoDisp();
void showClockBig(int jump = 0);
void showClockMed();
void showClock();
void showDate();
void showDateFull();
void showDateMonth();
void showTemp();
void showYTSubs();
void sendNTPpacket(IPAddress &address);

// Funzioni di Blynk
void accendiLed(int n);
void spegniLed(int n);


// ----------------------------------
void setup() {
#if defined(DEBUG_SERIAL) || defined(DEBUG_SETUP) || defined(DEBUG_TEMP) || defined(DEBUG_LUX)
  Serial.begin (9600);
#endif
  pinMode(PIN_SENSORS, OUTPUT);

  for (int thisReading = 0; thisReading < luxNumReadings; thisReading++) {
    luxReadings[thisReading] = 0;
  }

  max7219.initMAX7219();
  max7219.clr();
  max7219.refreshAll();
  max7219.sendCmdAll(CMD_SHUTDOWN, 1);
  max7219.sendCmdAll(CMD_INTENSITY, 0);
  max7219.showString(0, (char*)"WiFi...");
  max7219.refreshAll();

  Blynk.begin(auth, ssid, pass);

  Udp.begin(localPort);

  setSyncProvider(updateNtpTime);
  setSyncInterval(UPDATE_NTP);
  max7219.clr();
  max7219.refreshAll();
  while (now() != prevTimeData) {
    prevTimeData = now();
    max7219.showString(0, (char*)"Clock...");
    max7219.refreshAll();
    delay(100);
  }
}


void loop() {

  Blynk.run();
  BLYNK_WRITE(V1);
  BLYNK_READ(V2);
  BLYNK_READ(V3);

  startTime = millis();

  // legge la luminosita e ottiene la media delle misurazioni precedenti
  luxTotal = luxTotal - luxReadings[readIndexLux];
  luxReadings[readIndexLux] = readLux();
  luxTotal = luxTotal + luxReadings[readIndexLux];
  readIndexLux++;
  if (readIndexLux >= luxNumReadings) {
    readIndexLux = 0;
  }
  luxAverage = luxTotal / luxNumReadings;
  max7219.sendCmdAll(CMD_INTENSITY, luxAverage);

  second_local  = second();
  minute_local  = minute();
  hour_local   = hour();
  day_local     = day();
  month_local   = month();
  year_local    = year();
  h_tens = hour() / 10;
  h_unity = hour() % 10;
  m_tens = minute() / 10;
  m_unity = minute() % 10;
  s_tens = second() / 10;
  s_unity = second() % 10;
  d_tens = day() / 10;
  d_unity = day() % 10;
  mn_tens = month() / 10;
  mn_unity = month() % 10;
  y_tens = (year() - 2000) / 10;
  y_unity = (year() - 2000) % 10;
  dw = weekday() - 1; // dw=0..6, dayOfWeek=1..7

  if (second_local != lastSec) {
    lastSec = second_local;
    secFr = 0;
  } else
    secFr++;

  if (hour_local == 0 && minute_local == 0 && second_local == 5 && lastDay != day_local) {
    lastDay = day_local;
    second_local = 0;
  }

  if (cnt < 0) cnt = second_local * 10;
  if (secFr == 0) cnt = 0;
  dots = (cnt % 40 < 20) ? 1 : 0;

  if (hour_local == hourAlarm && minute_local == minuteAlarm && second_local == 0 && alarmtoday == 1) {
#ifdef DEBUG_SERIAL
    Serial.print("ALARM CHIAMATA");
#endif
    alarmCnt = 1;
    disp = CLOCKBIG;
  }

  prevDisp = disp;
  switch (currMode) {
    case 0: autoDisp(); break;
    default: disp = currMode; break;
  }

  max7219.clr();
  if (disp != prevDisp) {
    trans = 1 + (prevTrans % 4);
    prevTrans = trans;
    switch (trans) {
      case 1:  tr1 = 0; tr2 = -38; st = +1; break;
      case 2:  tr1 = 0; tr2 =  38; st = -1; break;
      case 3:  tr1 = 0; tr2 = -11 << 1; st = +1; break;
      case 4:  tr1 = 0; tr2 =  11 << 1; st = -1; break;
    }
    trdisp1 = prevDisp;
    trdisp2 = disp;
    if (prevDisp == CLOCKBIGJMP || disp == CLOCKBIGJMP) {
      trans = max7219.delta_x = max7219.delta_y = 0;
    }
  }

  if (!trans) {
    render(disp);
  } else {
    if (trans == 1 || trans == 2) max7219.delta_x = tr1; else max7219.delta_y = tr1 >> 1;
    render(trdisp1);
    if (trans == 1 || trans == 2) max7219.delta_x = tr2; else max7219.delta_y = tr2 >> 1;
    render(trdisp2);
    tr1 += st;
    tr2 += st;
    if (tr2 == 0) trans = max7219.delta_x = max7219.delta_y = 0;
  }

  max7219.refreshAll();

  cnt++;
  while (millis() - startTime < 25);

  playAlarm();
  //readTherm();

  // Aggiorna temperatura OWM ogni minuto
//  if ((millis() - startTime) / (1000 * 60) ) {
//    readThermOWM();
//  }

}

// Aggiorno iscritti YouTube
void UpdateSubs() {
  if (millis() > api_lasttime + API_YT_REFRESH)  {
    if (YTApi.getChannelStatistics(CHANNEL_ID)) {
      YTSubs = YTApi.channelStats.subscriberCount;
    }
    api_lasttime = millis();
  }
}

// Lettura luce
int readLux() {
  digitalWrite(PIN_SENSORS, HIGH);
  double Vcc = 5.0;
  unsigned int Rs = 47000;
  double V_LUX = (double)analogRead(ANALOG_PIN) / 1024;
  double R_LUX = (Rs * V_LUX) / (Vcc - V_LUX);
  // int brightness = (int)R_LUX / 10;
#ifdef DEBUG_LUX
  Serial.print("R_LUX: ");
  Serial.print((long)R_LUX);
#endif
  int brightness = map(R_LUX, 100, 5000, 15 , 0);
#ifdef DEBUG_LUX
  Serial.print("\tbrightness: ");
  Serial.println(brightness);
#endif
  if (brightness > 15) brightness = 15;
  if (brightness < 0) brightness = 0;

  return brightness;
}

// Lettura temperatura
int readTherm() {
  digitalWrite(PIN_SENSORS, LOW);
  delay(10);

  // Temperature negative non vengono visualizzate
  double Vcc = 3;
  unsigned int Rs = 48000;
  double V_NTC = (double)analogRead(ANALOG_PIN) / 1024;
  double R_NTC = (Rs * V_NTC) / (Vcc - V_NTC);
#ifdef DEBUG_TEMP
  Serial.print("R_NTC: ");
  Serial.println(R_NTC);
#endif
  R_NTC = log(R_NTC);
  double temp_ntc = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * R_NTC * R_NTC )) * R_NTC );
  temp_ntc -= 273.15;
#ifdef DEBUG_TEMP
  Serial.print("Temperatura: ");
  Serial.println(temp_ntc);
  Serial.println((int)(temp_ntc*100));
#endif
  return temp_ntc;
}

// Aggiorno l'ora
time_t updateNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
#ifdef DEBUG_SERIAL
  Serial.println("Transmit NTP Request");
#endif
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);

#ifdef DEBUG_SERIAL
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
#endif

  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {

#ifdef DEBUG_SERIAL
      Serial.println("Receive NTP Response");
#endif

      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }

#ifdef DEBUG_SERIAL
  Serial.println("No NTP Response :-(");
#endif

  return 0; // return 0 if unable to get the time
}

// manda una richiesta NTP al server impostato
void sendNTPpacket(IPAddress &address) {
  // tutti i byte impostati a 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// chiama le funzioni corrispondenti per mostrare i vari dati sui display
void render(int displayMode) {
  switch (displayMode) {
    case CLOCKBIG:      showClockBig();   break;
    case CLOCKBIGJMP:   showClockBig(1);  break;
    case CLOCKMED:      showClockMed();   break;
    case CLOCK:         showClock();      break;
    case DATE:          showDate();       break;
    case DATEFULL:      showDateFull();   break;
    case DATEMONTH:     showDateMonth();  break;
    case TEMP:          showTemp();       break;
    case YTSUBS:        showYTSubs();     break;
    case STRING_SUB:    stringSub();      break;
    default:            showClockMed();   break;
  }
}

// Funzioni per mostrare del contenuto sul display
// Stringa prima degli iscritti di YouTube
void stringSub() {
  String str = "YTSubs";
  max7219.showString(0, (char*)str.c_str());
}

// Convertire un char in int
int ascii2int(char *buf) {
  return (buf[0] & 0xf) * 10 + (buf[1] & 0x0f);
}

// questa non viene mai chiamata, era per settare l'ora da seriale?
void setClock() {
  int m = toupper(charBuf[0]);
  if (m != 'T' && m != 'D' && m != 'W' && m != 'A') return;
  switch (m) {
    case 'T':
      hour_local   = ascii2int(charBuf + 1);
      minute_local = ascii2int(charBuf + 3);
      second_local = ascii2int(charBuf + 5);
      break;
    case 'D':
      day_local   = ascii2int(charBuf + 1);
      month_local = ascii2int(charBuf + 3);
      year_local  = ascii2int(charBuf + 5) + 2000;
      break;
    case 'W':
      dayOfWeek = charBuf[1] & 0xf; break;
    case 'A':
      hourAlarm   = ascii2int(charBuf + 1);
      // come mai qui non hai usato ascii2int?
      minuteAlarm = (charBuf[3] & 0xf) * 10 + (charBuf[4] & 0x0f);
      //minuteAlarm   = ascii2int(charBuf+3);
      break;
  }
}

// Funzioni per mostrare info sul display
void autoDisp() {
  // seconds 0-59 -> 0-19, 3s steps
  disp = dispTab[second_local / 3];
#ifdef DEBUG_SERIAL
  Serial.println("disp: " + disp);
#endif
}

//6+2+6+3+6+2+6 = 31 pixel
void showClockBig(int jump) {
  if (jump && !trans) {
    max7219.delta_x += stx; if (max7219.delta_x > 25 || max7219.delta_x < -25) stx = -stx;
    max7219.delta_y += sty; if (max7219.delta_y > 6 || max7219.delta_y < -6) sty = -sty;
    delay(40); // ugly!
  }
  if (h_tens > 0) max7219.showDigit(h_tens, h_tens == 2 ? 1 : 2, dig4x8); //??
  max7219.showDigit(h_unity, 8, dig6x8);
  max7219.showDigit(m_tens, 17, dig6x8);
  max7219.showDigit(m_unity, 24, dig6x8);
  max7219.setColumn(15, dots ? 0x24 : 0);
}

// 5+1+5+3+5+1+5+ 1+3+1+3=33 pixel
void showClockMed() {
  if (h_tens > 0) max7219.showDigit(h_tens, 0, dig5x8rn);
  max7219.showDigit(h_unity, h_tens == 2 ? 6 : 5, dig5x8rn); // <20h display 1 pixel earlier for better looking dots
  max7219.showDigit(m_tens, 13, dig5x8rn);
  max7219.showDigit(m_unity, 19, dig5x8rn);
  max7219.showDigit(s_tens, 25, dig3x6);
  max7219.showDigit(s_unity, 29, dig3x6);
  max7219.setColumn((hour_local == 20) ? 12 : 11, dots ? 0x24 : 0); // 20:xx - dots 1 pixel later
}

// 4+1+4+3+4+1+4=21 + 3+1+3 pixel
void showClock() {
  if (h_tens > 0) max7219.showDigit(h_tens, h_tens == 2 ? 0 : 1, dig4x8);
  max7219.showDigit(h_unity, 5, dig4x8);
  max7219.showDigit(m_tens, 13, dig4x8);
  max7219.showDigit(m_unity, 18, dig4x8);
  max7219.showDigit(s_tens, 25, dig3x6);
  max7219.showDigit(s_unity, 29, dig3x6);
  max7219.setColumn(11, dots ? 0x24 : 0);
}

// 4+1+4+2+4+1+4+2+4+1+4 = 31 pixel
//  [ 4. 1.16]
void showDate() {
  if (d_tens) max7219.showDigit(d_tens, 0, dig4x8);
  max7219.showDigit(d_unity, 5, dig4x8);
  if (mn_tens) max7219.showDigit(mn_tens, 11, dig4x8);
  max7219.showDigit(mn_unity, 16, dig4x8);
  max7219.showDigit(y_tens, 22, dig4x8);
  max7219.showDigit(y_unity, 27, dig4x8);
  max7219.setColumn(10, 0x80);
  max7219.setColumn(21, 0x80);
}

//  [ 4. 1.2016]
void showDateFull() {
  if (d_tens) max7219.showDigit(d_tens, 0, dig3x8);
  max7219.showDigit(d_unity, 4, dig3x8);
  if (mn_tens) max7219.showDigit(mn_tens, 9, dig3x8);
  max7219.showDigit(mn_unity, 12, dig3x8);
  max7219.showDigit(2, 18, dig3x8);
  max7219.showDigit(0, 22, dig3x8);
  max7219.showDigit(y_tens, 26, dig3x8);
  max7219.showDigit(y_unity, 29, dig3x8);
  max7219.setColumn(8, 0x80);
  max7219.setColumn(16, 0x80);
}

// 4+1+4+ 3 +4+1+4 = 21 +1 +10 =32
//  [ 4. 1 MO]
void showDateMonth() {
  if (d_tens) max7219.showDigit(d_tens, 0, dig4x8);
  max7219.showDigit(d_unity, 5, dig4x8);
  if (mn_tens) max7219.showDigit(mn_tens, 11, dig4x8);
  max7219.showDigit(mn_unity, 16, dig4x8);
  max7219.showDigit(dw, 22, dweek_en);
  max7219.setColumn(10, 0x80);
}

// mostra gli iscritti di YouTube
void showYTSubs() {
  UpdateSubs();
  
  YTSubs_dmigliaia = YTSubs / 10000;
  YTSubs_migliaia = (YTSubs - (YTSubs_dmigliaia * 10000)) / 1000;
  YTSubs_centinaia = (YTSubs - (YTSubs_dmigliaia * 10000) - (YTSubs_migliaia * 1000) ) / 100;
  YTSubs_decine = (YTSubs - (YTSubs_dmigliaia * 10000) - (YTSubs_migliaia * 1000) - (YTSubs_centinaia * 100) ) / 10;
  YTSubs_unita = YTSubs % 10;
  
  max7219.showDigit(YTSubs_dmigliaia, 1, dig5x8rn);
  max7219.showDigit(YTSubs_migliaia, 7, dig5x8rn);
  max7219.showDigit(YTSubs_centinaia, 13, dig5x8rn);
  max7219.showDigit(YTSubs_decine, 19, dig5x8rn);
  max7219.showDigit(YTSubs_unita, 25, dig5x8rn);
}

// mostra temperatura
// [ xx.x°]
void showTemp() {
  if (secFr == 0) {
    temp = ((temp*0.8) + (readTherm()*0.2) + 0.05);
  }
#ifdef DEBUG_SERIAL
  Serial.print("Temp: ");
  Serial.println(thermAverage);
#endif
  //float temp = (float)(readTherm())/100;//(float)thermAverage / 100;
  if (temp > 0 && temp < 99) {
    int t1 = (int)temp / 10;
    int t0 = (int)temp % 10;
    int tf = (temp - int(temp)) * 10.0;
    if (t1) max7219.showDigit(t1, 2, dig5x8sq);
    max7219.showDigit(t0, 8, dig5x8sq);
    max7219.showDigit(tf, 16, dig5x8sq);
  }
  max7219.setColumn(14, 0x80);
  max7219.showDigit(7, 22, dweek_pl);
}

// Suona la melodia come allarme
void playAlarm() {
  if (alarmCnt > 0) {
    alarmCnt--;
    suonaSuoneria(melody10, noteDurations10, 2000, 1.00, (sizeof(melody10) / sizeof(melody10[0])));
    suonaSuoneria(melody10, noteDurations10, 2000, 1.00, (sizeof(melody10) / sizeof(melody10[0])));
    // da implementare spegnere la sveglia con il bottone
  }
}

// Accendi il led n
void accendiLed (int n) {
  switch (n) {
    case 1: ; Blynk.virtualWrite(V4, 255); break;
    case 2: ; Blynk.virtualWrite(V5, 255); break;
    case 3: ; Blynk.virtualWrite(V6, 255); break;
    case 4: ; Blynk.virtualWrite(V7, 255); break;
    case 5: ; Blynk.virtualWrite(V8, 255); break;
    case 6: ; Blynk.virtualWrite(V9, 255); break;
    case 7: ; Blynk.virtualWrite(V10, 255); break;
  }
}

// Spegne il led n
void spegniLed (int n) {
  switch (n) {
    case 1: ; Blynk.virtualWrite(V4, 0); break;
    case 2: ; Blynk.virtualWrite(V5, 0); break;
    case 3: ; Blynk.virtualWrite(V6, 0); break;
    case 4: ; Blynk.virtualWrite(V7, 0); break;
    case 5: ; Blynk.virtualWrite(V8, 0); break;
    case 6: ; Blynk.virtualWrite(V9, 0); break;
    case 7: ; Blynk.virtualWrite(V10, 0); break;
  }
}

BLYNK_WRITE(V1) {
  TimeInputParam t(param);
  hourAlarm = t.getStartHour();
  minuteAlarm = t.getStartMinute();
#ifdef DEBUG_SERIAL
  Serial.println(hourAlarm);
  Serial.println(minuteAlarm);
  Serial.println();
#endif
  for (int i = 1; i <= 7; i++) {
    if (t.isWeekdaySelected(i)) {
#ifdef DEBUG_SERIAL
      Serial.println(String("Day ") + i + " is selected");
#endif
      accendiLed(i);
    }
    else {
      spegniLed(i);
    }
  }
  if (t.isWeekdaySelected(dw)) {
    alarmtoday = 1;
  }
  else {
    alarmtoday = 0;
  }
}

BLYNK_READ(V2) {
  Blynk.virtualWrite(V2, hourAlarm);
}

BLYNK_READ(V3) {
  Blynk.virtualWrite(V3, minuteAlarm);
}

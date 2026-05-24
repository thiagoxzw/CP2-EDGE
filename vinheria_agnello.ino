/* =====================================================================
 * Projeto Vinheria Agnello - Fase 2 (FIAP)
 * ---------------------------------------------------------------------
 * Funcionalidades:
 *   - Medição de temperatura/umidade (DHT22) e luminosidade (LDR)
 *   - Display LCD 16x2 I2C com logo animado no boot
 *   - RTC DS1307 para timestamp dos logs
 *   - EEPROM para armazenar logs (eventos fora dos triggers) E configurações
 *   - 3 LEDs (verde/amarelo/vermelho) + Buzzer para alertas
 *   - 3 botões (Menu / Up / Down) para setup
 *   - Setup: Unidade (°C/°F), Idioma (PT/EN), Triggers (min/max)
 *   - LDR com calibração automática + função map() + média de 10s
 * =====================================================================
 */

#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Wire.h>
#include <EEPROM.h>
#include "DHT.h"

// ---------------------- DEFINES E PINAGEM ---------------------------
#define LOG_OPTION    1
#define SERIAL_OPTION 1
#define UTC_OFFSET   -3

#define DHTPIN   2
#define DHTTYPE  DHT22

#define LDR_PIN     A0
#define LED_VERDE   3
#define LED_AMARELO 4
#define LED_VERMELHO 5
#define BUZZER_PIN  6

#define BTN_MENU 7
#define BTN_UP   8
#define BTN_DOWN 9

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 RTC;

// ---------------------- EEPROM: layout -------------------------------
// 0..49  -> configurações persistentes
// 50..849 -> logs (100 registros de 8 bytes)
const int CONFIG_ADDR = 0;
const int LOG_START   = 50;
const int LOG_END     = LOG_START + 100 * 8;
const int RECORD_SIZE = 8;
int currentAddress    = LOG_START;

// ---------------------- ESTRUTURA DE CONFIG --------------------------
struct Config {
  byte magic;          // 0xAC -> indica EEPROM já inicializada
  byte unidade;        // 0 = Celsius, 1 = Fahrenheit
  byte idioma;         // 0 = PT, 1 = EN
  float t_min;
  float t_max;
  float u_min;
  float u_max;
  int   l_min;         // % luminosidade
  int   l_max;
};
Config cfg;

// ---------------------- ESTADO GERAL ---------------------------------
int lastLoggedMinute = -1;

// LDR - calibração e média
int ldrMin = 1023, ldrMax = 0;
const int LDR_BUF = 10;
int  ldrBuffer[LDR_BUF];
int  ldrIdx = 0;
bool ldrBufFull = false;
unsigned long lastLdrRead = 0;

// Botões (debounce simples)
unsigned long lastBtnPress = 0;
const unsigned long DEBOUNCE_MS = 200;

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  dht.begin();
  lcd.init();
  lcd.backlight();
  RTC.begin();
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  EEPROM.begin();

  loadConfig();          // carrega ou inicializa configurações
  criarCaracteresLogo(); // registra caracteres customizados
  mostrarLogoAnimado();  // animação de boot
  calibrarLDR();         // calibração automática inicial
}

// =====================================================================
// LOOP
// =====================================================================
void loop() {
  // 1) Botões - se Menu for pressionado, entra no setup
  if (digitalRead(BTN_MENU) == LOW && millis() - lastBtnPress > DEBOUNCE_MS) {
    lastBtnPress = millis();
    entrarMenuSetup();
  }

  // 2) Tempo atual ajustado por UTC
  DateTime now = RTC.now();
  long offsetSeconds = (long)UTC_OFFSET * 3600L;
  DateTime adjustedTime = DateTime(now.unixtime() + offsetSeconds);

  if (LOG_OPTION) {
    // chamada manual via Serial para imprimir log: descomente se quiser
    // get_log();
  }

  // 3) Leitura do LDR a cada 1s alimenta o buffer (média de 10s)
  if (millis() - lastLdrRead >= 1000) {
    lastLdrRead = millis();
    int leitura = analogRead(LDR_PIN);

    // recalibração dinâmica suave
    if (leitura < ldrMin) ldrMin = leitura;
    if (leitura > ldrMax) ldrMax = leitura;

    ldrBuffer[ldrIdx++] = leitura;
    if (ldrIdx >= LDR_BUF) { ldrIdx = 0; ldrBufFull = true; }
  }

  int luzPercent = calcularLuminosidadePct();
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  float temperatureDisplay = (cfg.unidade == 0) ? temperatureC
                                                : (temperatureC * 9.0 / 5.0 + 32.0);

  // 4) Sinalização (LEDs + buzzer)
  bool tempCritica = (temperatureC < cfg.t_min || temperatureC > cfg.t_max);
  bool umidCritica = (humidity     < cfg.u_min || humidity     > cfg.u_max);
  bool luzCritica  = (luzPercent   < cfg.l_min || luzPercent   > cfg.l_max);
  atualizarAlertas(tempCritica, umidCritica, luzCritica);

  // 5) Gravar log na EEPROM quando passa de minuto E há condição crítica
  if (adjustedTime.minute() != lastLoggedMinute) {
    lastLoggedMinute = adjustedTime.minute();

    if (tempCritica || umidCritica || luzCritica) {
      int tempInt = (int)(temperatureC * 100);
      int humiInt = (int)(humidity * 100);
      unsigned long ts = adjustedTime.unixtime();
      EEPROM.put(currentAddress,     ts);
      EEPROM.put(currentAddress + 4, tempInt);
      EEPROM.put(currentAddress + 6, humiInt);
      getNextAddress();
    }
  }

  // 6) Serial debug
  if (SERIAL_OPTION) {
    Serial.print(adjustedTime.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.print(" | T="); Serial.print(temperatureDisplay);
    Serial.print(cfg.unidade == 0 ? "C" : "F");
    Serial.print(" U="); Serial.print(humidity); Serial.print("%");
    Serial.print(" L="); Serial.print(luzPercent); Serial.println("%");
  }

  // 7) LCD - alterna info a cada 3s
  exibirNoLCD(adjustedTime, temperatureDisplay, humidity, luzPercent);

  delay(500);
}

// =====================================================================
// FUNÇÕES - CONFIG / EEPROM
// =====================================================================
void loadConfig() {
  EEPROM.get(CONFIG_ADDR, cfg);
  if (cfg.magic != 0xAC) {
    // primeira execução -> grava defaults
    cfg.magic    = 0xAC;
    cfg.unidade  = 0;        // Celsius
    cfg.idioma   = 0;        // PT
    cfg.t_min    = 10.0;
    cfg.t_max    = 16.0;     // vinho: ideal ~13C, variacao de ate +-3C
    cfg.u_min    = 60.0;     // umidade ideal: faixa 60-80%
    cfg.u_max    = 80.0;
    cfg.l_min    = 0;
    cfg.l_max    = 30;       // vinho prefere ambiente em penumbra
    saveConfig();
  }
}
void saveConfig() { EEPROM.put(CONFIG_ADDR, cfg); }

void getNextAddress() {
  currentAddress += RECORD_SIZE;
  if (currentAddress >= LOG_END) currentAddress = LOG_START;
}

void get_log() {
  Serial.println(F("Timestamp\t\tTemp\tUmid"));
  for (int addr = LOG_START; addr < LOG_END; addr += RECORD_SIZE) {
    unsigned long ts; int tempInt, humiInt;
    EEPROM.get(addr, ts);
    EEPROM.get(addr + 4, tempInt);
    EEPROM.get(addr + 6, humiInt);
    if (ts != 0xFFFFFFFF && ts != 0) {
      DateTime dt(ts);
      Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
      Serial.print("\t"); Serial.print(tempInt / 100.0);
      Serial.print(" C\t"); Serial.print(humiInt / 100.0);
      Serial.println(" %");
    }
  }
}

// =====================================================================
// FUNÇÕES - LDR
// =====================================================================
void calibrarLDR() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(cfg.idioma == 0 ? "Calibrando LDR" : "Calibrating LDR");
  lcd.setCursor(0, 1);
  lcd.print(cfg.idioma == 0 ? "Aguarde 5s..."  : "Wait 5s...");
  unsigned long t0 = millis();
  while (millis() - t0 < 5000) {
    int v = analogRead(LDR_PIN);
    if (v < ldrMin) ldrMin = v;
    if (v > ldrMax) ldrMax = v;
    delay(50);
  }
  // garante uma faixa mínima para evitar divisão por zero no map()
  if (ldrMax - ldrMin < 50) { ldrMin = 0; ldrMax = 1023; }
}

int calcularLuminosidadePct() {
  int n = ldrBufFull ? LDR_BUF : ldrIdx;
  if (n == 0) return 0;
  long soma = 0;
  for (int i = 0; i < n; i++) soma += ldrBuffer[i];
  int media = soma / n;
  int pct = map(media, ldrMin, ldrMax, 0, 100);
  return constrain(pct, 0, 100);
}

// =====================================================================
// FUNÇÕES - ALERTAS
// =====================================================================
void atualizarAlertas(bool tCrit, bool uCrit, bool lCrit) {
  int criticos = (int)tCrit + (int)uCrit + (int)lCrit;
  digitalWrite(LED_VERDE,    criticos == 0);
  digitalWrite(LED_AMARELO,  criticos == 1);
  digitalWrite(LED_VERMELHO, criticos >= 2);
  if (criticos >= 2) tone(BUZZER_PIN, 1000, 200);
  else noTone(BUZZER_PIN);
}

// =====================================================================
// FUNÇÕES - LCD
// =====================================================================
unsigned long ultimaTroca = 0;
byte modoTela = 0; // 0 = data/hora, 1 = temp/umid, 2 = luz

void exibirNoTela0(DateTime t) {
  lcd.setCursor(0, 0);
  lcd.print(cfg.idioma == 0 ? "DATA: " : "DATE: ");
  if (t.day() < 10) lcd.print('0'); lcd.print(t.day()); lcd.print('/');
  if (t.month() < 10) lcd.print('0'); lcd.print(t.month()); lcd.print('/');
  lcd.print(t.year());
  lcd.setCursor(0, 1);
  lcd.print(cfg.idioma == 0 ? "HORA: " : "TIME: ");
  if (t.hour() < 10) lcd.print('0'); lcd.print(t.hour()); lcd.print(':');
  if (t.minute() < 10) lcd.print('0'); lcd.print(t.minute()); lcd.print(':');
  if (t.second() < 10) lcd.print('0'); lcd.print(t.second());
  lcd.print("   ");
}

void exibirNoTela1(float temp, float umid) {
  lcd.setCursor(0, 0);
  lcd.print(cfg.idioma == 0 ? "Temp: " : "Temp: ");
  lcd.print(temp, 1); lcd.print(cfg.unidade == 0 ? (char)223 : (char)223);
  lcd.print(cfg.unidade == 0 ? "C  " : "F  ");
  lcd.setCursor(0, 1);
  lcd.print(cfg.idioma == 0 ? "Umid: " : "Hum:  ");
  lcd.print(umid, 1); lcd.print(" %   ");
}

void exibirNoTela2(int luzPct) {
  lcd.setCursor(0, 0);
  lcd.print(cfg.idioma == 0 ? "Luminosidade:" : "Light level:");
  lcd.setCursor(0, 1);
  lcd.print(luzPct); lcd.print(" %         ");
}

void exibirNoLCD(DateTime t, float temp, float umid, int luzPct) {
  if (millis() - ultimaTroca > 3000) {
    ultimaTroca = millis();
    modoTela = (modoTela + 1) % 3;
    lcd.clear();
  }
  if      (modoTela == 0) exibirNoTela0(t);
  else if (modoTela == 1) exibirNoTela1(temp, umid);
  else                    exibirNoTela2(luzPct);
}

// =====================================================================
// FUNÇÕES - LOGO ANIMADO
// =====================================================================
// 8 caracteres customizados desenhando taça enchendo
byte taca0[8] = {B11111,B10001,B10001,B01010,B00100,B00100,B00100,B11111};
byte taca1[8] = {B11111,B10001,B11111,B01110,B00100,B00100,B00100,B11111};
byte taca2[8] = {B11111,B11111,B11111,B01110,B00100,B00100,B00100,B11111};
byte uva[8]   = {B00000,B01110,B11111,B11111,B11111,B01110,B00100,B00000};

void criarCaracteresLogo() {
  lcd.createChar(0, taca0);
  lcd.createChar(1, taca1);
  lcd.createChar(2, taca2);
  lcd.createChar(3, uva);
}

void mostrarLogoAnimado() {
  lcd.clear();
  lcd.setCursor(2, 0); lcd.print("VINHERIA");
  lcd.setCursor(3, 1); lcd.print("AGNELLO ");
  delay(700);
  for (byte fase = 0; fase < 3; fase++) {
    lcd.setCursor(13, 1); lcd.write(fase);
    lcd.setCursor(15, 1); lcd.write((byte)3);
    delay(500);
  }
  delay(800);
  lcd.clear();
}

// =====================================================================
// FUNÇÕES - MENU DE SETUP
// =====================================================================
// Itens do menu:
//   0 Unidade  1 Idioma  2 Tmin  3 Tmax  4 Umin  5 Umax  6 Lmin  7 Lmax  8 Sair
void entrarMenuSetup() {
  byte item = 0;
  const byte TOTAL = 9;
  bool sair = false;

  // estados anteriores dos botoes (para detectar a borda HIGH->LOW)
  bool prevMenu = HIGH, prevUp = HIGH, prevDown = HIGH;

  // espera soltar o botao que abriu o menu, senao ele dispara de imediato
  while (digitalRead(BTN_MENU) == LOW) delay(10);
  delay(50);

  desenharItemMenu(item);

  while (!sair) {
    bool menu = digitalRead(BTN_MENU);
    bool up   = digitalRead(BTN_UP);
    bool down = digitalRead(BTN_DOWN);

    // MENU: so age na transicao "acabou de pressionar"
    if (menu == LOW && prevMenu == HIGH) {
      if (item == 8) {
        sair = true;                 // confirma a saida
      } else {
        item++;
        if (item == 8) desenharTelaSair();
        else           desenharItemMenu(item);
      }
    }

    // UP / DOWN: so ajustam valores nos itens 0..7
    if (up == LOW && prevUp == HIGH && item < 8) {
      ajustarItem(item, +1);
      desenharItemMenu(item);
    }

    if (down == LOW && prevDown == HIGH && item < 8) {
      ajustarItem(item, -1);
      desenharItemMenu(item);
    }

    prevMenu = menu;
    prevUp   = up;
    prevDown = down;

    delay(50);   // debounce simples
  }

  saveConfig();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(cfg.idioma == 0 ? "Salvo!" : "Saved!");
  delay(1000);

  // evita que o loop() reabra o menu com o botao ainda pressionado
  while (digitalRead(BTN_MENU) == LOW) delay(10);
  lastBtnPress = millis();
  lcd.clear();
}

void desenharTelaSair() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(cfg.idioma == 0 ? "Salvar e sair?" : "Save & exit?");
  lcd.setCursor(0,1); lcd.print(cfg.idioma == 0 ? "MENU=sim" : "MENU=yes");
}

void ajustarItem(byte item, int delta) {
  switch (item) {
    case 0: cfg.unidade = (cfg.unidade + 1) % 2; break;
    case 1: cfg.idioma  = (cfg.idioma  + 1) % 2; break;
    // triggers guardados em Celsius; passo de 1 grau na unidade exibida
    case 2: cfg.t_min += (cfg.unidade == 0) ? delta * 1.0 : delta * (5.0 / 9.0); break;
    case 3: cfg.t_max += (cfg.unidade == 0) ? delta * 1.0 : delta * (5.0 / 9.0); break;
    case 4: cfg.u_min += delta * 5.0; break;
    case 5: cfg.u_max += delta * 5.0; break;
    case 6: cfg.l_min += delta * 5;   break;
    case 7: cfg.l_max += delta * 5;   break;
  }
  // sanitizar
  cfg.u_min = constrain(cfg.u_min, 0, 100);
  cfg.u_max = constrain(cfg.u_max, 0, 100);
  cfg.l_min = constrain(cfg.l_min, 0, 100);
  cfg.l_max = constrain(cfg.l_max, 0, 100);
}

void desenharItemMenu(byte item) {
  lcd.clear();
  lcd.setCursor(0,0);
  switch (item) {
    case 0:
      lcd.print(cfg.idioma == 0 ? "Unidade:" : "Unit:");
      lcd.setCursor(0,1); lcd.print(cfg.unidade == 0 ? "Celsius (C)" : "Fahrenheit(F)");
      break;
    case 1:
      lcd.print(cfg.idioma == 0 ? "Idioma:" : "Language:");
      lcd.setCursor(0,1); lcd.print(cfg.idioma == 0 ? "Portugues" : "English");
      break;
    case 2: {
      lcd.print(cfg.unidade == 0 ? "Temp min (C):" : "Temp min (F):");
      lcd.setCursor(0,1);
      float v = (cfg.unidade == 0) ? cfg.t_min : (cfg.t_min * 9.0 / 5.0 + 32.0);
      lcd.print(v, 1);
      break;
    }
    case 3: {
      lcd.print(cfg.unidade == 0 ? "Temp max (C):" : "Temp max (F):");
      lcd.setCursor(0,1);
      float v = (cfg.unidade == 0) ? cfg.t_max : (cfg.t_max * 9.0 / 5.0 + 32.0);
      lcd.print(v, 1);
      break;
    }
    case 4: lcd.print(cfg.idioma==0?"Umid min (%):":"Hum min (%):"); lcd.setCursor(0,1); lcd.print(cfg.u_min,0); break;
    case 5: lcd.print(cfg.idioma==0?"Umid max (%):":"Hum max (%):"); lcd.setCursor(0,1); lcd.print(cfg.u_max,0); break;
    case 6: lcd.print(cfg.idioma==0?"Luz min (%):":"Light min(%):"); lcd.setCursor(0,1); lcd.print(cfg.l_min);  break;
    case 7: lcd.print(cfg.idioma==0?"Luz max (%):":"Light max(%):"); lcd.setCursor(0,1); lcd.print(cfg.l_max);  break;
  }
}

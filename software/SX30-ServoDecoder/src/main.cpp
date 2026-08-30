/*
  SX30 Servo-Decoder (Basisplatine 3.0 + ServoAufsatz/PCA9685)

  Ziel:
  - 16 Weichenservos über SX-Bus schalten
  - 2 SX-Adressen (je 8 Bits) steuern 16 Servos
  - Programmiermodus wie OpenSX-Beispiele (Taste + Track aus)
  - EEPROM speichert:
      * SX-Adresse A/B
      * pro Servo: rel-Min/rel-Max/Nullpunkt
      * pro Servo: Abzweig links/rechts

  Laufzeit-Logik:
  - SX-Bit = 0  => GERADE
  - SX-Bit = 1  => ABZWEIG
  - Ob ABZWEIG links oder rechts ist, wird pro Servo konfiguriert (EEPROM)
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>
#include <SX30.h>

// ---------------- Hardware ----------------
#define PROGLED 13
#define PROGBUTTON A6
#define KEYPRESS (analogRead(PROGBUTTON) > 512)
#define DEBOUNCETIME 200
#define DEBUG_SERIAL 1

// Unfertiger Programmier-Wizard ueber SX-Bus.
// 0 = komplett deaktiviert; serielles Servo-Setup bleibt aktiv.
// 1 = SX-Setup-Wizard kompilieren/aktivieren.
#define ENABLE_SX_SETUP_WIZARD 0

// SX30 ISR auf INT0
void sxisr();
SX30 sx;
Adafruit_PWMServoDriver pwm(0x40);

// ---------------- Servo/PCA9685 ----------------
const uint8_t SERVO_COUNT = 16;
const uint16_t SERVO_MIN_TICK = 110; // bei Bedarf kalibrieren
const uint16_t SERVO_MAX_TICK = 500; // bei Bedarf kalibrieren

const char FW_DECODER_TYPE[] = "servodecoder";
const char FW_VERSION[] = "2026-08-30-Button-S-W";
const char FW_HARDWARE[] = "V1.0 und V4.0 verwenden";
const uint8_t FW_PROTO = 1;

// ---------------- SX Kanalgrenzen ----------------
const uint8_t SX_ADDR_DISABLED = 0;
const uint8_t SX_ADDR_MIN = 1;
const uint8_t SX_ADDR_MAX = 111;

// Programmierkanäle am SX-Bus
// Altbestand (Adresse/Orientierung)
const uint8_t SX_CHAN_ADDR_A = 1;
const uint8_t SX_CHAN_ADDR_B = 2;
const uint8_t SX_CHAN_ORIENT_L = 3; // Servo 0..7:  1=Abzweig links, 0=rechts
const uint8_t SX_CHAN_ORIENT_H = 4; // Servo 8..15: 1=Abzweig links, 0=rechts

// Neuer Setup-Wizard (gleiches Verhalten wie Serial-Setup)
const uint8_t SX_CHAN_SETUP_SESSION = 9; // Session-ID 1..255 (0=invalid)
const uint8_t SX_CHAN_SETUP_CMD = 10;    // 1=start, 2=abort, 3=save+end
const uint8_t SX_CHAN_SETUP_SERVO = 11;  // 0..15
const uint8_t SX_CHAN_SETUP_STEP = 12;   // 1/2/5/10/20
const uint8_t SX_CHAN_SETUP_MOVE = 13;   // 1=-step, 2=+step, 3=mitte
const uint8_t SX_CHAN_SETUP_STORE = 14;  // 1=save L, 2=save R
const uint8_t SX_CHAN_SETUP_ACK = 15;    // 0=idle,1=ok,2=err,3=busy

// ---------------- EEPROM ----------------
const uint16_t CFG_MAGIC = 0x5A41;
const int EEPROM_ADDR = 0;

struct ServoCfg
{
  int16_t zeroPhys;        // physischer Nullpunkt für rel=0
  int16_t relMin;          // linker Endpunkt relativ zu zeroPhys
  int16_t relMax;          // rechter Endpunkt relativ zu zeroPhys
  uint8_t divergingIsLeft; // 1=Abzweig links, 0=Abzweig rechts
};

struct DecoderCfg
{
  uint16_t magic;
  uint8_t sxAddrA;
  uint8_t sxAddrB;
  ServoCfg servo[SERVO_COUNT];
};

DecoderCfg cfg;

// ---------------- Laufzeitstatus ----------------
bool programming = false;
uint32_t keyPressTime = 0;
uint8_t oldDataA = 0xFF;
uint8_t oldDataB = 0xFF;

// Interaktive Initialroutine (seriell)
bool setupMode = false;
bool setupBySxWizard = false;
uint8_t setupServo = 0;
int16_t setupRelPos = 0;
int16_t setupStep = 5;
uint8_t sxSetupLastCmd = 0;
uint8_t sxSetupLastServo = 0;
uint8_t sxSetupLastMove = 0;
uint8_t sxSetupLastStore = 0;
bool sxCmdArmed = true;
unsigned long sxCmdCooldownUntilMs = 0;
unsigned long sxPostEndIgnoreUntilMs = 0;
unsigned long sxGuardUntilMs = 0;
uint8_t sxActiveSessionId = 0;
uint8_t sxCmdZeroStableCount = 0;
int8_t sxLockedServo = -1;

// Sequenzielles Ansteuern: niemals alle Servos gleichzeitig umschalten
// Groessere Staffelzeit reduziert Stromspitzen bei Mehrfach-Umschaltung.
const uint16_t SERVO_SWITCH_INTERVAL_MS = 180;
const uint16_t SERVO_HOLD_AFTER_MOVE_MS = 350;
uint8_t pendingDataA = 0;
uint8_t pendingDataB = 0;
uint8_t pendingMaskA = 0;
uint8_t pendingMaskB = 0;
bool pendingUseA = false;
bool pendingUseB = false;
bool hasPendingApply = false;
uint8_t nextServoToApply = 0;
uint32_t lastServoSwitchMs = 0;
uint32_t servoHoldUntilMs[SERVO_COUNT] = {0};
bool servoOutputActive[SERVO_COUNT] = {false};

// ---------- Hilfsfunktionen ----------
uint16_t angleToTick(uint8_t angle)
{
  if (angle > 180)
    angle = 180;
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

bool validSxAddr(uint8_t a)
{
  return (a >= SX_ADDR_MIN && a <= SX_ADDR_MAX);
}

bool validOrDisabledSxAddr(uint8_t a)
{
  return (a == SX_ADDR_DISABLED) || validSxAddr(a);
}

bool sxAddrEnabled(uint8_t a)
{
  return validSxAddr(a);
}

int16_t clampRel(uint8_t ch, int16_t rel)
{
  if (rel < -90)
    rel = -90;
  if (rel > 90)
    rel = 90;
  if (rel < cfg.servo[ch].relMin)
    rel = cfg.servo[ch].relMin;
  if (rel > cfg.servo[ch].relMax)
    rel = cfg.servo[ch].relMax;
  return rel;
}

void setServoRawPhys(uint8_t ch, uint8_t physAngle)
{
  if (ch >= SERVO_COUNT)
    return;
  pwm.setPWM(ch, 0, angleToTick(physAngle));
  servoOutputActive[ch] = true;
  servoHoldUntilMs[ch] = millis() + SERVO_HOLD_AFTER_MOVE_MS;
}

void releaseServoOutput(uint8_t ch)
{
  if (ch >= SERVO_COUNT)
    return;
  pwm.setPWM(ch, 0, 0);
  servoOutputActive[ch] = false;
}

void processServoAutoRelease()
{
  const uint32_t now = millis();
  for (uint8_t ch = 0; ch < SERVO_COUNT; ch++)
  {
    if (!servoOutputActive[ch])
      continue;
    if ((int32_t)(now - servoHoldUntilMs[ch]) >= 0)
    {
      releaseServoOutput(ch);
    }
  }
}

void setServoRel(uint8_t ch, int16_t rel)
{
  if (ch >= SERVO_COUNT)
    return;
  rel = clampRel(ch, rel);

  int16_t phys = cfg.servo[ch].zeroPhys + rel;
  if (phys < 0)
    phys = 0;
  if (phys > 180)
    phys = 180;

  setServoRawPhys(ch, (uint8_t)phys);
}

void moveGerade(uint8_t ch)
{
  if (cfg.servo[ch].divergingIsLeft)
  {
    setServoRel(ch, cfg.servo[ch].relMax);
  }
  else
  {
    setServoRel(ch, cfg.servo[ch].relMin);
  }
}

void moveAbzweig(uint8_t ch)
{
  if (cfg.servo[ch].divergingIsLeft)
  {
    setServoRel(ch, cfg.servo[ch].relMin);
  }
  else
  {
    setServoRel(ch, cfg.servo[ch].relMax);
  }
}

void applyBitToServo(uint8_t ch, uint8_t bitVal)
{
  // 0 = Gerade, 1 = Abzweig
  if (bitVal)
    moveAbzweig(ch);
  else
    moveGerade(ch);
}

void applyAllFromSx(uint8_t dataA, uint8_t dataB, bool useA, bool useB)
{
  // Nur geaenderte Bits als Pending markieren (wichtig: sonst bewegen sich zu viele Servos)
  pendingDataA = dataA;
  pendingDataB = dataB;
  pendingUseA = useA;
  pendingUseB = useB;

  pendingMaskA = useA ? (uint8_t)(dataA ^ oldDataA) : 0;
  pendingMaskB = useB ? (uint8_t)(dataB ^ oldDataB) : 0;

  hasPendingApply = (pendingMaskA != 0) || (pendingMaskB != 0);
}

void processPendingServoStep()
{
  if (!hasPendingApply)
    return;
  if ((millis() - lastServoSwitchMs) < SERVO_SWITCH_INTERVAL_MS)
    return;

  for (uint8_t n = 0; n < SERVO_COUNT; n++)
  {
    uint8_t ch = (nextServoToApply + n) % SERVO_COUNT;
    bool enabled = (ch < 8) ? pendingUseA : pendingUseB;
    if (!enabled)
      continue;

    uint8_t bitVal = (ch < 8) ? bitRead(pendingDataA, ch)
                              : bitRead(pendingDataB, ch - 8);
    bool isPending = (ch < 8) ? bitRead(pendingMaskA, ch)
                              : bitRead(pendingMaskB, ch - 8);

    if (isPending)
    {
      applyBitToServo(ch, bitVal);
      if (ch < 8)
      {
        bitWrite(oldDataA, ch, bitVal);
        bitWrite(pendingMaskA, ch, 0);
      }
      else
      {
        bitWrite(oldDataB, ch - 8, bitVal);
        bitWrite(pendingMaskB, ch - 8, 0);
      }

      nextServoToApply = (ch + 1) % SERVO_COUNT;
      lastServoSwitchMs = millis();
      return;
    }
  }

  // nichts mehr zu tun
  hasPendingApply = false;
  pendingMaskA = 0;
  pendingMaskB = 0;
}

void setDefaults()
{
  // SX-Adresse A ist Pflichtadresse und standardmaessig 3.
  // SX-Adresse B ist optional; 0 bedeutet "nicht benutzt".
  cfg.sxAddrA = 3;
  cfg.sxAddrB = 0;

  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    cfg.servo[i].zeroPhys = 90;
    cfg.servo[i].relMin = -40;
    cfg.servo[i].relMax = 40;
    cfg.servo[i].divergingIsLeft = 1;
  }
}

bool configValid(const DecoderCfg &c)
{
  if (c.magic != CFG_MAGIC)
    return false;
  // Adresse A ist Pflicht: 1..111.
  if (!validSxAddr(c.sxAddrA))
    return false;

  // Adresse B ist optional: 0 = nicht benutzt, sonst 1..111.
  if (!validOrDisabledSxAddr(c.sxAddrB))
    return false;

  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    const ServoCfg &s = c.servo[i];
    if (s.zeroPhys < 0 || s.zeroPhys > 180)
      return false;
    if (s.relMin < -90 || s.relMin > 90)
      return false;
    if (s.relMax < -90 || s.relMax > 90)
      return false;
    if (s.relMin >= s.relMax)
      return false;
    if (!(s.divergingIsLeft == 0 || s.divergingIsLeft == 1))
      return false;
  }
  return true;
}

void saveConfig()
{
  cfg.magic = CFG_MAGIC;
  EEPROM.put(EEPROM_ADDR, cfg);
  delay(10);
}

bool loadConfig()
{
  DecoderCfg tmp;
  EEPROM.get(EEPROM_ADDR, tmp);
  if (!configValid(tmp))
    return false;
  cfg = tmp;
  return true;
}

uint8_t getOrientationMaskLow()
{
  uint8_t m = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (cfg.servo[i].divergingIsLeft)
      bitSet(m, i);
  }
  return m;
}

uint8_t getOrientationMaskHigh()
{
  uint8_t m = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (cfg.servo[i + 8].divergingIsLeft)
      bitSet(m, i);
  }
  return m;
}

void setOrientationFromMasks(uint8_t lowMask, uint8_t highMask)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    cfg.servo[i].divergingIsLeft = bitRead(lowMask, i) ? 1 : 0;
  }
  for (uint8_t i = 0; i < 8; i++)
  {
    cfg.servo[i + 8].divergingIsLeft = bitRead(highMask, i) ? 1 : 0;
  }
}

void printSetupHelp()
{
  Serial.println(F("\n=== SERVO SETUP ==="));
  Serial.println(F("n = naechster Servo"));
  Serial.println(F("v = voriger Servo"));
  Serial.println(F("0 = Mitte (rel 0)"));
  Serial.println(F("l = linken Anschlag aus aktueller Position speichern"));
  Serial.println(F("r = rechten Anschlag aus aktueller Position speichern"));
  Serial.println(F("1/2/5 = Schrittweite 1/2/5"));
  Serial.println(F("a = gespeicherte Abzweig-Position anfahren"));
  Serial.println(F("g = gespeicherte Gerade-Position anfahren"));
  Serial.println(F("p <1..111> = SX-Adresse A setzen (Pflichtadresse, Default=3)"));
  Serial.println(F("o <0..111> = SX-Adresse B setzen (0=nicht benutzt)"));
  Serial.println(F("+ = nach rechts, - = nach links (um Schrittweite)"));
  Serial.println(F("w = alles speichern und Setup beenden"));
  Serial.println(F("Taster = wie w: speichern und Setup beenden"));
  Serial.println(F("x = Setup ohne Speichern beenden"));
}

bool readSerialUint8Argument(uint8_t &value)
{
  // Interaktive Eingabe:
  // Nach 'p' bzw. 'o' darf der Benutzer die Adresse normal eintippen,
  // z.B. "p 20" und danach ENTER. Die alte 50-ms-Grenze war fuer
  // manuelles Tippen zu kurz und machte aus 20 bereits nach der '2' den Wert 2.

  const unsigned long firstCharTimeoutMs = 5000;
  const unsigned long interCharTimeoutMs = 1500;

  unsigned long startMs = millis();

  // Zunaechst Leerzeichen / CR / LF verwerfen und auf die erste Ziffer warten.
  while (true)
  {
    if (Serial.available() > 0)
    {
      char c = (char)Serial.peek();

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      {
        Serial.read();
        continue;
      }

      if (!isDigit(c))
        return false;

      break;
    }

    if (millis() - startMs >= firstCharTimeoutMs)
      return false;

    delay(1);
  }

  uint16_t parsed = 0;
  unsigned long lastDigitMs = millis();

  while (true)
  {
    if (Serial.available() > 0)
    {
      char c = (char)Serial.peek();

      if (isDigit(c))
      {
        parsed = (uint16_t)(parsed * 10 + (Serial.read() - '0'));

        if (parsed > 255)
          return false;

        lastDigitMs = millis();
        continue;
      }

      // ENTER / Leerzeichen beendet die Zahl sauber.
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
      {
        Serial.read();
        break;
      }

      // Unerwartetes Zeichen beendet die Eingabe als Fehler.
      return false;
    }

    // Falls kein ENTER gesendet wird, nach ausreichend langer Tipp-Pause
    // die bisher eingegebene Zahl uebernehmen.
    if (millis() - lastDigitMs >= interCharTimeoutMs)
      break;

    delay(1);
  }

  value = (uint8_t)parsed;
  return true;
}

void printSxAddressStatus()
{
  Serial.print(F("SX-Adresse A: "));
  Serial.println(cfg.sxAddrA);

  if (cfg.sxAddrB == SX_ADDR_DISABLED)
  {
    Serial.println(F("SX-Adresse B: nicht benutzt (0)"));
  }
  else
  {
    Serial.print(F("SX-Adresse B: "));
    Serial.println(cfg.sxAddrB);
  }
}

void handleSerialSxAddressCommand(char cmd)
{
  uint8_t newAddr = 0;
  if (!readSerialUint8Argument(newAddr))
  {
    Serial.println(F("FEHLER: erwartet Zahl 0..111, z.B. 'p 72' oder 'o 73'."));
    return;
  }

  const bool isAddrA = (cmd == 'p' || cmd == 'P');

  if (isAddrA)
  {
    // Adresse A ist zwingend vorhanden.
    if (!validSxAddr(newAddr))
    {
      Serial.println(F("FEHLER: SX-Adresse A muss zwischen 1 und 111 liegen."));
      return;
    }
  }
  else
  {
    // Adresse B darf mit 0 deaktiviert werden.
    if (!validOrDisabledSxAddr(newAddr))
    {
      Serial.println(F("FEHLER: SX-Adresse B muss 0 oder zwischen 1 und 111 liegen."));
      return;
    }
  }

  if (isAddrA)
    cfg.sxAddrA = newAddr;
  else
    cfg.sxAddrB = newAddr;
  Serial.print(F("OK: SX-Adresse "));
  Serial.print((cmd == 'p' || cmd == 'P') ? 'A' : 'B');
  Serial.print(F("="));
  Serial.print(newAddr);
  Serial.println(F(" gesetzt (Speichern mit 'w')."));
  printSxAddressStatus();
}

void setupTelemetryMove(const __FlashStringHelper *src, int16_t step, int8_t moveCmd, int16_t relBefore, int16_t relAfter)
{
  Serial.print(F("ACK_SETUP_MOVE src="));
  Serial.print(src);
  Serial.print(F(" servo="));
  Serial.print(setupServo + 1);
  Serial.print(F(" rel="));
  Serial.print(setupRelPos);
  Serial.print(F(" relBefore="));
  Serial.print(relBefore);
  Serial.print(F(" relAfter="));
  Serial.print(relAfter);
  Serial.print(F(" step="));
  Serial.print(step);
  Serial.print(F(" move="));
  Serial.println(moveCmd);
}

void setupTelemetryState(const __FlashStringHelper *src, const __FlashStringHelper *action)
{
  Serial.print(F("ACK_SETUP_STATE src="));
  Serial.print(src);
  Serial.print(F(" action="));
  Serial.print(action);
  Serial.print(F(" servo="));
  Serial.print(setupServo + 1);
  Serial.print(F(" rel="));
  Serial.println(setupRelPos);
}

void setupTelemetryStore(const __FlashStringHelper *src, int8_t storeCmd)
{
  Serial.print(F("ACK_SETUP_STORE src="));
  Serial.print(src);
  Serial.print(F(" servo="));
  Serial.print(setupServo + 1);
  Serial.print(F(" cmd="));
  Serial.print(storeCmd);
  Serial.print(F(" rel="));
  Serial.print(setupRelPos);
  Serial.print(F(" relMin="));
  Serial.print(cfg.servo[setupServo].relMin);
  Serial.print(F(" relMax="));
  Serial.println(cfg.servo[setupServo].relMax);
}

void setupTelemetryStoreReject(const __FlashStringHelper *src, int8_t storeCmd, uint8_t sxServo, bool guardActive, bool edgeActive, bool sessionOk)
{
  Serial.print(F("ACK_SETUP_STORE_REJECT src="));
  Serial.print(src);
  Serial.print(F(" cmd="));
  Serial.print(storeCmd);
  Serial.print(F(" sxServo="));
  Serial.print((int)sxServo + 1);
  Serial.print(F(" setupServo="));
  Serial.print((int)setupServo + 1);
  Serial.print(F(" rel="));
  Serial.print(setupRelPos);
  Serial.print(F(" guard="));
  Serial.print(guardActive ? 1 : 0);
  Serial.print(F(" edge="));
  Serial.print(edgeActive ? 1 : 0);
  Serial.print(F(" sessionOk="));
  Serial.println(sessionOk ? 1 : 0);
}

void emitHello()
{
  Serial.print(F("HELLO decoder="));
  Serial.print(FW_DECODER_TYPE);
  Serial.print(F(" fw="));
  Serial.print(FW_VERSION);
  Serial.print(F(" proto="));
  Serial.println(FW_PROTO);
}

void emitCfgDump()
{
  Serial.print(F("CFG_HDR decoder="));
  Serial.print(FW_DECODER_TYPE);
  Serial.print(F(" fw="));
  Serial.print(FW_VERSION);
  Serial.print(F(" sxA="));
  Serial.print(cfg.sxAddrA);
  Serial.print(F(" sxB="));
  Serial.println(cfg.sxAddrB);
  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    Serial.print(F("CFG_S servo="));
    Serial.print(i + 1);
    Serial.print(F(" zero="));
    Serial.print(cfg.servo[i].zeroPhys);
    Serial.print(F(" relMin="));
    Serial.print(cfg.servo[i].relMin);
    Serial.print(F(" relMax="));
    Serial.print(cfg.servo[i].relMax);
    Serial.print(F(" divLeft="));
    Serial.println(cfg.servo[i].divergingIsLeft);
  }
  Serial.println(F("CFG_END"));
}

void setupSelectServo(uint8_t ch)
{
  if (ch >= SERVO_COUNT)
    ch = SERVO_COUNT - 1;
  setupServo = ch;
  setupRelPos = 0;
  setServoRel(setupServo, setupRelPos);
  Serial.print(F("\nServo S"));
  Serial.print(setupServo + 1);
  Serial.println(F(" aktiv, Mitte angefahren (rel 0)."));
  setupTelemetryState(F("core"), F("select"));
}

void debugPrintConfigTable(const __FlashStringHelper *source)
{
#if DEBUG_SERIAL

  Serial.println();
  Serial.println(F("=============================================================="));
  Serial.print(F(" SERVO DECODER CONFIG - "));
  Serial.println(source);
  Serial.println(F("=============================================================="));

  Serial.print(F("SX-Adresse A: "));
  Serial.println(cfg.sxAddrA);

  Serial.print(F("SX-Adresse B: "));
  Serial.println(cfg.sxAddrB);

  Serial.println();

  Serial.println(F("Servo | Zero | RelMin | RelMax | Abzweig"));
  Serial.println(F("------+------|--------|--------|---------"));

  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    Serial.print(F("S"));

    if ((i + 1) < 10)
      Serial.print('0');

    Serial.print(i + 1);

    Serial.print(F("   | "));

    if (cfg.servo[i].zeroPhys < 100)
      Serial.print(' ');
    if (cfg.servo[i].zeroPhys < 10)
      Serial.print(' ');

    Serial.print(cfg.servo[i].zeroPhys);

    Serial.print(F("  | "));

    if (cfg.servo[i].relMin >= 0)
      Serial.print(' ');

    Serial.print(cfg.servo[i].relMin);

    Serial.print(F("    | "));

    if (cfg.servo[i].relMax >= 0)
      Serial.print(' ');

    Serial.print(cfg.servo[i].relMax);

    Serial.print(F("    | "));

    if (cfg.servo[i].divergingIsLeft)
      Serial.println(F("LINKS"));
    else
      Serial.println(F("RECHTS"));
  }

  Serial.println(F("=============================================================="));
  Serial.println();

#endif
}
void setupMoveRel(int16_t delta)
{
  setupRelPos += delta;
  if (setupRelPos < -90)
    setupRelPos = -90;
  if (setupRelPos > 90)
    setupRelPos = 90;

  int16_t phys = cfg.servo[setupServo].zeroPhys + setupRelPos;
  if (phys < 0)
    phys = 0;
  if (phys > 180)
    phys = 180;
  setServoRawPhys(setupServo, (uint8_t)phys);

  Serial.print(F("S"));
  Serial.print(setupServo + 1);
  Serial.print(F(" rel="));
  Serial.println(setupRelPos);
}

#if ENABLE_SX_SETUP_WIZARD
bool setupAck(uint8_t v)
{
  unsigned long start = millis();

  while (sx.set(SX_CHAN_SETUP_ACK, v) != 0)
  {
    if (millis() - start > 500)
    {
#if DEBUG_SERIAL
      Serial.println(F("WARNUNG: setupAck Timeout"));
#endif
      return false;
    }

    delay(2);
  }

  return true;
}
#endif

bool setupValidateAll()
{
  // Adresse A ist Pflichtadresse.
  if (!validSxAddr(cfg.sxAddrA))
    return false;

  // Adresse B darf 0 (=nicht benutzt) oder 1..111 sein.
  if (!validOrDisabledSxAddr(cfg.sxAddrB))
    return false;

  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    if (cfg.servo[i].relMin >= cfg.servo[i].relMax)
      return false;
  }

  return true;
}

void printSerialSetupAddressWarning()
{
  Serial.println();
  Serial.println(F("**************************************************************"));
  Serial.println(F(" WARNUNG: SERVO-SETUP GESTARTET"));
  Serial.println(F("**************************************************************"));
  Serial.print(F(" SX-Adresse A (Pflicht): "));
  Serial.println(cfg.sxAddrA);

  if (cfg.sxAddrB == SX_ADDR_DISABLED)
  {
    Serial.println(F(" SX-Adresse B: nicht benutzt (0)"));
  }
  else
  {
    Serial.print(F(" SX-Adresse B: "));
    Serial.println(cfg.sxAddrB);
  }

  Serial.println(F(" Adressen aendern mit: p <1..111> und o <0..111>"));
  Serial.println(F(" Speichern mit: w"));
  Serial.println(F("**************************************************************"));
  Serial.println();
}

void startInitialSetup(bool fromSxWizard = false)
{
  // Servo-Setup und klassischer Modul-Programmiermodus duerfen
  // nicht parallel aktiv bleiben.
  programming = false;
  setupMode = true;
  digitalWrite(PROGLED, HIGH); // D13: Einstellmodus aktiv
  setupStep = 5;

#if ENABLE_SX_SETUP_WIZARD
  setupBySxWizard = fromSxWizard;
  sxSetupLastCmd = fromSxWizard ? sx.get(SX_CHAN_SETUP_CMD) : 0;
  sxSetupLastServo = fromSxWizard ? sx.get(SX_CHAN_SETUP_SERVO) : 0;
  sxSetupLastMove = fromSxWizard ? sx.get(SX_CHAN_SETUP_MOVE) : 0;
  sxSetupLastStore = fromSxWizard ? sx.get(SX_CHAN_SETUP_STORE) : 0;

  if (fromSxWizard)
  {
    setupSelectServo(0);
    setupAck(1);
    return;
  }
#else
  // SX-Wizard ist absichtlich deaktiviert. Selbst wenn versehentlich
  // true uebergeben wird, bleibt der Setup-Pfad rein seriell.
  (void)fromSxWizard;
  setupBySxWizard = false;
#endif

  printSerialSetupAddressWarning();
  printSetupHelp();
  setupSelectServo(0);
}

bool saveAndExitSetup(const __FlashStringHelper *source)
{
#if DEBUG_SERIAL
  Serial.print(F("DEBUG: SAVE gestartet, Quelle="));
  Serial.println(source);
#endif

  if (!setupValidateAll())
  {
    Serial.println(
        F("FEHLER: Konfiguration ungueltig (Adresse A/B oder Servo-Endpunkte). Nicht gespeichert."));
    return false;
  }

#if DEBUG_SERIAL
  Serial.println(F("DEBUG: Konfiguration gueltig"));
  Serial.println(F("DEBUG: schreibe EEPROM"));
#endif

  saveConfig();

#if DEBUG_SERIAL
  Serial.println(F("DEBUG: EEPROM geschrieben"));
#endif

  setupMode = false;
  setupBySxWizard = false;
  sxActiveSessionId = 0;
  sxLockedServo = -1;
  digitalWrite(PROGLED, LOW);

  Serial.print(F("Setup gespeichert und beendet. Quelle="));
  Serial.println(source);

#if DEBUG_SERIAL
  debugPrintConfigTable(F("NACH SPEICHERN"));
#endif

  return true;
}

void processSetupSerial()
{

  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ')
      continue;

    switch (c)
    {
    case 'n':
      setupSelectServo((setupServo + 1) % SERVO_COUNT);
      break;
    case 'v':
      setupSelectServo((setupServo == 0) ? (SERVO_COUNT - 1) : (setupServo - 1));
      break;
    case '0':
      setupRelPos = 0;
      setServoRel(setupServo, setupRelPos);
      Serial.println(F("Mitte (rel 0)."));
      setupTelemetryState(F("serial"), F("mid"));
      break;
    case 'l':
      cfg.servo[setupServo].relMin = setupRelPos;
      Serial.print(F("S"));
      Serial.print(setupServo + 1);
      Serial.print(F(" relMin gespeichert: "));
      Serial.println(cfg.servo[setupServo].relMin);
      setupTelemetryStore(F("serial"), 1);
      break;
    case 'r':
      cfg.servo[setupServo].relMax = setupRelPos;
      Serial.print(F("S"));
      Serial.print(setupServo + 1);
      Serial.print(F(" relMax gespeichert: "));
      Serial.println(cfg.servo[setupServo].relMax);
      setupTelemetryStore(F("serial"), 2);
      break;
    case '1':
      setupStep = 1;
      Serial.println(F("Schrittweite=1"));
      break;
    case '2':
      setupStep = 2;
      Serial.println(F("Schrittweite=2"));
      break;
    case '5':
      setupStep = 5;
      Serial.println(F("Schrittweite=5"));
      break;
    case 'p':
    case 'P':
    case 'o':
    case 'O':
      handleSerialSxAddressCommand(c);
      break;
    case 'a':
    case 'A':
    {
      int16_t relBefore = setupRelPos;

      // a = Abzweig anfahren
      if (cfg.servo[setupServo].divergingIsLeft)
      {
        setupRelPos = cfg.servo[setupServo].relMin;
      }
      else
      {
        setupRelPos = cfg.servo[setupServo].relMax;
      }

      setServoRel(setupServo, setupRelPos);

      Serial.print(F("S"));
      Serial.print(setupServo + 1);
      Serial.print(F(" Abzweig angefahren, rel="));
      Serial.println(setupRelPos);

      setupTelemetryMove(F("serial"), 0, 4, relBefore, setupRelPos);
    }
    break;

    case 'g':
    case 'G':
    {
      int16_t relBefore = setupRelPos;

      // g = Gerade anfahren
      if (cfg.servo[setupServo].divergingIsLeft)
      {
        setupRelPos = cfg.servo[setupServo].relMax;
      }
      else
      {
        setupRelPos = cfg.servo[setupServo].relMin;
      }

      setServoRel(setupServo, setupRelPos);

      Serial.print(F("S"));
      Serial.print(setupServo + 1);
      Serial.print(F(" Gerade angefahren, rel="));
      Serial.println(setupRelPos);

      setupTelemetryMove(F("serial"), 0, 5, relBefore, setupRelPos);
    }
    break;
    case '+':
    {
      int16_t b = setupRelPos;
      setupMoveRel(setupStep);
      setupTelemetryMove(F("serial"), setupStep, 2, b, setupRelPos);
    }
    break;
    case '-':
    {
      int16_t b = setupRelPos;
      setupMoveRel(-setupStep);
      setupTelemetryMove(F("serial"), setupStep, 1, b, setupRelPos);
    }
    break;

    case 'w':
    case 'W':
      saveAndExitSetup(F("serial"));
      break;

    case 'x':
    case 'X':

      setupMode = false;
      setupBySxWizard = false;

      sxActiveSessionId = 0;
      sxLockedServo = -1;

      digitalWrite(PROGLED, LOW);

      Serial.println(F("Setup beendet ohne Speichern."));

      break;
    case 'h':
    case '?':
      printSetupHelp();
      break;
    case 'c':
      emitCfgDump();
      break;
    case 't':
      emitHello();
      break;
    default:
      // Unbekannte/Steuerzeichen ignorieren (z.B. Bus-/Terminal-Noise)
      break;
    }
  }
}

bool keypressed()
{
  if ((millis() - keyPressTime) < (5UL * DEBOUNCETIME))
    return false;

  if (KEYPRESS)
  {
    delay(DEBOUNCETIME);
    if (KEYPRESS)
    {
      keyPressTime = millis();
      return true;
    }
  }
  return false;
}

void startModuleProgramming()
{
  programming = true;
  keyPressTime = millis();
  digitalWrite(PROGLED, HIGH);
  Serial.println(F("PROG_STATUS active=1 source=local_button track=0 led=1"));

  // aktuelle Werte auf Programmierkanäle legen
  while (sx.set(SX_CHAN_ADDR_A, cfg.sxAddrA) != 0)
    delay(10);
  while (sx.set(SX_CHAN_ADDR_B, cfg.sxAddrB) != 0)
    delay(10);
  while (sx.set(SX_CHAN_ORIENT_L, getOrientationMaskLow()) != 0)
    delay(10);
  while (sx.set(SX_CHAN_ORIENT_H, getOrientationMaskHigh()) != 0)
    delay(10);
}

void finishModuleProgramming()
{
  programming = false;

  uint8_t newA = sx.get(SX_CHAN_ADDR_A);
  uint8_t newB = sx.get(SX_CHAN_ADDR_B);
  uint8_t lowMask = sx.get(SX_CHAN_ORIENT_L);
  uint8_t highMask = sx.get(SX_CHAN_ORIENT_H);

  uint8_t candA = cfg.sxAddrA;
  uint8_t candB = cfg.sxAddrB;

  if (validOrDisabledSxAddr(newA))
    candA = newA;
  if (validOrDisabledSxAddr(newB))
    candB = newB;

  // Sicherheit: nie beide Adressen deaktivieren
  if (!sxAddrEnabled(candA) && !sxAddrEnabled(candB))
  {
    // falls beide 0 wurden, Konfiguration unverändert lassen
  }
  else
  {
    cfg.sxAddrA = candA;
    cfg.sxAddrB = candB;
  }

  setOrientationFromMasks(lowMask, highMask);

  saveConfig();
  digitalWrite(PROGLED, LOW);
  Serial.print(F("PROG_STATUS active=0 source=local_button track="));
  Serial.print(sx.getTrackBit());
  Serial.println(F(" led=0"));
}

// ---------- SX ISR ----------
void sxisr()
{
  sx.isr();
}

// ---------- Setup / Loop ----------
void setup()
{
  Serial.begin(115200);

  pinMode(PROGLED, OUTPUT);
  digitalWrite(PROGLED, LOW);
  pinMode(PROGBUTTON, INPUT);
  digitalWrite(PROGBUTTON, HIGH); // Pullup

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  bool loadedFromEeprom = loadConfig();

  if (!loadedFromEeprom)
  {
    setDefaults();
    saveConfig();

#if DEBUG_SERIAL
    Serial.println(F("DEBUG: EEPROM ungueltig -> Defaults erzeugt und gespeichert"));
#endif
  }
  else
  {
#if DEBUG_SERIAL
    Serial.println(F("DEBUG: Konfiguration erfolgreich aus EEPROM geladen"));
#endif
  }

  debugPrintConfigTable(
      loadedFromEeprom ? F("EEPROM") : F("DEFAULTS"));
  Serial.println();
  Serial.println(F("SX30 ServoDecoder start"));
  Serial.print(F("FW-Version: SX30-ServoDecoder "));
  Serial.println(FW_VERSION);
  emitHello();
  Serial.println(loadedFromEeprom ? F("CFG: aus EEPROM geladen") : F("CFG: Defaults genutzt"));
  Serial.println(F("Setup starten: 's' senden"));

  // Beim Start zuerst alle auf Mitte (rel 0), bewusst nacheinander
  for (uint8_t i = 0; i < SERVO_COUNT; i++)
  {
    setServoRel(i, 0);
    delay(SERVO_SWITCH_INTERVAL_MS);
  }

  sx.init();
  attachInterrupt(0, sxisr, CHANGE);

  // aktuellen SX-Zustand direkt übernehmen
  bool useA = sxAddrEnabled(cfg.sxAddrA);
  bool useB = sxAddrEnabled(cfg.sxAddrB);
  oldDataA = useA ? sx.get(cfg.sxAddrA) : 0;
  oldDataB = useB ? sx.get(cfg.sxAddrB) : 0;
  applyAllFromSx(oldDataA, oldDataB, useA, useB);
}

#if ENABLE_SX_SETUP_WIZARD
void processSetupSxWizard()
{
  const unsigned long nowMs = millis();
  if (nowMs < sxPostEndIgnoreUntilMs)
    return;

  auto normCmd = [](uint8_t raw) -> uint8_t
  {
    return (raw <= 3) ? raw : 0;
  };
  auto normMove = [](uint8_t raw) -> uint8_t
  {
    return (raw <= 3) ? raw : 0;
  };
  auto normStore = [](uint8_t raw) -> uint8_t
  {
    return (raw <= 2) ? raw : 0;
  };

  uint8_t sxSession = sx.get(SX_CHAN_SETUP_SESSION);
  uint8_t rawCmd = sx.get(SX_CHAN_SETUP_CMD);
  uint8_t rawMove = sx.get(SX_CHAN_SETUP_MOVE);
  uint8_t rawStore = sx.get(SX_CHAN_SETUP_STORE);
  uint8_t k15 = sx.get(SX_CHAN_SETUP_ACK);
  uint8_t k1AddrA = sx.get(SX_CHAN_ADDR_A);
  uint8_t cmd = normCmd(rawCmd);
  uint8_t move = normMove(rawMove);
  uint8_t store = normStore(rawStore);

  uint8_t prevZeroStable = sxCmdZeroStableCount;
  if (cmd == 0)
  {
    if (sxCmdZeroStableCount < 255)
      sxCmdZeroStableCount++;
  }
  else
  {
    sxCmdZeroStableCount = 0;
  }

  // Start nur akzeptieren, wenn Session gesetzt + K15-Freigabe aktiv + K1 plausibel
  // und vorher cmd=0 einige Zyklen stabil war (Edge-Detektor gegen raw=85-Flattern)
  if (cmd == 1)
  {
    bool startValid = (sxSession != 0) && (k15 == 1) && (k1AddrA == cfg.sxAddrA) && (prevZeroStable >= 2);
    if (startValid)
    {
      sxActiveSessionId = sxSession;
    }
    else
    {
      return;
    }
  }

  // Fuer alle weiteren Wizard-Befehle muss die aktive Session exakt passen
  if (sxActiveSessionId == 0 || sxSession != sxActiveSessionId)
  {
    return;
  }

  // CMD nur als entprellte Impulsflanke akzeptieren: erst cmd=0 armt erneut
  // plus Cooldown gegen spaete Wiederholimpulse (raw=85-Muster)
  if (cmd == 0)
  {
    sxCmdArmed = true;
  }
  else if (sxCmdArmed && millis() >= sxCmdCooldownUntilMs)
  {
    Serial.print(F("ACK_SETUP_CMD src=sx raw="));
    Serial.print(rawCmd);
    Serial.print(F(" cmd="));
    Serial.println(cmd);
    sxCmdArmed = false;
    sxSetupLastCmd = cmd;
    sxCmdCooldownUntilMs = millis() + 700;
    if (cmd == 1)
    {
      startInitialSetup(true);
      sxLockedServo = (int8_t)sx.get(SX_CHAN_SETUP_SERVO);
      if (sxLockedServo < 0 || sxLockedServo >= SERVO_COUNT)
        sxLockedServo = setupServo;
      if ((uint8_t)sxLockedServo != setupServo)
        setupSelectServo((uint8_t)sxLockedServo);
      sxGuardUntilMs = millis() + 400; // nur kurz danach Move/Store akzeptieren
      setupAck(1);
    }
    else if (cmd == 2)
    {
      setupMode = false;
      digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
      sxActiveSessionId = 0;
      sxLockedServo = -1;
      sxPostEndIgnoreUntilMs = millis() + 1000;
      setupAck(0);
    }
    else if (cmd == 3)
    {
      if (setupValidateAll())
      {
        saveConfig();
        setupMode = false;
        digitalWrite(PROGLED, LOW); // D13 aus: Einstellmodus beendet
        sxActiveSessionId = 0;
        sxLockedServo = -1;
        sxPostEndIgnoreUntilMs = millis() + 1000;
        setupAck(1);
      }
      else
      {
        setupAck(2);
      }
    }
  }

  if (!setupMode)
    return;

  uint8_t sxServo = sx.get(SX_CHAN_SETUP_SERVO);
  if (sxServo >= SERVO_COUNT)
    sxServo = SERVO_COUNT - 1;
  if (sxLockedServo >= 0)
    sxServo = (uint8_t)sxLockedServo;
  if (sxServo != sxSetupLastServo)
  {
    sxSetupLastServo = sxServo;
    if (sxServo != setupServo)
    {
      setupSelectServo(sxServo);
      setupAck(1);
    }
  }

  uint8_t sxStep = sx.get(SX_CHAN_SETUP_STEP);
  if (sxStep == 1 || sxStep == 2 || sxStep == 5 || sxStep == 10 || sxStep == 20)
    setupStep = sxStep;

  // Nur 0->Befehl Flanken akzeptieren (robust gegen Bus-Jitter/Mehrfachtelegramme)
  if (move != sxSetupLastMove)
  {
    if (sxSetupLastMove == 0 && millis() <= sxGuardUntilMs)
    {
      // SX-Wizard-Richtung: im Feldtest war + aus Qt effektiv invertiert.
      // Daher fuer SX-Pfad 1/2 gespiegelt behandeln.
      if (move == 1)
      {
        int16_t b = setupRelPos;
        setupMoveRel(setupStep);
        setupTelemetryMove(F("sx"), setupStep, 1, b, setupRelPos);
        setupAck(1);
      }
      else if (move == 2)
      {
        int16_t b = setupRelPos;
        setupMoveRel(-setupStep);
        setupTelemetryMove(F("sx"), setupStep, 2, b, setupRelPos);
        setupAck(1);
      }
      else if (move == 3)
      {
        int16_t b = setupRelPos;
        setupRelPos = 0;
        setServoRel(setupServo, 0);
        setupTelemetryMove(F("sx"), setupStep, 3, b, setupRelPos);
        setupTelemetryState(F("sx"), F("mid"));
        setupAck(1);
      }
    }
    sxSetupLastMove = move;
  }

  if (store != sxSetupLastStore)
  {
    bool edgeActive = (sxSetupLastStore == 0);
    bool guardActive = (millis() <= sxGuardUntilMs);
    bool sessionOk = (sxActiveSessionId != 0);
    bool servoMatch = (sxServo == setupServo);
    if (edgeActive && guardActive && servoMatch)
    {
      if (store == 1)
      {
        cfg.servo[setupServo].relMin = setupRelPos;
        setupTelemetryStore(F("sx"), 1);
        setupAck(1);
      }
      else if (store == 2)
      {
        cfg.servo[setupServo].relMax = setupRelPos;
        setupTelemetryStore(F("sx"), 2);
        setupAck(1);
      }
    }
    else if (store == 1 || store == 2)
    {
      setupTelemetryStoreReject(F("sx"), store, sxServo, guardActive, edgeActive, sessionOk);
    }
    sxSetupLastStore = store;
  }
}

#endif

void loop()
{
#if ENABLE_SX_SETUP_WIZARD
  processSetupSxWizard();
#endif

  // Im S-Setup:
  // - LED 13 bleibt EIN.
  // - lokaler Taster ist identisch zu 'w':
  //   validieren, EEPROM speichern, Setup beenden, LED AUS.
  if (setupMode)
  {
    digitalWrite(PROGLED, HIGH);

    if (keypressed())
    {
      saveAndExitSetup(F("button"));
      return;
    }

    processSetupSerial();
    return;
  }

  // Ausserhalb des S-Setups:
  // serielles 's' startet das Setup.
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();

    if (c == '\r' || c == '\n' || c == ' ')
      continue;

#if DEBUG_SERIAL
    Serial.print(F("RX:"));
    Serial.println(c);
#endif

    if (c == 's' || c == 'S')
    {
      startInitialSetup(false);   // setzt setupMode=true und LED 13 HIGH
      return;
    }

    if (c == 't' || c == 'T')
    {
      emitHello();
      continue;
    }

    if (c == 'c' || c == 'C')
    {
      emitCfgDump();
      continue;
    }
  }

  // Lokaler Taster startet jetzt immer das gleiche Setup wie 's'.
  // Der alte lokale SX-Modul-Programmiermodus wird nicht mehr ueber
  // diesen Taster gestartet.
  if (keypressed())
  {
#if DEBUG_SERIAL
    Serial.println(F("DEBUG: Taster gedrueckt -> starte S-Setup"));
#endif
    startInitialSetup(false);
    return;
  }

  // ---------------- Normaler SX-Fahrbetrieb ----------------
  bool useA = sxAddrEnabled(cfg.sxAddrA);
  bool useB = sxAddrEnabled(cfg.sxAddrB);

  uint8_t dA = useA ? sx.get(cfg.sxAddrA) : oldDataA;
  uint8_t dB = useB ? sx.get(cfg.sxAddrB) : oldDataB;

  bool changed = false;

  if (useA && dA != oldDataA)
    changed = true;

  if (useB && dB != oldDataB)
    changed = true;

  if (changed)
  {
    applyAllFromSx(dA, dB, useA, useB);
  }

  processPendingServoStep();
  processServoAutoRelease();

  // Ausserhalb des Setups ist die Setup-LED aus.
  digitalWrite(PROGLED, LOW);
}
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------------------- LCD --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Dirección típica del módulo I2C

// -------------------- Pines --------------------
// Sensores
const int PIN_MQ2      = A0;
const int PIN_LDR      = A1;
const int PIN_PIR_EXT  = 2;
const int PIN_PIR_INT  = 3;

// Botones (con INPUT_PULLUP)
const int PIN_BTN_ARM     = 4;
const int PIN_BTN_MENU    = 5;
const int PIN_BTN_SILENCE = 6;

// Relés
const int PIN_RELAY_LUZ_EXT  = 8;
const int PIN_RELAY_LUZ_INT  = 9;
const int PIN_RELAY_VENT     = 10;

// Buzzer
const int PIN_BUZZER = 11;

// -------------------- Parámetros de funcionamiento --------------------
const unsigned long TIEMPO_LUZ_EXTERIOR = 20000UL; // 20 s
unsigned long tiempoInactividad = 30000UL;         // 30 s (cambiable con MENU)

const int UMBRAL_GAS       = 400;
const int UMBRAL_OSCURIDAD = 400;

// -------------------- Estado del sistema --------------------
bool modoArmado = false;
bool alarmaGasActiva = false;
bool alarmaRoboActiva = false;
bool alarmaSilenciada = false;

bool luzExteriorEncendida = false;
unsigned long tInicioLuzExterior = 0;

unsigned long tUltimoMovimientoInterior = 0;

unsigned long tUltimoRefreshLCD = 0;
const unsigned long INTERVALO_LCD = 500;

bool lastArmState     = HIGH;
bool lastMenuState    = HIGH;
bool lastSilenceState = HIGH;

// -------------------- Funciones auxiliares --------------------

void actualizarLCD(int gas, int luz, bool movInt, bool movExt) {
  unsigned long ahora = millis();
  if (ahora - tUltimoRefreshLCD < INTERVALO_LCD) return;
  tUltimoRefreshLCD = ahora;

  lcd.clear();

  // --------- Línea 1: estado y tipo de alarma ---------
  lcd.setCursor(0, 0);
  if (modoArmado) {
    lcd.print("ARMADO ");
  } else {
    lcd.print("DESARMADO");
  }

  lcd.setCursor(10, 0);
  if (alarmaGasActiva || alarmaRoboActiva) {
    if (alarmaGasActiva && alarmaRoboActiva) {
      lcd.print("G+R");   // gas + robo
    } else if (alarmaGasActiva) {
      lcd.print("GAS");   // solo gas
    } else {
      lcd.print("ROB");   // solo robo
    }
  } else {
    lcd.print("OK ");
  }

  // --------- Cálculo de cuenta regresiva de luces ---------
  bool luzIntEncendida = (digitalRead(PIN_RELAY_LUZ_INT) == HIGH);

  unsigned long segRestExt = 0;
  unsigned long segRestInt = 0;

  if (luzExteriorEncendida) {
    long diffExt = (long)TIEMPO_LUZ_EXTERIOR - (long)(ahora - tInicioLuzExterior);
    if (diffExt < 0) diffExt = 0;
    segRestExt = (unsigned long)(diffExt / 1000UL);
  }

  if (luzIntEncendida && !modoArmado) {
    long diffInt = (long)tiempoInactividad - (long)(ahora - tUltimoMovimientoInterior);
    if (diffInt < 0) diffInt = 0;
    segRestInt = (unsigned long)(diffInt / 1000UL);
  }

  // --------- Línea 2: si hay luces con tiempo, mostrar contador ---------
  lcd.setCursor(0, 1);
  if (luzExteriorEncendida || (luzIntEncendida && !modoArmado)) {
    // Modo "contador de apagado"
    lcd.print("EXT:");
    if (luzExteriorEncendida) {
      lcd.print(segRestExt);
    } else {
      lcd.print("-");   // sin cuenta para exterior
    }

    lcd.print(" INT:");
    if (luzIntEncendida && !modoArmado) {
      lcd.print(segRestInt);
    } else {
      lcd.print("-");   // sin cuenta para interior
    }
  } else {
    // Modo normal (como lo tenías antes)
    lcd.print("G:");
    lcd.print(gas);
    lcd.print(" L:");
    lcd.print(luz);

    lcd.setCursor(12, 1);
    if (movExt) lcd.print("E");
    else        lcd.print("-");
    if (movInt) lcd.print("I");
    else        lcd.print("-");
  }
}


void leerBotones() {
  bool armState     = digitalRead(PIN_BTN_ARM);
  bool menuState    = digitalRead(PIN_BTN_MENU);
  bool silenceState = digitalRead(PIN_BTN_SILENCE);

  // ----- BOTÓN ARMAR / DESARMAR -----
  if (lastArmState == HIGH && armState == LOW) {
    modoArmado = !modoArmado;
    if (!modoArmado) {
      alarmaRoboActiva = false;
    }
  }

  // ----- BOTÓN MENU (CAMBIAR TIEMPO) -----
  if (lastMenuState == HIGH && menuState == LOW) {
    if (tiempoInactividad == 30000UL) {
      tiempoInactividad = 15000UL;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TIEMPO: 15s");
      delay(1000);

    } else {
      tiempoInactividad = 30000UL;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TIEMPO: 30s");
      delay(1000);
    }
  }

  // ----- BOTÓN SILENCIO -----
  if (lastSilenceState == HIGH && silenceState == LOW) {
    alarmaSilenciada = true;
    noTone(PIN_BUZZER);
  }

  lastArmState     = armState;
  lastMenuState    = menuState;
  lastSilenceState = silenceState;
}


void gestionarAlarmaSonora() {
  bool hayRiesgo = alarmaGasActiva || alarmaRoboActiva;

  if (hayRiesgo && !alarmaSilenciada) {
    tone(PIN_BUZZER, 1000);
  } else {
    noTone(PIN_BUZZER);
  }

  if (!hayRiesgo) {
    alarmaSilenciada = false;
  }
}

// -------------------- Setup --------------------

void setup() {
  Serial.begin(9600);

  pinMode(PIN_MQ2, INPUT);
  pinMode(PIN_LDR, INPUT);

  pinMode(PIN_PIR_EXT, INPUT);
  pinMode(PIN_PIR_INT, INPUT);

  pinMode(PIN_BTN_ARM, INPUT_PULLUP);
  pinMode(PIN_BTN_MENU, INPUT_PULLUP);
  pinMode(PIN_BTN_SILENCE, INPUT_PULLUP);

  pinMode(PIN_RELAY_LUZ_EXT, OUTPUT);
  pinMode(PIN_RELAY_LUZ_INT, OUTPUT);
  pinMode(PIN_RELAY_VENT, OUTPUT);

  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_RELAY_LUZ_EXT, LOW);
  digitalWrite(PIN_RELAY_LUZ_INT, LOW);
  digitalWrite(PIN_RELAY_VENT, LOW);
  noTone(PIN_BUZZER);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Domotico");
  lcd.setCursor(0, 1);
  lcd.print("Inicializando...");
  delay(1500);
  lcd.clear();

  tUltimoMovimientoInterior = millis();
}

// -------------------- Loop principal --------------------

void loop() {
  leerBotones();

  int valorGas = analogRead(PIN_MQ2);
  int valorLuz = analogRead(PIN_LDR);
  bool movExt = digitalRead(PIN_PIR_EXT) == HIGH;
  bool movInt = digitalRead(PIN_PIR_INT) == HIGH;

  unsigned long ahora = millis();

  if (valorGas >= UMBRAL_GAS) {
    alarmaGasActiva = true;
    digitalWrite(PIN_RELAY_VENT, HIGH);
  } else {
    alarmaGasActiva = false;
    digitalWrite(PIN_RELAY_VENT, LOW);
  }

  bool esOscuro = (valorLuz >= UMBRAL_OSCURIDAD);

  if (movExt && esOscuro) {
    luzExteriorEncendida = true;
    tInicioLuzExterior = ahora;
    digitalWrite(PIN_RELAY_LUZ_EXT, HIGH);
  }

  if (luzExteriorEncendida && (ahora - tInicioLuzExterior >= TIEMPO_LUZ_EXTERIOR)) {
    luzExteriorEncendida = false;
    digitalWrite(PIN_RELAY_LUZ_EXT, LOW);
  }

  if (movInt) {
    tUltimoMovimientoInterior = ahora;

    if (modoArmado) {
      alarmaRoboActiva = true;
      digitalWrite(PIN_RELAY_LUZ_INT, HIGH);
    } else {
      alarmaRoboActiva = false;
      digitalWrite(PIN_RELAY_LUZ_INT, HIGH);
    }
  } else {
    if (!modoArmado) {
      if (ahora - tUltimoMovimientoInterior >= tiempoInactividad) {
        digitalWrite(PIN_RELAY_LUZ_INT, LOW);
      }
    }
  }

  gestionarAlarmaSonora();
  actualizarLCD(valorGas, valorLuz, movInt, movExt);
}

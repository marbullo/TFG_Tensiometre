#include <Arduino.h>
#include <math.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

/* ────── PINS ────── */
const int sortida_bomba    = 25;  // MOSFET bomba (PWM)
const int sortida_valvula  = 26;  // MOSFET/relé vàlvula
const int filtrat_pasbaix  = 35;  // DC (pressió)
const int filtrat_pasbanda = 34;  // AC (oscil·lació)

/* ────── PWM BOMBA ────── */
const uint8_t  PWM_CH   = 0;
const uint16_t PWM_FREQ = 20000;
const uint8_t  PWM_BITS = 8;
const uint8_t  DUTY_ON  = 255; //255 es al 100% 

/* ────── variables detectar oscil ────── */
const int MIDA_FINESTRA = 50;
float finestra[MIDA_FINESTRA];
int index_finestra = 0;
bool finestra_plena = false;

int comptador_sense_pols = 0;
const int MAX_SENSE_POLS = 2; 
const float amplitud_llindar=0.24;


/* ────── DECLARACIÓ DE FUNCIONS ────── */
void engegar_bomba();
void enviar_python(float pressio, float volts_banda);
void llegir_pressions(float &pressio, float &volts_banda); 
void inflar_fins_no_osc(); 
float calcular_amplitud();
bool hi_ha_oscil();
float volts_a_mmHg(float v) { return (v - 0.2f) * 83.33f; } 
void desinflar();
void ini_pantalla(); 
void mostrar_pressio(String estat, float pressio);
void mostrar_resultats_finals();

/* ────── SETUP ────── */
void setup() {
  Serial.begin(115200);
  ini_pantalla(); 
  engegar_bomba(); 
  inflar_fins_no_osc();
  desinflar(); 
  mostrar_resultats_finals();
}

void loop(){}

/* ────── FUNCIONS ────── */
void engegar_bomba() {
  pinMode(sortida_valvula, OUTPUT);
  pinMode(sortida_bomba, OUTPUT);
  digitalWrite(sortida_valvula, LOW);     // vàlvula OBERTA (LOW=oberta)

  ledcSetup(PWM_CH, PWM_FREQ, PWM_BITS);
  ledcAttachPin(sortida_bomba, PWM_CH);
  ledcWrite(PWM_CH, 0);                   // bomba OFF


  while (!Serial.available()) { delay(10); }           // while- esperem que arribi start des de python i tanquem vàlvula 
  String start = Serial.readStringUntil('\n'); start.trim();
  while (start != "START") {                             // si no és START, segueix esperant
    while (!Serial.available()) { delay(10); }
    start = Serial.readStringUntil('\n'); start.trim();
  }
  digitalWrite(sortida_valvula, HIGH);  // vàlvula TANCADA (HIGH=tancada)
  ledcWrite(PWM_CH, DUTY_ON);           // bomba ON
}

/* ────── LLEGIR I ENVIAR ────── */
void enviar_python(float pressio, float volts_banda) {
  Serial.print("Pasbaix: ");
  Serial.print(pressio, 2);
  Serial.print(" mmHg | Pasbanda: ");
  Serial.print(volts_banda, 3);
  Serial.println(" V");
 }

void llegir_pressions(float &pressio, float &volts_banda){
  int adc = analogRead(filtrat_pasbaix);
  float volts = (adc / 4095.0f) * 3.3f;
  pressio = volts_a_mmHg(volts);

  int adc_banda = analogRead(filtrat_pasbanda);
  volts_banda = (adc_banda / 4095.0f) * 3.3f;
}

float calcular_amplitud() {
  float max = finestra[0];
  float min = finestra[0];

  for (int i = 1; i < MIDA_FINESTRA; i++) {
    if (finestra[i] > max) max = finestra[i];
    if (finestra[i] < min) min = finestra[i];
  }

  return max - min;
}

bool hi_ha_oscil(){
  if (!finestra_plena) return true;  // encara no podem decidir
  float amplitud = calcular_amplitud();
  if (amplitud < amplitud_llindar) comptador_sense_pols++;
  else comptador_sense_pols=0;
  if (comptador_sense_pols >= MAX_SENSE_POLS) return false;
  return true;
}

void inflar_fins_no_osc(){
  float pressio = 0.0;
  float volts_banda = 0.0;
  comptador_sense_pols=0;
  index_finestra = 0;
  finestra_plena = false;
  llegir_pressions(pressio,volts_banda);
  while (pressio<=120) {
    //llegim sense comprovar oscil·lacions fins a 120mmHg 
    llegir_pressions(pressio,volts_banda); 
    enviar_python(pressio,volts_banda); 
    mostrar_pressio("Inflant...", pressio); 
    delay(15);
  }
  while(true){
    //llegim fins que no es detectin oscil·lacions
    llegir_pressions(pressio,volts_banda); 
    enviar_python(pressio,volts_banda); 
    mostrar_pressio("Inflant...", pressio);
    //guardem mostra
    finestra[index_finestra] = volts_banda;
    index_finestra++;

    if (index_finestra >= MIDA_FINESTRA) {
      index_finestra = 0;
      finestra_plena = true;
    }
    if (!hi_ha_oscil()) break;
    if (pressio > 240) break;
    delay(15);
  }
}

void desinflar() {
  float pressio = 0.0;
  float volts_banda = 0.0;
  comptador_sense_pols = 0;
  index_finestra = 0;
  finestra_plena = false;
  bool hem_detectat_pols = false;

  ledcWrite(PWM_CH, 0); 
  digitalWrite(sortida_valvula, LOW);

  
  while (true) {
    // Llegim i enviem dades
    for (int i = 0; i < 10; i++) {
      llegir_pressions(pressio, volts_banda);
      enviar_python(pressio, volts_banda);
      mostrar_pressio("Desinflant...", pressio);

      // IMPORTANT: guardar mostra per detectar oscil·lacions
      finestra[index_finestra] = volts_banda;
      index_finestra++;

      if (index_finestra >= MIDA_FINESTRA) {
        index_finestra = 0;
        finestra_plena = true;
      }

      delay(15);
    }
    bool oscil= hi_ha_oscil(); 
    if (oscil) {
      hem_detectat_pols = true;
    }
    // Parem quan ja no hi ha oscil·lació
    if (pressio<40 && hem_detectat_pols && !oscil) break;

    //seguretat
    if (pressio < 10) break;
    
  }
}

void ini_pantalla(){
  //FUNCIÓ PER INICIALITZAR LA PANTALLA
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("TENSIOMETRE", 30, 20, 4);
  tft.drawString("mmHg", 90, 180, 4);
}

void mostrar_pressio(String estat, float pressio) {
  //FUNCIÓ PER MOSTRAR VALORS DE PRESSIÓ DURANT INFLAT I DESINFLAT
  static unsigned long ultim_update = 0;
  if (millis() - ultim_update < 150) return;
  ultim_update = millis();
  
  tft.fillRect(20, 50, 220, 40, TFT_BLACK);// esborrem estat

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(estat, 20, 50, 4);

  tft.fillRect(20, 90, 220, 80, TFT_BLACK);// esborrem numero

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  char textPressio[5];
  sprintf(textPressio, "%3d", (int)pressio);

  tft.drawString(textPressio, 50, 100, 8);
}

void mostrar_resultats_finals() {
  //FUNCIÓ PER MOSTRAR PRESSIÓ SISTÒLICA I DIASTÒLICA
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Esperant resultats", 20, 80, 4);

  while (!Serial.available()) {
    delay(10);
  }

  String msg = Serial.readStringUntil('\n');
  msg.trim();

  if (msg.startsWith("RESULTATS:")) {

    msg.replace("RESULTATS:", "");

    int separador = msg.indexOf(',');

    int sistolica = msg.substring(0, separador).toInt();
    int diastolica = msg.substring(separador + 1).toInt();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.drawString("RESULTATS", 40, 20, 4);

    tft.drawString("SYS:", 20, 80, 4);
    tft.drawString(String(sistolica), 120, 70, 6);

    tft.drawString("DIA:", 20, 150, 4);
    tft.drawString(String(diastolica), 120, 140, 6);
  }
}
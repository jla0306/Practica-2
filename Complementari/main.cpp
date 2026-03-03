#include <Arduino.h>

// Definición de pines
const int LED_PIN = 7;
const int BTN_UP_PIN = 18;   // Pulsador para aumentar frecuencia (parpadeo más rápido)
const int BTN_DOWN_PIN = 4; // Pulsador para bajar frecuencia (parpadeo más lento)

// Variables del Timer [cite: 227, 228]
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Variables de control del LED (marcadas como volatile porque se usan en la interrupción)
volatile int led_period_ms = 500; // Estado inicial: 500ms encendido, 500ms apagado
volatile int led_counter = 0;
volatile bool led_state = false;

// Variables para el filtro antirrebote (Debounce)
const int DEBOUNCE_TIME = 50; // El botón debe estar presionado 50ms seguidos para ser válido
volatile int btn_up_counter = 0;
volatile int btn_down_counter = 0;

// Bandera para avisar al loop() de que hay que imprimir información
volatile bool period_changed = false;

// Rutina de Servicio de Interrupción (ISR) del Timer [cite: 229]
void IRAM_ATTR onTimer() {
  // Sección crítica: bloqueamos otras interrupciones mientras calculamos [cite: 230]
  portENTER_CRITICAL_ISR(&timerMux);
 
  // -- 1. CONTROL DEL LED --
  led_counter++;
  if (led_counter >= led_period_ms) {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
    led_counter = 0;
  }

  // -- 2. LECTURA Y ANTIRREBOTE: Pulsador UP (Más rápido) --
  if (digitalRead(BTN_UP_PIN) == LOW) {
    // Si está presionado, incrementamos su contador de milisegundos
    if (btn_up_counter < DEBOUNCE_TIME) {
      btn_up_counter++;
      // Justo cuando llega a 50ms, lo damos por válido (evita que se dispare en bucle si lo mantienes apretado)
      if (btn_up_counter == DEBOUNCE_TIME) {
        if (led_period_ms > 50) { // Límite mínimo (más rápido no)
          led_period_ms -= 50;
          period_changed = true;
        }
      }
    }
  } else {
    // Si se suelta o hay ruido eléctrico (rebote a HIGH), el contador se reinicia a 0
    btn_up_counter = 0;
  }

  // -- 3. LECTURA Y ANTIRREBOTE: Pulsador DOWN (Más lento) --
  if (digitalRead(BTN_DOWN_PIN) == LOW) {
    if (btn_down_counter < DEBOUNCE_TIME) {
      btn_down_counter++;
      if (btn_down_counter == DEBOUNCE_TIME) {
        if (led_period_ms < 2000) { // Límite máximo (más de 2 segundos no)
          led_period_ms += 50;
          period_changed = true;
        }
      }
    }
  } else {
    btn_down_counter = 0;
  }

  // Fin de la sección crítica [cite: 232]
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  // Activamos resistencias internas para no usar externas
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  delay(2000);
  Serial.println("Iniciando control de parpadeo por Timer de 1ms...");

  // Configuración del Timer 0, divisor 80 (para contar microsegundos) [cite: 237]
  timer = timerBegin(0, 80, true);
  // Adjuntamos la función 'onTimer' [cite: 238]
  timerAttachInterrupt(timer, &onTimer, true);
  // Configuramos la alarma para que salte cada 1000 microsegundos (1 milisegundo) [cite: 239]
  timerAlarmWrite(timer, 1000, true);
  // Activamos el timer [cite: 239]
  timerAlarmEnable(timer);
}

void loop() {
  // El loop principal queda prácticamente vacío y libre al 100%
  // Solo se usa para imprimir de forma segura cuando hay un cambio
  if (period_changed) {
    portENTER_CRITICAL(&timerMux);
    int current_period = led_period_ms;
    period_changed = false;
    portEXIT_CRITICAL(&timerMux);
   
    Serial.print("Frecuencia modificada. Nuevo periodo (ms encendido/apagado): ");
    Serial.println(current_period);
  }
}
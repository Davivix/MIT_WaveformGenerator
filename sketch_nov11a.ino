#define BAUD 9600  // 9600 bitů/sekundu po seriové lince

#include <avr/io.h>
#include <math.h>
#include <stdio.h>
#include <util/setbaud.h>
#include <util/delay.h>

#define ch1 PORTF
#define ch2 PORTL

#define PI 3.14159265

// společná vzorkovací frekvence
#define Sampling_freq 5000.0

uint8_t Waveforms[3][360];  // look-up table pro předem vypočítané hodnoty periody průběhů

// 0 ->  čtverec, 1 -> trojúhelník, 2 -> sinus
uint8_t ch1_wavetype = 1;
uint8_t ch2_wavetype = 2;

double ch1_freq = 5;
double ch2_freq = 5;

// amplituda (0 - 255) -> (0 - 5V)
// max doporučená amplituda 204 (4V), jelikož dochází k saturaci někde kolem 4.5V
uint8_t ch1_amp = 51;
uint8_t ch2_amp = 204;

void calculate_square() {
  // čtverec
  uint8_t value = 0;
  for (uint16_t index = 0; index < 360; index++) {
    Waveforms[0][index] = value;
    if (index == 179) value = 255;
  }
}

void calculate_triangle() {
  // trojúhelník
  double value = 0;
  double gain = 255.0 / 180.0;
  for (uint16_t index = 0; index < 360; index++) {
    Waveforms[1][index] = (uint8_t)value;
    value += gain;

    if (value >= 255) {
      gain *= -1;   // klesání
      value = 255;  // korekce chyby
    } else if (value <= 0) {
      gain *= -1;  // stoupání
      value = 0;   // korekce chyby
    }
  }
}

void calculate_sine() {
  // sinus
  double angle_step = PI / 180;
  double angle = 0;
  uint8_t scalar = 255;
  for (uint16_t index = 0; index < 360; index++) {
    double value = ((sin(angle) + 1.0) / 2.0) * scalar;
    Waveforms[2][index] = (uint8_t)value;
    angle += angle_step;
  }
}

void calculate_waveforms() {
  calculate_square();
  calculate_triangle();
  calculate_sine();
}

void UART_init() {
  // 16-bitový registr UBRR, jelikož registry AVR jsou pouze 8-bitové, tak je rozdělen na registry dva. UBRR0H a UBBR0L.
  // registr určuje číslo, kterým když vydělíme frekvenci procesoru, tak získáme Baudovou rychlost
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;  // pro 16MHz procesoru hodnota 103

  UCSR0B = (1 << RXEN0) | (1 << TXEN0);  // povolení pinů UART pro příjem a vysílání

  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // délka datového slova 8 bitů, 1 stop bit
}

void UART_sendchar(char c) {
  while ((UCSR0A & (1 << UDRE0)) == 0)
    ;        // čekání než buffer pro přenos bude prázdný
  UDR0 = c;  // přenos charu přes datový registr UDR0
}

char UART_readchar() {
  while ((UCSR0A & (1 << RXC0)) == 0)
    ;  // čekání na příjem
  return UDR0;
}

void UART_sendstring(char strg[255]) {
  int i = 0;
  while (strg[i] != 0) {
    UART_sendchar(strg[i]);
    i++;
  }
}

void setup() {
  // Definuje porty F a L jako výstupy
  DDRF = 0xFF;
  DDRL = 0xFF;
  calculate_waveforms();
  UART_init();
}

void processCommand(char cmd[]) {
  char target[3];
  char action[16];
  int value = 0;

  // funkce sscanf přečte data ze stringu (cmd) a podle definovaného formátu "%3s_%15[^:]:%d\0"
  // vypíše jednotlivé části stringu do proměnných (target, action, &value)

  // %3s -> první tři chary. (ch1, ch2) do proměnný 'target'
  // %15[^:] -> přečte max 15 charů, čte dokud nenarazí na ':'. (set_freq, set_amp, set_wavetype) do proměnný 'action'
  // %d -> přečte celé číslo za ':', do proměnný 'value'
  // \0 zakončení řetězce nulovým znakem

  if (sscanf(cmd, "%3s_%15[^:]:%d\0", target, action, &value) == 3) {

    // Kontrola platných vstupních hodnot
    if (target[2] != '1' && target[2] != '2') {
      UART_sendstring("CHYBA: Neznámý kanál\r\n");
      return;
    }
    if (value < 0) {
      UART_sendstring("CHYBA: Hodnoty musí být kladné\r\n");
      return;
    }

    uint8_t ch = (target[2] == '1') ? 1 : 2;

    // ---- FREKVENCE ----
    if ((strcmp(action, "set_frequency") == 0) || (strcmp(action, "set_freq") == 0)) {
      if (value <= 1000) {
        if (ch == 1) ch1_freq = value;
        else ch2_freq = value;

        UART_sendstring("Frekvence úspěšně nastavena\r\n");
      } else UART_sendstring("CHYBA: Frekvence větší jak 1KHz nejsou podporovány\r\n");
    }

    // ---- AMPLITUDA ----
    else if (strcmp(action, "set_amplitude") == 0 || strcmp(action, "set_amp") == 0) {
      if (value >= 0 && value <= 255) {
        if (ch == 1) ch1_amp = (uint8_t)value;
        else ch2_amp = (uint8_t)value;

        UART_sendstring("Amplituda úspěšně nastavena\r\n");
      } else UART_sendstring("CHYBA: Neplatná hodnota amplitudy. Platné hodnoty jsou pouze 0-255\r\n");
    }

    // ---- PRŮBĚH ----
    else if (strcmp(action, "set_wavetype") == 0) {
      if (value >= 0 && value <= 2) {
        if (ch == 1) ch1_wavetype = (uint8_t)value;
        else ch2_wavetype = (uint8_t)value;

        UART_sendstring("Typ průběhu úspěšně nastaven\r\n");
      } else UART_sendstring("CHYBA: Neplatná hodnota Typu průběhu. Platné hodnoty jsou pouze 0-2\r\n");
    } else {
      UART_sendstring("CHYBA: Neznámý příkaz.\r\n");
    }
  } else {
    UART_sendstring("CHYBA: Neplatný formát. Platný formát např. ch1_set_amp:204 nebo ch2_set_freq:2.5\r\n");
  }
}

char input_buffer[32];
uint8_t input_pos_index = 0;

int main() {
  setup();

  // 'fáze signálu'
  double ch1_phase_index = 0;
  double ch2_phase_index = 0;

  while (1) {

    // pokud je byte připraven pro přijetí v UART bufferu
    if (UCSR0A & (1 << RXC0)) {
      char c = UART_readchar();  // přečte byte a odstraní ho z UART bufferu

      // pokud je char nový řádek (uživatel zmáčknul ENTER), tedy příkaz je kompletní
      if (c == '\r' || c == '\n') {
        if (input_pos_index > 0) {
          input_buffer[input_pos_index] = '\0';  // zakončení řetězce nulovým znakem
          processCommand(input_buffer);          // zpracování příkazu
          input_pos_index = 0;                   // reset indexu bufferu pro přiští příkaz
        }
      }
      // přidá char na další pozici bufferu. Zárověň zajišťuje, aby nedošlo k přetečení bufferu
      else if (input_pos_index < sizeof(input_buffer) - 1) {
        input_buffer[input_pos_index] = c;
        input_pos_index++;
      }
    }

    // získání hodnot pro zápis na porty z look-up tablu průběhů
    // wavetype: 0-2
    // phase_index: 0-359
    // amp: 0-255
    uint8_t ch1_out = (Waveforms[ch1_wavetype][(int)ch1_phase_index] * ch1_amp) / 255;
    uint8_t ch2_out = (Waveforms[ch2_wavetype][(int)ch2_phase_index] * ch2_amp) / 255;

    // zápis hodnot na porty
    ch1 = ch1_out;
    ch2 = ch2_out;

    // kolikrát se musí "delay()" vykonat pro 1 periodu signálu
    double ch1_delay = Sampling_freq / ch1_freq;
    double ch2_delay = Sampling_freq / ch2_freq;

    // inkrementace indexu 'fáze signálu'
    double ch1_index_gain = 360.0 / ch1_delay;
    double ch2_index_gain = 360.0 / ch2_delay;

    // reset periody
    ch1_phase_index += ch1_index_gain;
    if (ch1_phase_index > 359) ch1_phase_index -= 359;

    ch2_phase_index += ch2_index_gain;
    if (ch2_phase_index > 359) ch2_phase_index -= 359;

    if (Sampling_freq > 1000) _delay_us(1000000.0 / Sampling_freq);
    else _delay_ms(1000.0 / Sampling_freq);
  }

  return 0;
}

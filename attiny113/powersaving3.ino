/*
================================================================================
POWER TUNING GUIDE (READ THIS FIRST)
================================================================================

These are the ONLY areas you should change to reduce power safely:

-------------------------
1. TRANSMIT FREQUENCY
-------------------------
#define WDT_CYCLES

- Each watchdog cycle ≈ 8 seconds
- Transmission interval = WDT_CYCLES × 8s

Examples:
  1   → ~8 seconds
  8   → ~64 seconds
  30  → ~4 minutes
  225 → ~30 minutes

-------------------------
2. NUMBER OF RETRIES
-------------------------
#define TX_RETRIES

3 → safest
2 → good compromise
1 → lowest power

-------------------------
3. RADIO BOOT TIME
-------------------------
_delay_ms(2000);

2000 ms → very safe
1500 ms → usually fine
1000 ms → possible

-------------------------
4. GAP BETWEEN RETRIES
-------------------------
_delay_ms(300);

300 ms → safe
200 ms → often fine

-------------------------
5. POST-TRANSMIT DELAY
-------------------------
_delay_ms(1000);

1000 ms → safe
700 ms → often fine

-------------------------
6. ADC USAGE
-------------------------
_delay_ms(5);

5 ms → safe
2 ms → often fine

================================================================================
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#ifndef WDIE
#define WDIE 6
#endif

// ================= CONFIG =================
#define TX_PIN     PB3
#define RADIO_PWR  PB1
#define TRAP_PIN   PB4
#define VBAT_ADC   1

// ================= POWER TUNING =================
#define TX_RETRIES 3

// 225 × 8s ≈ 30 minutes
#define WDT_CYCLES 225

// ================= GLOBALS =================
volatile uint8_t wdtCounter = 0;

uint16_t lowestLoadedADC = 0;
uint8_t firstTransmission = 1;

// ================= WATCHDOG =================
ISR(WDT_vect) {
    wdtCounter++;
}

void setupWatchdog() {

    cli();

    MCUSR &= ~(1 << WDRF);

    WDTCR |= (1 << WDCE) | (1 << WDE);

    // ~8 second watchdog
    WDTCR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);

    sei();
}

// ================= SLEEP =================
void goToSleep() {

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    sleep_enable();

    sleep_cpu();

    sleep_disable();
}

// ================= UART =================
void txBit(uint8_t b) {

    if (b) PORTB |= (1 << TX_PIN);
    else   PORTB &= ~(1 << TX_PIN);

    _delay_us(99);
}

void txByte(uint8_t b) {

    txBit(0);

    for (uint8_t i = 0; i < 8; i++) {

        txBit(b & 1);

        b >>= 1;
    }

    txBit(1);
}

void txString(const char *s) {

    while (*s) txByte(*s++);
}

void txInt(uint16_t val) {

    char buf[6];

    uint8_t i = 0;

    if (val == 0) {

        txByte('0');

        return;
    }

    while (val > 0) {

        buf[i++] = '0' + (val % 10);

        val /= 10;
    }

    while (i--) txByte(buf[i]);
}

// ================= ADC =================
uint16_t readADC() {

    ADMUX = VBAT_ADC;

    ADMUX |= (1 << REFS0);

    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);

    _delay_ms(5);

    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC));

    return ADC;
}

// ================= SEND =================
void sendData() {

    uint8_t trap = (PINB & (1 << TRAP_PIN)) ? 0 : 1;

    // ===== POWER RADIO =====
    PORTB |= (1 << RADIO_PWR);

    // ===== RADIO BOOT TIME =====
    for (uint16_t i = 0; i < 2000; i++) {
        _delay_ms(1);
    }

    // ===== TRACK LOWEST ADC DURING LOAD =====
    uint16_t currentLowest = 1023;

    for (uint8_t i = 0; i < TX_RETRIES; i++) {

        // ===== STATUS CHARACTER =====
        if (trap) txByte('T');
        else      txByte('S');

        // ===== SEND PREVIOUS LOWEST ADC =====
        if (firstTransmission) {
            txString("0");
        } else {
            txInt(lowestLoadedADC);
        }

        txString("\r\n");

        // ===== SAMPLE ADC DURING ACTIVE RADIO LOAD =====
        for (uint16_t d = 0; d < 300; d++) {

            uint16_t adc = readADC();

            if (adc < currentLowest) {
                currentLowest = adc;
            }

            _delay_ms(1);
        }
    }

    // ===== FINAL LOAD SAMPLING =====
    for (uint16_t d = 0; d < 1000; d++) {

        uint16_t adc = readADC();

        if (adc < currentLowest) {
            currentLowest = adc;
        }

        _delay_ms(1);
    }

    // ===== POWER RADIO OFF =====
    PORTB &= ~(1 << RADIO_PWR);

    // ===== SAVE LOWEST ADC FOR NEXT TRANSMISSION =====
    lowestLoadedADC = currentLowest;

    firstTransmission = 0;
}

// ================= SETUP =================
void setup() {

    // ===== UART TX =====
    DDRB |= (1 << TX_PIN);

    PORTB |= (1 << TX_PIN);

    // ===== RADIO POWER =====
    DDRB |= (1 << RADIO_PWR);

    PORTB &= ~(1 << RADIO_PWR);

    // ===== TRAP INPUT =====
    DDRB &= ~(1 << TRAP_PIN);

    PORTB |= (1 << TRAP_PIN);

    setupWatchdog();

    // ===== SEND ON BOOT =====
    sendData();
}

// ================= MAIN =================
int main(void) {

    setup();

    while (1) {

        if (wdtCounter >= WDT_CYCLES) {

            wdtCounter = 0;

            sendData();
        }

        goToSleep();
    }
}

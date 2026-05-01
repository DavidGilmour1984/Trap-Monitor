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
  1  → ~8 seconds (testing)
  8  → ~64 seconds (~1 minute)
  30 → ~4 minutes
  60 → ~8 minutes

👉 BIGGEST power saving lever

-------------------------
2. NUMBER OF RETRIES
-------------------------
#define TX_RETRIES

- Sends packet multiple times for reliability

3 → safest (current)
2 → good compromise
1 → lowest power (test carefully)

👉 Reduces radio ON time directly

-------------------------
3. RADIO BOOT TIME
-------------------------
_delay_ms(2000);

- Time given for radio to power up

2000 ms → very safe (current)
1500 ms → usually fine
1000 ms → possible
<800 ms → risky

👉 Second biggest power saving

-------------------------
4. GAP BETWEEN RETRIES
-------------------------
_delay_ms(300);

- Prevents packet overlap

300 ms → safe
200 ms → often fine
150 ms → test
<100 ms → likely to break

👉 Medium impact

-------------------------
5. POST-TRANSMIT DELAY
-------------------------
_delay_ms(1000);

- Lets radio finish sending before power off

1000 ms → safe
700 ms → often fine
500 ms → test
<400 ms → risky

👉 Medium impact

-------------------------
6. ADC USAGE
-------------------------
_delay_ms(5);

- Settling time for ADC

5 ms → safe
2 ms → often fine
1 ms → test

👉 Small power saving

-------------------------
7. UART SPEED (ADVANCED)
-------------------------
_delay_us(99);

- Bit timing for 9600 baud

DO NOT CHANGE unless recalibrating

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

// 🔧 POWER TUNING ZONE 1
#define TX_RETRIES 3          // ↓ reduce to 2 → then 1

// 🔧 POWER TUNING ZONE 2
#define WDT_CYCLES 1          // ↑ increase for less frequent transmissions

// ================= GLOBALS =================
volatile uint8_t wdtCounter = 0;

// ================= WATCHDOG =================
ISR(WDT_vect) {
    wdtCounter++;
}

void setupWatchdog() {
    cli();
    MCUSR &= ~(1 << WDRF);

    WDTCR |= (1 << WDCE) | (1 << WDE);
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

    // ⚠️ DO NOT CHANGE unless recalibrating UART
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

    // 🔧 POWER TUNING ZONE 6
    _delay_ms(5);    // ↓ can try 2 ms

    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    return ADC;
}

// ================= SEND =================
void sendData() {

    uint8_t trap = (PINB & (1 << TRAP_PIN)) ? 0 : 1;
    uint16_t adc = readADC();

    PORTB |= (1 << RADIO_PWR);

    // 🔧 POWER TUNING ZONE 3
    for (uint16_t i = 0; i < 2000; i++) _delay_ms(1);   // ↓ try 1500 → 1000

    for (uint8_t i = 0; i < TX_RETRIES; i++) {

        if (trap) txByte('T');
        else      txByte('S');

        txInt(adc);

        txString("\r\n");

        // 🔧 POWER TUNING ZONE 4
        for (uint16_t d = 0; d < 300; d++) _delay_ms(1);   // ↓ try 200
    }

    // 🔧 POWER TUNING ZONE 5
    for (uint16_t d = 0; d < 1000; d++) _delay_ms(1);   // ↓ try 700 → 500

    PORTB &= ~(1 << RADIO_PWR);
}

// ================= SETUP =================
void setup() {

    DDRB |= (1 << TX_PIN);
    PORTB |= (1 << TX_PIN);

    DDRB |= (1 << RADIO_PWR);
    PORTB &= ~(1 << RADIO_PWR);

    DDRB &= ~(1 << TRAP_PIN);
    PORTB |= (1 << TRAP_PIN);

    setupWatchdog();

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

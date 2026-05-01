#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

// ================= FIX FOR MICROCORE =================
#ifndef WDIE
#define WDIE 6
#endif

// ================= CONFIG =================
#define TX_PIN     PB3
#define RADIO_PWR  PB1
#define TRAP_PIN   PB4
#define VBAT_ADC   1   // PB2

#define TX_RETRIES 3
#define WDT_CYCLES 1   // ~64s (~1 min)

// ================= GLOBALS =================
volatile uint8_t wdtCounter = 0;
uint16_t bootCount = 0;

// ================= WATCHDOG =================
ISR(WDT_vect) {
    wdtCounter++;
}

void setupWatchdog() {

    cli();

    MCUSR &= ~(1 << WDRF);

    // Enable timed change
    WDTCR |= (1 << WDCE) | (1 << WDE);

    // Interrupt mode (~8s)
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

// ================= UART (CALIBRATED) =================
void txBit(uint8_t b) {
    if (b) PORTB |= (1 << TX_PIN);
    else   PORTB &= ~(1 << TX_PIN);
    _delay_us(99);  // calibrated
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
    ADMUX |= (1 << REFS0); // 1.1V reference

    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);

    _delay_ms(5);

    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    return ADC;
}

// ================= SEND =================
void sendData() {

    uint8_t trap = (PINB & (1 << TRAP_PIN)) ? 0 : 1;
    uint16_t adc = readADC();

    // Approx voltage ×10
    uint16_t v = (adc * 22) / 10;

    // Radio ON
    PORTB |= (1 << RADIO_PWR);
    for (uint16_t i = 0; i < 2000; i++) _delay_ms(1);

    for (uint8_t i = 0; i < TX_RETRIES; i++) {

        txString("COUNT,");
        txInt(bootCount);
        txByte(',');

        if (trap) txString("TRIGGERED,");
        else      txString("OK,");

        txInt(v / 10);
        txByte('.');
        txByte('0' + (v % 10));

        txString("\r\n");

        for (uint16_t d = 0; d < 300; d++) _delay_ms(1);
    }

    for (uint16_t d = 0; d < 1000; d++) _delay_ms(1);

    // Radio OFF
    PORTB &= ~(1 << RADIO_PWR);
}

// ================= SETUP =================
void setup() {

    // TX output
    DDRB |= (1 << TX_PIN);
    PORTB |= (1 << TX_PIN);

    // Radio power output
    DDRB |= (1 << RADIO_PWR);
    PORTB &= ~(1 << RADIO_PWR);

    // Trap input pullup
    DDRB &= ~(1 << TRAP_PIN);
    PORTB |= (1 << TRAP_PIN);

    setupWatchdog();

    bootCount++;

    // Send on power-up
    sendData();
}

// ================= MAIN =================
int main(void) {

    setup();

    while (1) {

        if (wdtCounter >= WDT_CYCLES) {

            wdtCounter = 0;
            bootCount++;

            sendData();
        }

        goToSleep();
    }
}

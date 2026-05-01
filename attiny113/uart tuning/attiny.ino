#include <avr/io.h>
#include <util/delay.h>

// ================= CONFIG =================
#define TX_PIN PB3

// Sweep range (you changed this during testing)
#define START_DELAY 94
#define END_DELAY   108

// ================= UART =================
void txBit(uint8_t b, uint8_t d) {
    if (b) PORTB |= (1 << TX_PIN);
    else   PORTB &= ~(1 << TX_PIN);

    for (uint8_t i = 0; i < d; i++) _delay_us(1);
}

void txByte(uint8_t b, uint8_t d) {

    txBit(0, d); // start

    for (uint8_t i = 0; i < 8; i++) {
        txBit(b & 1, d);
        b >>= 1;
    }

    txBit(1, d); // stop
}

void txString(const char *s, uint8_t d) {
    while (*s) txByte(*s++, d);
}

// ================= MAIN =================
int main(void) {

    DDRB |= (1 << TX_PIN);
    PORTB |= (1 << TX_PIN);

    while (1) {

        for (uint8_t delay = START_DELAY; delay <= END_DELAY; delay++) {

            // Send marker with delay value
            txString("D", delay);
            txByte('0' + (delay / 100), delay);
            txByte('0' + ((delay / 10) % 10), delay);
            txByte('0' + (delay % 10), delay);
            txByte(':', delay);

            // Send test string
            txString("HELLO\r\n", delay);

            // Wait so ESP32 can evaluate
            for (uint16_t i = 0; i < 500; i++) _delay_ms(1);
        }
    }
}

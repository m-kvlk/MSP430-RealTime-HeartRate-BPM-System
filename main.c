#include <msp430.h>

// ---- Konfigürasyon Sabitleri ----
#define MIN_BEAT_INTERVAL 300    // ms
#define MAX_BPM 140
#define MIN_BPM 40
#define BUFFER_SIZE 8
#define DETECTION_DELTA 5
#define AVG_SIZE 6               // Ortalama alınacak BPM sayısı

// ---- Global Değişkenler ----
volatile unsigned long millis = 0;
volatile unsigned long last_beat_time = 0;

unsigned int adc_buffer[BUFFER_SIZE] = {0};
unsigned char adc_index = 0;
unsigned int prev_adc = 0;

unsigned char pulse_detected = 0;

// BPM ortalama için
unsigned int bpm_history[AVG_SIZE] = {0};
unsigned char bpm_index = 0;

// ---- Timer (1 ms) ----
void setup_timer() {
    TA0CCR0 = 1000 - 1;
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL_2 + MC_1 + TACLR;
}

// ---- UART ----
void setup_uart() {
    P1SEL |= BIT1 + BIT2;
    P1SEL2 |= BIT1 + BIT2;
    UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS0;
    UCA0CTL1 &= ~UCSWRST;
}

void uart_send_char(char c) {
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = c;
}

void uart_send_string(const char *str) {
    while (*str) uart_send_char(*str++);
}

void send_bpm(unsigned int bpm) {
    char buf[20];
    int i = 0;

    buf[i++] = 'B'; buf[i++] = 'P'; buf[i++] = 'M'; buf[i++] = ':'; buf[i++] = ' ';

    if (bpm >= 100) {
        buf[i++] = (bpm / 100) + '0';
        bpm %= 100;
    }
    if (bpm >= 10 || i > 5) {
        buf[i++] = (bpm / 10) + '0';
        bpm %= 10;
    }
    buf[i++] = bpm + '0';

    buf[i++] = '\r';
    buf[i++] = '\n';
    buf[i] = '\0';

    uart_send_string(buf);
}

// ---- Ortalama filtreleme ----
unsigned int get_smoothed_adc() {
    unsigned long sum = 0;
    int i;
    for (i = 0; i < BUFFER_SIZE; i++) {
        sum += adc_buffer[i];
    }
    return sum / BUFFER_SIZE;
}

unsigned int get_bpm_average() {
    unsigned long sum = 0;
    int i;
    for (i = 0; i < AVG_SIZE; i++) {
        sum += bpm_history[i];
    }
    return sum / AVG_SIZE;
}

// ---- Timer ISR ----
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A_ISR(void) {
    millis++;
}

// ---- Main ----
int main(void) {
    WDTCTL = WDTPW + WDTHOLD;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    ADC10CTL1 = INCH_3;
    ADC10CTL0 = ADC10SHT_3 + ADC10ON;
    ADC10AE0 |= BIT3;

    setup_timer();
    setup_uart();
    __enable_interrupt();

    while (1) {
        ADC10CTL0 |= ENC + ADC10SC;
        while (ADC10CTL1 & ADC10BUSY);
        unsigned int adc_value = ADC10MEM;

        adc_buffer[adc_index++] = adc_value;
        if (adc_index >= BUFFER_SIZE) adc_index = 0;

        unsigned int smooth = get_smoothed_adc();
        unsigned int threshold = smooth + DETECTION_DELTA;

        // Pulse yükselişi tespiti
        if ((adc_value > threshold) && (prev_adc <= threshold) && !pulse_detected) {
            pulse_detected = 1;

            unsigned long interval = millis - last_beat_time;
            if (interval > MIN_BEAT_INTERVAL) {
                last_beat_time = millis;
                unsigned int bpm = 60000 / interval;

                if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
                    bpm_history[bpm_index++] = bpm;
                    if (bpm_index >= AVG_SIZE) bpm_index = 0;

                    unsigned int avg_bpm = get_bpm_average();
                    send_bpm(avg_bpm);
                }
            }
        }

        // Düşen kenarda reset
        if ((adc_value < smooth) && pulse_detected) {
            pulse_detected = 0;
        }

        prev_adc = adc_value;
        __delay_cycles(10000);  // ~10 ms
    }
}

/********************************************
 *
 * Posture Pilot 
 * Arduino Uno R3 / ATmega328P
 * LCD shield + HC-SR04 + MP3-TF-16P
 *
 ********************************************/

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include "lcd.h"
#include "adc.h"

#define FOSC 16000000UL

#define TRIG_PIN PD3
#define ECHO_PIN PD2
#define MP3_TX_PIN PB3

#define SELECT_MIN 600 / 4
#define SELECT_MAX 850 / 4

#define SLOUCH_THRESHOLD_MM 80
#define ALERT_DELAY_COUNT 20

volatile uint8_t echo_high = 0;
volatile uint8_t pulse_ready = 0;
volatile uint8_t pulse_timeout = 0;
volatile uint16_t echo_count = 0;

uint16_t baseline_distance_mm = 0;
uint16_t current_distance_mm = 0;

uint8_t calibrated = 0;
uint8_t alert_counter = 0;

const uint16_t max_echo_count = 47000;

void timer1_stop(void);
void timer1_start_echo(void);
void trigger_ultrasonic(void);
uint16_t echo_count_to_mm(uint16_t count);

uint8_t button_select_pressed(void);

void mp3_send_byte(uint8_t b);
void mp3_send_command(uint8_t cmd, uint16_t param);
void mp3_play(uint16_t track);
void mp3_set_volume(uint8_t volume);

void show_start_screen(void);
void show_calibrated_screen(void);
void show_status(uint8_t slouching, uint8_t score);

int main(void)
{
    uint16_t diff_mm;
    uint8_t score;
    uint8_t slouching;

    lcd_init();
    adc_init();

    DDRD |= (1 << TRIG_PIN);
    DDRD &= ~(1 << ECHO_PIN);

    DDRB |= (1 << MP3_TX_PIN);
    PORTB |= (1 << MP3_TX_PIN);

    PORTD &= ~(1 << TRIG_PIN);

    PCMSK2 |= (1 << PCINT18);
    PCICR |= (1 << PCIE2);

    sei();

    _delay_ms(1000);
    mp3_set_volume(22);

    show_start_screen();

    while (1) {
        trigger_ultrasonic();
        _delay_ms(70);

        if (pulse_ready) {
            pulse_ready = 0;
            current_distance_mm = echo_count_to_mm(echo_count);
        }

        if (pulse_timeout) {
            pulse_timeout = 0;
            current_distance_mm = 0;
        }

        if (button_select_pressed() && current_distance_mm > 0) {
            baseline_distance_mm = current_distance_mm;
            calibrated = 1;
            alert_counter = 0;

            show_calibrated_screen();
            mp3_play(2);
            _delay_ms(1500);
        }

        if (!calibrated) {
            show_start_screen();
            _delay_ms(300);
            continue;
        }

        if (current_distance_mm > baseline_distance_mm) {
            diff_mm = current_distance_mm - baseline_distance_mm;
        }
        else {
            diff_mm = 0;
        }

        if (diff_mm > SLOUCH_THRESHOLD_MM) {
            slouching = 1;
        }
        else {
            slouching = 0;
        }

        if (diff_mm >= 125) {
            score = 0;
        }
        else {
            score = 100 - ((diff_mm / 10) * 8);
        }

        show_status(slouching, score);

        if (slouching) {
            alert_counter++;

            if (alert_counter > ALERT_DELAY_COUNT) {
                mp3_play(1);
                alert_counter = 0;
            }
        }
        else {
            alert_counter = 0;
        }

        _delay_ms(300);
    }
}

void trigger_ultrasonic(void)
{
    pulse_ready = 0;
    pulse_timeout = 0;
    echo_high = 0;

    PORTD &= ~(1 << TRIG_PIN);
    _delay_us(2);

    PORTD |= (1 << TRIG_PIN);
    _delay_us(10);

    PORTD &= ~(1 << TRIG_PIN);
}

void timer1_stop(void)
{
    TCCR1A = 0;
    TCCR1B = 0;
    TIMSK1 = 0;
}

void timer1_start_echo(void)
{
    timer1_stop();

    TCNT1 = 0;
    OCR1A = max_echo_count;

    TCCR1B = (1 << CS11);
    TIMSK1 = (1 << OCIE1A);
}

uint16_t echo_count_to_mm(uint16_t count)
{
    uint32_t mm;

    mm = (uint32_t)count * 10;
    mm = (mm + 58) / 116;

    return (uint16_t)mm;
}

uint8_t button_select_pressed(void)
{
    uint8_t value;

    value = adc_sample(0);

    if (value > SELECT_MIN && value < SELECT_MAX) {
        _delay_ms(40);

        value = adc_sample(0);

        if (value > SELECT_MIN && value < SELECT_MAX) {
            while (adc_sample(0) > SELECT_MIN && adc_sample(0) < SELECT_MAX) {
            }

            return 1;
        }
    }

    return 0;
}

void mp3_send_byte(uint8_t b)
{
    uint8_t i;

    PORTB &= ~(1 << MP3_TX_PIN);
    _delay_us(104);

    for (i = 0; i < 8; i++) {
        if (b & 0x01) {
            PORTB |= (1 << MP3_TX_PIN);
        }
        else {
            PORTB &= ~(1 << MP3_TX_PIN);
        }

        _delay_us(104);
        b >>= 1;
    }

    PORTB |= (1 << MP3_TX_PIN);
    _delay_us(104);
}

void mp3_send_command(uint8_t cmd, uint16_t param)
{
    uint8_t packet[10];
    uint16_t checksum;
    uint8_t i;

    packet[0] = 0x7E;
    packet[1] = 0xFF;
    packet[2] = 0x06;
    packet[3] = cmd;
    packet[4] = 0x00;
    packet[5] = (param >> 8) & 0xFF;
    packet[6] = param & 0xFF;

    checksum = 0 - (packet[1] + packet[2] + packet[3] +
                    packet[4] + packet[5] + packet[6]);

    packet[7] = (checksum >> 8) & 0xFF;
    packet[8] = checksum & 0xFF;
    packet[9] = 0xEF;

    for (i = 0; i < 10; i++) {
        mp3_send_byte(packet[i]);
    }

    _delay_ms(100);
}

void mp3_play(uint16_t track)
{
    mp3_send_command(0x12, track);
}

void mp3_set_volume(uint8_t volume)
{
    if (volume > 30) {
        volume = 30;
    }

    mp3_send_command(0x06, volume);
}

void show_start_screen(void)
{
    lcd_writecommand(1);
    lcd_moveto(0, 0);
    lcd_stringout("Posture Pilot");
    lcd_moveto(1, 0);
    lcd_stringout("SELECT calibr.");
}

void show_calibrated_screen(void)
{
    char line[17];

    lcd_writecommand(1);
    lcd_moveto(0, 0);
    lcd_stringout("Calibrated!");

    lcd_moveto(1, 0);
    snprintf(line, sizeof(line), "Base:%3ucm", baseline_distance_mm / 10);
    lcd_stringout(line);
}

void show_status(uint8_t slouching, uint8_t score)
{
    char line[17];

    lcd_writecommand(1);

    lcd_moveto(0, 0);

    if (slouching) {
        lcd_stringout("SLOUCHING!");
    }
    else {
        lcd_stringout("Posture Good");
    }

    lcd_moveto(1, 0);
    snprintf(line, sizeof(line), "D:%3ucm S:%3u%%",
             current_distance_mm / 10,
             score);
    lcd_stringout(line);
}

ISR(PCINT2_vect)
{
    if ((PIND & (1 << ECHO_PIN)) && echo_high == 0) {
        echo_high = 1;
        timer1_start_echo();
    }
    else if (!(PIND & (1 << ECHO_PIN)) && echo_high == 1) {
        echo_count = TCNT1;
        timer1_stop();
        echo_high = 0;
        pulse_ready = 1;
    }
}

ISR(TIMER1_COMPA_vect)
{
    timer1_stop();
    echo_high = 0;
    pulse_timeout = 1;
}

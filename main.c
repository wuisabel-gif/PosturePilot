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

#define SELECT_MIN (600 / 4)
#define SELECT_MAX (850 / 4)

#define DISTANCE_SAMPLE_COUNT 5
#define BASELINE_SAMPLE_COUNT 4
#define SAMPLE_SPACING_MS 25

#define SLOUCH_THRESHOLD_MM 80
#define MAX_SCORE_DELTA_MM 150
#define SLOUCH_TRIGGER_COUNT 3
#define SLOUCH_CLEAR_COUNT 2

#define SLOUCH_AUDIO_COOLDOWN_LOOPS 18
#define BREAK_REMINDER_LOOPS 720

#define TRACK_SLOUCH 1
#define TRACK_CALIBRATED 2
#define TRACK_BREAK 3
#define TRACK_STARTUP 4

volatile uint8_t echo_high = 0;
volatile uint8_t pulse_ready = 0;
volatile uint8_t pulse_timeout = 0;
volatile uint16_t echo_count = 0;

uint16_t baseline_distance_mm = 0;
uint16_t current_distance_mm = 0;

uint8_t calibrated = 0;
uint8_t slouching = 0;
uint8_t slouch_streak = 0;
uint8_t upright_streak = 0;
uint16_t break_counter = 0;
uint8_t audio_cooldown = 0;

const uint16_t max_echo_count = 47000;

void timer1_stop(void);
void timer1_start_echo(void);
void trigger_ultrasonic(void);
uint16_t echo_count_to_mm(uint16_t count);
uint16_t measure_distance_once_mm(void);
uint16_t read_filtered_distance_mm(void);
uint16_t capture_baseline_mm(void);
void sort_u16(uint16_t *values, uint8_t count);
uint8_t calculate_score(uint16_t diff_mm);
void update_posture_state(uint16_t diff_mm);
void handle_audio_events(void);
uint8_t button_select_pressed(void);
void reset_monitoring_state(void);

void mp3_send_byte(uint8_t b);
void mp3_send_command(uint8_t cmd, uint16_t param);
void mp3_play(uint16_t track);
void mp3_set_volume(uint8_t volume);

void show_start_screen(void);
void show_calibrated_screen(void);
void show_status(uint8_t score, uint16_t diff_mm);
void show_sensor_error(void);

int main(void)
{
    uint16_t diff_mm;
    uint8_t score;
    uint16_t baseline_candidate;

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
    mp3_play(TRACK_STARTUP);
    _delay_ms(1200);

    while (1) {
        current_distance_mm = read_filtered_distance_mm();

        if (button_select_pressed()) {
            baseline_candidate = capture_baseline_mm();

            if (baseline_candidate > 0) {
                baseline_distance_mm = baseline_candidate;
                calibrated = 1;

                reset_monitoring_state();
                show_calibrated_screen();
                mp3_play(TRACK_CALIBRATED);
                _delay_ms(1500);
            }
        }

        if (!calibrated) {
            show_start_screen();
            _delay_ms(250);
            continue;
        }

        if (current_distance_mm == 0) {
            show_sensor_error();
            _delay_ms(250);
            continue;
        }

        if (current_distance_mm > baseline_distance_mm) {
            diff_mm = current_distance_mm - baseline_distance_mm;
        }
        else {
            diff_mm = 0;
        }

        score = calculate_score(diff_mm);
        update_posture_state(diff_mm);
        handle_audio_events();
        show_status(score, diff_mm);

        _delay_ms(250);
    }
}

void reset_monitoring_state(void)
{
    slouching = 0;
    slouch_streak = 0;
    upright_streak = 0;
    break_counter = 0;
    audio_cooldown = 0;
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

    return (uint16_t) mm;
}

uint16_t measure_distance_once_mm(void)
{
    trigger_ultrasonic();
    _delay_ms(70);

    if (pulse_ready) {
        pulse_ready = 0;
        return echo_count_to_mm(echo_count);
    }

    if (pulse_timeout) {
        pulse_timeout = 0;
    }

    return 0;
}

void sort_u16(uint16_t *values, uint8_t count)
{
    uint8_t i;
    uint8_t j;
    uint16_t temp;

    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            if (values[j] < values[i]) {
                temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }
}

uint16_t read_filtered_distance_mm(void)
{
    uint16_t samples[DISTANCE_SAMPLE_COUNT];
    uint8_t valid;
    uint8_t attempts;
    uint32_t sum;

    valid = 0;
    attempts = 0;

    while (valid < DISTANCE_SAMPLE_COUNT && attempts < (DISTANCE_SAMPLE_COUNT * 3)) {
        samples[valid] = measure_distance_once_mm();

        if (samples[valid] > 0) {
            valid++;
        }

        attempts++;
        _delay_ms(SAMPLE_SPACING_MS);
    }

    if (valid < 3) {
        return 0;
    }

    sort_u16(samples, valid);

    if (valid >= 5) {
        sum = (uint32_t) samples[1] + samples[2] + samples[3];
        return (uint16_t) (sum / 3);
    }

    if (valid == 4) {
        sum = (uint32_t) samples[1] + samples[2];
        return (uint16_t) (sum / 2);
    }

    return samples[1];
}

uint16_t capture_baseline_mm(void)
{
    uint16_t samples[BASELINE_SAMPLE_COUNT];
    uint8_t i;
    uint32_t sum;

    lcd_writecommand(1);
    lcd_moveto(0, 0);
    lcd_stringout("Sit upright");
    lcd_moveto(1, 0);
    lcd_stringout("Calibrating...");

    _delay_ms(700);

    sum = 0;
    for (i = 0; i < BASELINE_SAMPLE_COUNT; i++) {
        samples[i] = read_filtered_distance_mm();

        if (samples[i] == 0) {
            return 0;
        }

        sum += samples[i];
    }

    return (uint16_t) (sum / BASELINE_SAMPLE_COUNT);
}

uint8_t calculate_score(uint16_t diff_mm)
{
    uint32_t penalty;

    if (diff_mm == 0) {
        return 100;
    }

    if (diff_mm >= MAX_SCORE_DELTA_MM) {
        return 0;
    }

    penalty = (uint32_t) diff_mm * 100;
    penalty /= MAX_SCORE_DELTA_MM;

    return (uint8_t) (100 - penalty);
}

void update_posture_state(uint16_t diff_mm)
{
    uint8_t over_threshold;

    over_threshold = (diff_mm >= SLOUCH_THRESHOLD_MM);

    if (over_threshold) {
        if (slouch_streak < 255) {
            slouch_streak++;
        }
        upright_streak = 0;
    }
    else {
        if (upright_streak < 255) {
            upright_streak++;
        }
        slouch_streak = 0;
    }

    if (!slouching && slouch_streak >= SLOUCH_TRIGGER_COUNT) {
        slouching = 1;
    }
    else if (slouching && upright_streak >= SLOUCH_CLEAR_COUNT) {
        slouching = 0;
    }
}

void handle_audio_events(void)
{
    if (audio_cooldown > 0) {
        audio_cooldown--;
    }

    if (slouching) {
        break_counter = 0;

        if (audio_cooldown == 0) {
            mp3_play(TRACK_SLOUCH);
            audio_cooldown = SLOUCH_AUDIO_COOLDOWN_LOOPS;
        }

        return;
    }

    break_counter++;

    if (break_counter >= BREAK_REMINDER_LOOPS && audio_cooldown == 0) {
        mp3_play(TRACK_BREAK);
        break_counter = 0;
        audio_cooldown = SLOUCH_AUDIO_COOLDOWN_LOOPS;
    }
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

void show_sensor_error(void)
{
    lcd_writecommand(1);
    lcd_moveto(0, 0);
    lcd_stringout("Sensor timeout");
    lcd_moveto(1, 0);
    lcd_stringout("Check chair pos");
}

void show_status(uint8_t score, uint16_t diff_mm)
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
    snprintf(line, sizeof(line), "D:%2ucm S:%3u%%",
             diff_mm / 10,
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

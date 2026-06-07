#include <avr/io.h>

#include "adc.h"


void adc_init(void)
{
    // Initialize the ADC
    ADMUX |= (1 << REFS0);
    ADMUX &= ~(1 << REFS1);

    ADMUX |= (1 << ADLAR);

    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    ADCSRA |= (1 << ADEN);
}

uint8_t adc_sample(uint8_t channel)
{
    // Set ADC input mux bits to 'channel' value
    ADMUX &= 0xF0;            // Clear old MUX bits
    ADMUX |= (channel & 0x0F);

    // Convert an analog input and return the 8-bit result
    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC))
    { 
    }

    return ADCH;

}

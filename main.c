#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdint.h>

#define F_CPU 8000000UL
#define trig PB3
#define echo PB4
#define TX_PIN PB1
#define RX_PIN PB0

volatile float duration, distance;

void softSerial_init()
{
    DDRB |= (1 << TX_PIN);  // Set TX pin as output
    PORTB |= (1 << RX_PIN); // Enable pull-up on RX pin
    PORTB |= (1 << TX_PIN); // Set TX idle state HIGH
}

void softSerial_write(char data)
{
    uint8_t i;
    // Start bit
    PORTB &= ~(1 << TX_PIN);
    _delay_us(104);
    // Data bits (LSB first)
    for (i = 0; i < 8; i++)
    {
        if (data & (1 << i))
            PORTB |= (1 << TX_PIN);
        else
            PORTB &= ~(1 << TX_PIN);
        _delay_us(104);
    }
    // Stop bit
    PORTB |= (1 << TX_PIN);
    _delay_us(104);
}

void softSerial_writeString(const char *str)
{
    while (*str)
    {
        softSerial_write(*str++);
    }
}

void intToString(int num, char *buffer)
{
    int i = 0, isNegative = 0;
    if (num < 0)
    {
        isNegative = 1;
        num = -num;
    }
    // Handle 0 explicitly
    if (num == 0)
    {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }
    // Convert digits in reverse
    while (num != 0)
    {
        buffer[i++] = (num % 10) + '0';
        num /= 10;
    }
    // Add negative sign
    if (isNegative)
        buffer[i++] = '-';
    buffer[i] = '\0';
    // Reverse the string
    for (int start = 0, end = i - 1; start < end; start++, end--)
    {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
    }
}

void floatToString(float num, char *buffer)
{
    int i = 0;
    if (num < 0)
    {
        buffer[i++] = '-';
        num = -num;
    }

    int integerPart = (int)num;
    float fractionalPart = num - integerPart;

    // Round and convert fractional part
    fractionalPart += 0.00005; // Rounding adjustment
    int fractional = (int)(fractionalPart * 10000.0);

    // Handle overflow
    if (fractional >= 10000)
    {
        integerPart++;
        fractional = 0;
    }

    // Convert integer part
    char intBuffer[20];
    intToString(integerPart, intBuffer);
    strcpy(&buffer[i], intBuffer);
    i += strlen(intBuffer);

    // Add decimal point
    buffer[i++] = '.';

    // Convert fractional part with leading zeros
    char fracBuffer[5];
    intToString(fractional, fracBuffer);
    int digits = strlen(fracBuffer);
    for (int j = 0; j < 4 - digits; j++)
        buffer[i++] = '0';
    strcpy(&buffer[i], fracBuffer);
    i += digits;
    buffer[i] = '\0';
}

unsigned long pulseIn(uint8_t pin)
{
    // Timeout counters (adjust based on required max duration)
    unsigned long timeout = 1000000UL;

    // Wait for the pin to go HIGH with timeout
    while (!(PINB & (1 << pin)))
    {
        if (--timeout == 0)
            return 0;
    }

    // Configure Timer0
    TCCR0A = 0;          // Normal mode
    TCCR0B = 0;          // Stop timer
    TCNT0 = 0;           // Reset timer count
    TIFR |= (1 << TOV0); // Clear overflow flag

    unsigned long overflow_count = 0;
    TCCR0B = (1 << CS01); // Start timer with prescaler 8 (1µs/tick @8MHz)

    // Wait for the pin to go LOW with timeout
    timeout = 1000000UL;
    while (PINB & (1 << pin))
    {
        if (TIFR & (1 << TOV0))
        { // Check timer overflow
            overflow_count++;
            TIFR |= (1 << TOV0); // Clear overflow flag
        }
        if (--timeout == 0)
        {
            TCCR0B = 0; // Stop timer on timeout
            return 0;
        }
    }

    TCCR0B = 0; // Stop timer
    unsigned long duration = (overflow_count << 8) + TCNT0;

    return duration; // Returns duration in microseconds
}

int main()
{
    softSerial_init();

    DDRB |= (1 << trig);

    while (1)
    {
        PORTB &= ~(1 << trig);
        _delay_us(2);
        PORTB |= (1 << trig);
        _delay_us(10);
        PORTB &= ~(1 << trig);

        duration = pulseIn(echo);
        distance = (duration * .0343) / 2;

        softSerial_writeString("Distance: ");
        char buffer[20];
        floatToString(distance, buffer);
        softSerial_writeString(buffer);
        softSerial_writeString("cm");
        softSerial_writeString("\r\n");

        _delay_ms(100);
    }

    return 0;
}
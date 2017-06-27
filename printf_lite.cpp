/* 
    This code should be pasted within the files where this function is needed.
    This function will not create any code conflicts.
    
    The function call is similar to printf: ardprintf("Test %d %s", 25, "string");

    To print the '%' character, use '%%'

    This code was first posted on http://arduino.stackexchange.com/a/201
*/

#ifdef USE_HAL_DRIVER
	#include "stm32f1xx_hal.h"
	#include "usb_device.h"
	#include "usbd_cdc_if.h"
#endif

#ifdef TRACE
	#include "newlib/diag/trace_impl.h"
#endif

#ifdef ARDUINO
	#include <Arduino.h>
	#include <HardwareSerial.h>
#endif

#include "printf_lite.h"

#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>

#ifdef USE_HAL_DRIVER
extern UART_HandleTypeDef huart1;
#endif


/*
 * if you don't need output buffering for performance (makes a big difference with semihosting), you can lower that to 1 or any value you want.
 * Anyway, buffer isn't statically allocated so it won't use permanent RAM. If you really have a tiny tiny tiny heap, you might lower this.
 */
#define BUF_MAX 60

/*
 * June 2017, avr-gcc 4.9.2-atmel3.5.4-arduino2 :
 * Float support increase 828 bytes of text and 16 bytes of data
 */
#define FLOAT_SUPPORT 1

typedef struct  {
	int *newline;
	char buf[BUF_MAX];
	int bufIdx;
	transmitBufCallBackType transmitBufCallBack;
	int inDirective;
	int l_modifier;
	int width_specifier;
	char pad_char;

} PrintfParams;

static void print_timestamp(PrintfParams* printfParams);


static void print_char(char c, PrintfParams* printfParams)
{
	if ( printfParams->newline )
	{
		if ( *printfParams->newline ) {
			*printfParams->newline = 0; // to do BEFORE call to printTimeStamp
			print_timestamp(printfParams);
		}
//		if ( printfParams->bufIdx == BUF_MAX ) {
//			printfParams->transmitBufCallBack(printfParams->buf, printfParams->bufIdx);
//			printfParams->bufIdx = 0;
//		}
		printfParams->buf[(printfParams->bufIdx)++] = c;
		if ( c == '\n' ) {
			*printfParams->newline = 1;
		}
	}else{
		printfParams->buf[(printfParams->bufIdx)++] = c;
	}
	if ( printfParams->bufIdx == BUF_MAX ) {
		printfParams->transmitBufCallBack(printfParams->buf, printfParams->bufIdx);
		printfParams->bufIdx = 0;
	}
}

static void print_string(const char* s, PrintfParams* printfParams)
{
	while ( *s ) print_char(*s++, printfParams);
}

#ifdef ARDUINO

static void print_Fstring(const char* s, PrintfParams* printfParams)
{
	PGM_P p_to_print = reinterpret_cast<PGM_P>(s);
	unsigned char c_to_print = pgm_read_byte(p_to_print++);
	while ( c_to_print != 0 ) {
		print_char(c_to_print, printfParams);
		c_to_print = pgm_read_byte(p_to_print++);
	}
}

#endif

/* Jief : I found this here : https://github.com/cjlano/tinyprintf/blob/master/tinyprintf.c. Thanks CJlano */
static void print_long(long long v, int base, PrintfParams* printfParams)
{
    int n = 0;
    long long int d = 1;
//    char *bf = p->bf;
    int nbDigits = 1;
    while (v / d >= base) {
    	d *= base;
    	nbDigits += 1;
    }
    while ( printfParams->width_specifier > nbDigits ) {
    	print_char(printfParams->pad_char, printfParams);
    	nbDigits += 1;
    }
    while (d != 0) {
        int dgt = v / d;
        v %= d;
        d /= base;
        if (n || dgt > 0 || d == 0) {
            print_char(dgt + (dgt < 10 ? '0' : (0 ? 'A' : 'a') - 10), printfParams);
            n += 1;
        }
    }
}

static void print_timestamp(PrintfParams* printfParams)
{
	#ifdef USE_HAL_DRIVER
		uint32_t ms = HAL_GetTick();
	#endif
	#ifdef ARDUINO
		uint32_t ms = millis();
	#endif
	uint32_t s = ms/1000;
	uint32_t m = s/60;
	uint32_t h = m/60;
	m %= 60;
	s %= 60;
	ms %= 1000;

	// ardprintf seems not reentrant (because of va_list ?), so using basic print.

	char pad_char = printfParams->pad_char;
	int width_specifier = printfParams->width_specifier;
	printfParams->pad_char = '0';
	printfParams->width_specifier = 3;
	print_long(h, 10, printfParams);
	print_char(':', printfParams);
	printfParams->width_specifier = 2;
	print_long(m, 10, printfParams);
	print_char(':', printfParams);
	print_long(s, 10, printfParams);
	print_char('.', printfParams);
	printfParams->width_specifier = 3;
	print_long(ms, 10, printfParams);
	print_char(' ', printfParams); // Not using print_Fstring(PSTR(" - "), printfParams); because 3 chars use less space in Flash than a 3 chars strings. Don't ask me why, just reporting what avr-size says.
	print_char('-', printfParams);
	print_char(' ', printfParams);
	printfParams->pad_char = pad_char;
	printfParams->width_specifier = width_specifier;
}

#if FLOAT_SUPPORT == 1
/* Jief : I found this in Arduino code */
/* According to snprintf(),
 *
 * nextafter((double)numeric_limits<long long>::max(), 0.0) ~= 9.22337e+18
 *
 * This slightly smaller value was picked semi-arbitrarily. */
#define LARGE_DOUBLE_TRESHOLD (9.1e18)

/* THIS FUNCTION SHOULDN'T BE USED IF YOU NEED ACCURATE RESULTS.
 *
 * This implementation is meant to be simple and not occupy too much
 * code size.  However, printing floating point values accurately is a
 * subtle task, best left to a well-tested library function.
 *
 * See Steele and White 2003 for more details:
 *
 * http://kurtstephens.com/files/p372-steele.pdf
 */
static void print_float(double number, uint8_t digits, PrintfParams* printfParams)
{
    // Hackish fail-fast behavior for large-magnitude doubles
    if (abs(number) >= LARGE_DOUBLE_TRESHOLD) {
        if (number < 0.0) {
            print_char('-', printfParams);
        }
        print_string("<large double>", printfParams);
    }

    // Handle negative numbers
    if (number < 0.0) {
        print_char('-', printfParams);
        number = -number;
    }

    // Simplistic rounding strategy so that e.g. print(1.999, 2)
    // prints as "2.00"
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; i++) {
        rounding /= 10.0;
    }
    number += rounding;

    // Extract the integer part of the number and print it
    long long int_part = (long long)number;
    double remainder = number - int_part;
	char pad_char = printfParams->pad_char;
	int width_specifier = printfParams->width_specifier;
	printfParams->pad_char = '0';
	printfParams->width_specifier = 1;
    print_long(int_part, 10, printfParams);
	printfParams->pad_char = pad_char;
	printfParams->width_specifier = width_specifier;


    // Print the decimal point, but only if there are digits beyond
    if (digits > 0) {
        print_char('.', printfParams);
    }

    // Extract digits from the remainder one at a time
    while (digits-- > 0) {
        remainder *= 10.0;
        int to_print = (int)remainder;
        print_char(to_print + '0', printfParams);
        remainder -= to_print;
    }
}
#endif

static int printf_handle_format_char(char c, va_list* valist_ptr, PrintfParams* printfParams)
{
	if ( printfParams->inDirective )
	{
		switch(c)
		{
			case '0':
				printfParams->pad_char = '0';
				break;
			case '1'...'9':
				printfParams->width_specifier *= 10;
				printfParams->width_specifier += ( c - '0');
				break;
			case 'd':
				if ( printfParams->l_modifier ) print_long(va_arg(*valist_ptr, long), 10, printfParams);
				else print_long(va_arg(*valist_ptr, int), 10, printfParams);
				printfParams->inDirective = 0;
				break;
			case 'x':
				if ( printfParams->l_modifier ) print_long(va_arg(*valist_ptr, long), 16, printfParams);
				else print_long(va_arg(*valist_ptr, int), 16, printfParams);
				printfParams->inDirective = 0;
				break;
#if FLOAT_SUPPORT == 1
			case 'f':
				if ( printfParams->l_modifier ) print_float(va_arg(*valist_ptr, double), 2, printfParams);
				else print_float(va_arg(*valist_ptr, double), 2, printfParams); // Compiler says : 'float' is promoted to 'double' when passed through '...'
#endif
				printfParams->inDirective = 0;
				break;
			case 'l':
				printfParams->l_modifier = 1;
				break;
			case 'c':
				print_char((char)va_arg(*valist_ptr, int), printfParams); // 'char' is promoted to 'int' when passed through '...'
				printfParams->inDirective = 0;
				break;
			case 's':
				print_string(va_arg(*valist_ptr, char *), printfParams);
				printfParams->inDirective = 0;
				break;
#ifdef ARDUINO
			case 'F':
				print_Fstring(va_arg(*valist_ptr, char *), printfParams);
				printfParams->inDirective = 0;
				break;
#endif
			default:  {
				print_char('%', printfParams);
				print_char(c, printfParams);
				printfParams->inDirective = 0;
			}
		}
	}
	else
	{
		switch(c)
		{
			case '%':
				printfParams->inDirective = 1;
				printfParams->l_modifier = 0;
				printfParams->width_specifier = 0;
				printfParams->pad_char = ' ';
				break;
			default: {
				print_char(c, printfParams);
			}
		}
	}
	return 1;
}

int printf_callback(const char* str, va_list valist, int* newline, transmitBufCallBackType transmitBufCallBack)
{
	PrintfParams printfParams;
	printfParams.newline = newline;
	printfParams.bufIdx = 0;
	printfParams.inDirective = 0;
	printfParams.l_modifier = 0;
	printfParams.width_specifier = 0;
	printfParams.pad_char = ' ';
	printfParams.transmitBufCallBack = transmitBufCallBack;

	int i;
	for( i=0 ;  str[i] != '\0'  ; i++ ) //Iterate over formatting string
	{
		char c = str[i];
		printf_handle_format_char(c, &valist, &printfParams);
	}
	if ( printfParams.bufIdx > 0 ) printfParams.transmitBufCallBack(printfParams.buf, printfParams.bufIdx);
	return 1;
}


#ifdef __USBD_CDC_IF_H
/* ------------------------ USB ------------------------ */

size_t transmitBufUsb(const char* buf, size_t nbyte)
{
	#ifdef DEBUG
		if ( nbyte > BUF_MAX ) {
			__asm volatile ("bkpt 0");
		}
	#endif
//	uint32_t ms = HAL_GetTick() + 3000;
//	while ( HAL_GetTick() < ms  &&  CDC_Transmit_FS((char*)buf, nbyte) == USBD_BUSY ) {}; // If data isn't consumed by the host in 1 ms, it probaly won't be so we give up to not block.


	uint32_t ms = HAL_GetTick() + 3000;
	uint8_t ret = CDC_Transmit_FS((uint8_t*)buf, nbyte);
	if ( ret != USBD_OK )
	{
	    uint32_t now = HAL_GetTick();
		while ( now < ms  &&  ret != USBD_OK ) {
			ret = CDC_Transmit_FS((uint8_t*)buf, nbyte);
			now = HAL_GetTick();
		}
	}


	return nbyte;
}

int printf_usb(const char *str, ...) //Variadic Function
{
	static int usb_newline = 1;


	va_list valist;
	va_start(valist, str);
	int ret = printf_callback(str, valist, &usb_newline, transmitBufUsb);
	va_end(valist);
	return ret;
}
#endif


/* ------------------------ SERIAL ------------------------ */

#ifdef USE_HAL_DRIVER

	static size_t transmitBufUart(const char* buf, size_t nbyte)
	{
		int ret = HAL_UART_Transmit(&huart1, (uint8_t*)buf, nbyte, 1000);

		if ( ret != HAL_OK ) // this is only to be able to put a breakpoint in case the first HAL_UART_Transmit return !HAL_OK
		{
			while ( ret != HAL_OK )
			{
				ssize_t nbByteNotTransmitted = huart1.TxXferCount + (ret == HAL_TIMEOUT ? 1 : 0); // size to transmit.
				if ( nbByteNotTransmitted <= 0 ) {
					#ifdef DEBUG
						__asm volatile ("bkpt 0");
					#endif
				}
				buf += nbyte - nbByteNotTransmitted;
				nbyte = nbByteNotTransmitted;
				int ret = HAL_UART_Transmit(&huart1, (uint8_t*)buf, nbyte, 1000);
			}
		}
		return nbyte;
	}

#endif

#ifdef ARDUINO

	static size_t transmitBufUart(const char* buf, size_t nbyte)
	{
		Serial.write(buf, nbyte);     // opens serial port, sets data rate to 9600 bps
		return nbyte;
	}

#endif

static int uart_newline = 1; // newline is static avoid being reinitialized at each call

int printf_uart(const char *str, ...) //Variadic Function
{
	va_list valist;
	va_start(valist, str);
	int ret = printf_callback(str, valist, &uart_newline, transmitBufUart);
	va_end(valist);
	return ret;
}

#ifdef ARDUINO

int printf_uart(const __FlashStringHelper *str, ...)
{

	va_list valist;
	va_start(valist, str);

	PrintfParams printfParams;

	printfParams.newline = &uart_newline;
	printfParams.bufIdx = 0;
	printfParams.inDirective = 0;
	printfParams.l_modifier = 0;
	printfParams.width_specifier = 0;
	printfParams.pad_char = ' ';
	printfParams.transmitBufCallBack = transmitBufUart;


	PGM_P p = reinterpret_cast<PGM_P>(str);
	size_t n = 0;
	while (1)
	{
		unsigned char c = pgm_read_byte(p++);
		if (c == 0)	break;
		n += printf_handle_format_char(c, &valist, &printfParams);
	}
	if ( printfParams.bufIdx > 0 ) printfParams.transmitBufCallBack(printfParams.buf, printfParams.bufIdx);

	va_end(valist);
	return n;
}

#endif

/* ------------------------ TRACE ------------------------ */

#ifdef TRACE

int printf_semih(const char* format, ...)
{
	static int newline = 1;

	int ret;
	va_list ap;

	va_start(ap, format);

	ret = printf_callback(format, ap, &newline, trace_write);

//	// TODO: rewrite it to no longer use newlib, it is way too heavy
//	// Print to the local buffer
//	ret = vsnprintf(buf, sizeof(buf), format, ap);
//	if (ret > 0) {
//		// Transfer the buffer to the device
//		ret = trace_write(buf, (size_t) ret);
//	}
//
	va_end(ap);
	return ret;
}

#endif

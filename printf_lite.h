#include <stdarg.h>
#include <stddef.h>
#ifdef ARDUINO
	#include <WString.h>
#endif

typedef size_t (*transmitBufCallBackType)(const char* buf, size_t nbyte);

#if defined(__cplusplus)
extern "C"
{
#endif

//int printf_callback(const char* str, va_list valist, int* newline, char* buf, int* bufIdx, transmitBufCallBackType transmitBufCallBack);
int printf_callback(const char* str, va_list valist, int* newline, transmitBufCallBackType transmitBufCallBack);

size_t transmitBufUsb(const char* buf, size_t nbyte);

int printf_uart(const char *str, ...) __attribute__((format(printf, 1, 2)));
int printf_usb(const char *str, ...) __attribute__((format(printf, 1, 2)));

#ifdef TRACE
	int printf_semih(const char* format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#endif


#if defined(__cplusplus)
}
#endif


#if defined(__cplusplus)

#ifdef ARDUINO
	// C code can't access this because __FlashStringHelper is a class
	int printf_uart(const __FlashStringHelper *str, ...); // Unfortunately, we lose the format checking. GCC complains that format isn't a string.
#endif

#endif

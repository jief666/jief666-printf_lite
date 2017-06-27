# printf_lite
Small printf implementation. Good for embedded (stm32, arduino, nrf51822)

- Currently, it can print on :
  - Arduino serial
  - Stm32 UART, via HAL
  - Stm32 USB, via HAL
  - Semi-hosting, by calling trace_write.


* This printf recognize :  
  - char (%c)
  - int and long int (%d and %ld)
  - int and long int printed in hexdecimal (%x and %lx)
  - float and double (%f and %lf) (can be disable with a macro to save a bit of space)
  - string (%s)
  - string from flash memory for Arduino (I created %F ex:printf_uart("Hello, World 3 %F\n", F("Hey")); // format in RAM, string arg in flash
  - width specifier for int, hexadecimal int, float and double
  - pad char for int, hexadecimal int, float and double

For Arduino, format string can be in flash memory.  
Ex : printf_uart(F("Hellow world")).  
NOTE : gcc or g++ won't check printf format/args consistency if format string is in flash.

----------------------

This implementation use local variable (on the stack) as a buffer to reduce RAM footprint. It's possible to set the buffer length to 1. It'll just makes more call to transmit function. Except for semi hosting, it doesn't make a big difference. Can be nice for a tiny tiny heap space. 

File extension is cpp. Function are defined extern "C" so thay can be called from C. "printf_lite.cpp" can be renamed .c, it'll compile the same.

If you add some transmit function (ex:other UART for stm32, or MCU, handling DMA UART transfer, etc.), please open a pull request (or contact me through an issue) so everyone can use it.

----------------------

Don't hesitate to open an issue just to say thanks : it'd be nice to know that it was useful and will encourage me to share plenty or other stuff I've accumulated over years. 


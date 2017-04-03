/*  Main file for the medmanager project.
 *  This file will utilize the built in ADC
 *  of the mbed to read in values from the load
 *  cell and communicate that with the server.
 */

#include <Hx711.h>
 //#include "Hx711.h"   // Include for the ADC library
 #include "mbed.h"     // Include for the serial communication

 Serial pc(USBTX, USBRX); // Used for USART TX, RX

 DigitalOut CLK(D13);
 DigitalIn DATA(D12);

 int main(void)
 {
    // initialize ADC with Hx711 object
    Hx711 load_cell = Hx711(D13, D12, 128);
 
    while(1)
    {
    	// read raw data
    	uint32_t data = load_cell.readRaw();
    	wait(2);
    	pc.printf("%i", data);
    }
    
    return 0;
 }

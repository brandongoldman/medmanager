/*
 *  Main file for the medmanager project.
 *  This file will utilize the built in ADC
 *  of the mbed to read in values from the load
 *  cell and communicate that with the server.
 */

#include <Hx711.h>
 #include "mbed.h"     // Include for the serial communication

 Serial pc(USBTX, USBRX); // Used for USART TX, RX

 DigitalOut CLK(D13); // Clock signal
 DigitalIn DATA(D12); // Input signal

 int main(void)
 {
    // initialize ADC with Hx711 object
    Hx711 load_cell = Hx711(D13, D12, 128);

    while(1)
    {
    	// read raw data
    	uint32_t data = load_cell.readRaw();
    	wait(2); // Delay for 2 seconds
    	pc.printf("%i", data); // Print the data to the screen for debugging
    }

    return 0;
 }

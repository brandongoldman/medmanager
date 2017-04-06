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
 
 // Prototyping Functions
 uint32_t convertToMass(uint32_t data);
 
 // Global Variables
 int offset = 0;
 int scale = 0;

 int main(void)
 {
    // initialize ADC with Hx711 object
    Hx711 load_cell = Hx711(D13, D12, offset, scale, 128);
    //Hx711 load_cell = Hx711(D13, D12, 128);

    while(1)
    {
        // read raw data
        int data = load_cell.readRaw();
        float mass = (float) ((-1) * (data));
        mass = (data / 1000.00);
        //uint32_t mass = convertToMass(data);
        
        
        //float data = load_cell.readRaw();
        //data = ((-1) * (data));
        //float mass = (data / 1000);
        
        // print statements for Tera Term
        wait(2); // Delay for 2 seconds
        printf("%f", mass); // Print the data to the screen for debugging
        printf("\r\n");
    }

    return 0;
 }
 
uint32_t convertToMass(uint32_t data)
{
    int mass = data / 1000;
    /*if(data <= -550000)
    {
         mass = 0;  
    }  
    else if(data > -550000 && data <= */
    return mass;
}

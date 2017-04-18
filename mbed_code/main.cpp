#include "string"
#include "mbed.h"

#include <Hx711.h>
#include "EthernetInterface.h"
#include "frdm_client.hpp"

//#include "metronome.hpp"
#include "utils.hpp"

#define IOT_ENABLED

//! The activation level of a circuit describes what voltage level is needed to
//! make (in this case) an LED turn ON, or light up. Since the FRDM board's
//! LEDs are active LOW, they must be pulled to GND (or 0, or false) to turn on.
namespace active_low
{
	const bool on = false;
	const bool off = true;
}

DigitalOut g_led_red(LED1);
DigitalOut g_led_green(LED2);
DigitalOut g_led_blue(LED3);

DigitalOut CLK(D13); // Clock signal
DigitalIn DATA(D12); // Input signal

// Global Variables
int offset = 0;
int scale = 0;

//InterruptIn g_button_mode(SW3);
//InterruptIn g_button_tap(SW2);

//metronome g_metronome;
//! Since the green LED will blink asynchronously from user input, we will use
//! a Ticker to set up a timer callback to call the pulse() function.
Ticker g_ticker;

// Declarations for Mass
size_t current_mass = 0;
volatile bool mass_changed = false;

size_t current_bpm = 0;
size_t minimum_bpm = 0;
size_t maximum_bpm = 0;

//! Network requests cannot be created (getting/setting the resource objects) in
//! secondary threads / interrupt contexts. Instead, we must mark that a value
//! changed in these cases and have the main loop complete the operation.
volatile bool bpm_changed = false;
//volatile bool bpm_updated = false;

//! A utility function for formatting values to their string equivalent.
void format_resource_value(size_t value, M2MResource* resource)
{
	//! Use sprintf to convert the integral value to a string.
	char result_string[30];
	size_t size = sprintf(result_string, "%u", value);

	const uint8_t* buffer = reinterpret_cast<const uint8_t*>(result_string);
	resource->set_value(buffer, size);
}

int main()
{
	// Seed the RNG for networking purposes
    unsigned seed = utils::entropy_seed();
    srand(seed);

    g_led_red = active_low::off;
    g_led_green = active_low::off;
    g_led_blue = active_low::off;

#ifdef IOT_ENABLED
	// Turn on the blue LED until connected to the network
    g_led_blue = active_low::on;

	// Need to be connected with Ethernet cable for success
    EthernetInterface ethernet;
    if (ethernet.connect() != 0)
        return 1;

	// Pair with the device connector
    frdm_client client("coap://api.connector.mbed.com:5684", &ethernet);
    if (client.get_state() == frdm_client::state::error)
        return 1;

	// The REST endpoints for this device
	// Add your own M2MObjects to this list with push_back before client.connect()
    M2MObjectList objects;

    M2MDevice* device = frdm_client::make_device();
    objects.push_back(device);

    //! ***********************
    //! Begin Endpoint Creation
    //! ***********************

    M2MObject* mass = M2MInterfaceFactory::create_object("3318");
    M2MObjectInstance* mass_counter = mass->create_object_instance();

    //! Set Point allows the user to read/write the mass value itself through
    //! GET and PUT.
    M2MResource* set_point = mass_counter->create_dynamic_resource("5700", "integer", M2MResourceInstance::INTEGER, true);
    set_point->set_operation(M2MBase::GET_PUT_ALLOWED);

    //! Units is a simple unchanging resource that specifies what kind of
    //! measurement is being taken. Since the units (like inches, meters, etc.)
    //! for BPM is "BPM", this resource will always have a value of "BPM".
    M2MResource* units = mass_counter->create_dynamic_resource("5701", "string", M2MResourceInstance::STRING, true);
    units->set_operation(M2MBase::GET_ALLOWED);

    units->set_value(reinterpret_cast<const uint8_t*>("g"), 1);

    //! Once we create our needed endpoints, we have to push the OBJECT.
    objects.push_back(mass);

    //! *********************
    //! End Endpoint Creation
    //! *********************

	// Publish the RESTful endpoints
    client.connect(objects);

	// Connect complete; turn off blue LED forever
    g_led_blue = active_low::off;
#endif

	// initialize ADC with Hx711 object
    Hx711 load_cell = Hx711(D13, D12, offset, scale, 128);
    //Hx711 load_cell = Hx711(D13, D12, 128);

    while (true)
    {
#ifdef IOT_ENABLED
        if (client.get_state() == frdm_client::state::error)
            break;
#endif

		/*-------LOGIC OF SCALE-------*/
		
		// read raw data
        int data = load_cell.readRaw();
        float mass = -1.0*data;
                
        //mass = (((((mass / 1000) - 551.8 ) / 25) * 2) + 0.6); <-- OLD SCALE
        mass /= 1000.00;
        mass += 51.50;
        mass /= 9.00;
        mass += 0.04;
        
        if(mass <= 0.05)
        {
            mass = 0;   
        }    
        
        mass_changed = true; // IoT boolean

        // print statements for Tera Term
        wait(2); // Delay for 2 seconds
        printf("%f", mass); // Print the data to the screen for debugging
        printf("\r\n");

		if (mass_changed)
        {
        	format_resource_value(mass, set_point);
        	mass_changed = false;
        }
    }

#ifdef IOT_ENABLED
    client.disconnect();
#endif

    return 1;
}

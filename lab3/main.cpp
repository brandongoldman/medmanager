#include "string"
#include "mbed.h"

#include "EthernetInterface.h"
#include "frdm_client.hpp"

#include "metronome.hpp"
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

InterruptIn g_button_mode(SW3);
InterruptIn g_button_tap(SW2);

metronome g_metronome;
//! Since the green LED will blink asynchronously from user input, we will use
//! a Ticker to set up a timer callback to call the pulse() function.
Ticker g_ticker;

size_t current_bpm = 0;
//! The minimum and maximum invalid/unset states are when the value is 0 here.
size_t minimum_bpm = 0;
size_t maximum_bpm = 0;
//! Network requests cannot be created (getting/setting the resource objects) in
//! secondary threads / interrupt contexts. Instead, we must mark that a value
//! changed in these cases and have the main loop complete the operation.
volatile bool bpm_changed = false;
volatile bool bpm_updated = false;

//! The function that is attached to a Ticker must have a signature of void(),
//! so wrap the pulse utility with our desired arguments.
void pulse_led_green() { utils::pulse(g_led_green); }

//! A helper function to unify the logic between the user manually setting the
//! BPM, and the BPM being set through the resource endpoint.
void update_bpm(size_t bpm)
{
	current_bpm = bpm;
	bpm_changed = true;

    //! Each time the BPM changes, check if the max/min need upadting.
    if (!minimum_bpm || current_bpm < minimum_bpm)
        minimum_bpm = current_bpm;
    if (!maximum_bpm || current_bpm > maximum_bpm)
        maximum_bpm = current_bpm;

    //! Convert BPM (frequency) into period for the ticker.
    g_ticker.attach(pulse_led_green, 60.0f / current_bpm);
}

void on_mode()
{
    //! If the metronome was timing, this tap means we should stop
    if (g_metronome.is_timing())
    {
        g_metronome.stop_timing();

        size_t bpm = g_metronome.get_bpm();
        //! Ensure the user pressed the button enough times to actually
        //! calculate a new BPM before updating everything.
        if (bpm)
        	update_bpm(bpm);
    }
    //! The metronome was not timing, so we should start
    else
    {
        g_ticker.detach();
        g_metronome.start_timing();
    }
}

void on_tap()
{
    //! A tap is only valid in the timing mode. The metronome class already
    //! checks for this, but it is good practice to check here too.
    if (!g_metronome.is_timing())
        return;

    g_metronome.tap();
    utils::pulse(g_led_red);
}

void set_point_PUT(const char*)
{
	bpm_updated = true;
}

void reset_values_POST(void*)
{
	minimum_bpm = 0;
	maximum_bpm = 0;

	//! Make sure these updates are reflected in the resource.
	bpm_changed = true;
}

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

	//! There was a mistake in the starting code that turned all LEDS on instead
	//! of turning them off (active_low off/on constants were used in the
	//! wrong place).
    g_led_red = active_low::off;
    g_led_green = active_low::off;
    g_led_blue = active_low::off;

	// Button falling edge is on push (rising is on release)
    g_button_mode.fall(&on_mode);
    g_button_tap.fall(&on_tap);

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

    //! We first need to create the object and instance. Since we only have a
    //! frequency counter (and one of them at that), we only need one object,
    //! and one instance. The object describes the BPM counter interface in
    //! general, while the instance represents a physical counter on the board.
    M2MObject* frequency = M2MInterfaceFactory::create_object("3318");
    M2MObjectInstance* bpm_counter = frequency->create_object_instance();

    //! Set Point allows the user to read/write the BPM value itself through
    //! GET and PUT.
    M2MResource* set_point = bpm_counter->create_dynamic_resource("5700", "integer", M2MResourceInstance::INTEGER, true);
    set_point->set_operation(M2MBase::GET_PUT_ALLOWED);

    //! Give the resource a default value, and set an action for when PUT occurs.
    format_resource_value(0, set_point);
    set_point->set_value_updated_function(set_point_PUT);

    //! Min value records the smallest BPM ever set, or 0 if nothing.
    M2MResource* min_value = bpm_counter->create_dynamic_resource("5601", "integer", M2MResourceInstance::INTEGER, true);
    min_value->set_operation(M2MBase::GET_ALLOWED);

    //! Every resource that can respond to GET needs a default value.
    format_resource_value(0, min_value);

    //! Max value records the smallest BPM ever set, or 0 if nothing.
    M2MResource* max_value = bpm_counter->create_dynamic_resource("5602", "integer", M2MResourceInstance::INTEGER, true);
    max_value->set_operation(M2MBase::GET_ALLOWED);

    format_resource_value(0, max_value);

    //! Reset Min/Max returns min & max to their invalid state; a value of 0.
    //! This resource can be POST-ed to execute its functionality.
    M2MResource* reset_values = bpm_counter->create_dynamic_resource("5605", "opaque", M2MResourceInstance::OPAQUE, true);
    reset_values->set_operation(M2MBase::POST_ALLOWED);

    //! Set up the action for when a POST occurs. Notice that this function is
    //! different than the one used for PUT; each REST action has its own
    //! callback function and corresponding setter.
    reset_values->set_execute_function(reset_values_POST);

    //! Units is a simple unchanging resource that specifies what kind of
    //! measurement is being taken. Since the units (like inches, meters, etc.)
    //! for BPM is "BPM", this resource will always have a value of "BPM".
    M2MResource* units = bpm_counter->create_dynamic_resource("5701", "string", M2MResourceInstance::STRING, true);
    units->set_operation(M2MBase::GET_ALLOWED);

    units->set_value(reinterpret_cast<const uint8_t*>("BPM"), 3);

    //! Once we create our needed endpoints, we have to push the OBJECT.
    objects.push_back(frequency);

    //! *********************
    //! End Endpoint Creation
    //! *********************

	// Publish the RESTful endpoints
    client.connect(objects);

	// Connect complete; turn off blue LED forever
    g_led_blue = active_low::off;
#endif

    while (true)
    {
#ifdef IOT_ENABLED
        if (client.get_state() == frdm_client::state::error)
            break;
#endif

        //! Here we must check for when our BPM/state is updated asynchronously,
        //! and update the resource values as necessary.
        if (bpm_changed)
        {
        	//! Our three values that can change may all be updated when the BPM
        	//! changes, so just update them every time to be safe.
        	format_resource_value(current_bpm, set_point);
        	format_resource_value(minimum_bpm, min_value);
        	format_resource_value(maximum_bpm, max_value);

        	bpm_changed = false;
        }
        if (bpm_updated)
        {
        	uint8_t* buffer;
					size_t buffer_length;

			//! Retrieve the value the endpoint has been set to.
			set_point->get_value(buffer, buffer_length);
			String bpm_string(reinterpret_cast<const char*>(buffer), buffer_length);

			//! Extract the new BPM as an actual integer with sscanf().
			size_t bpm;
			sscanf(bpm_string.c_str(), "%u", &bpm);

			//! The user cannot set the BPM to zero; just ignore if that is the case.
			if (bpm)
				update_bpm(bpm);

			bpm_updated = false;
		}
    }

#ifdef IOT_ENABLED
    client.disconnect();
#endif

    return 1;
}

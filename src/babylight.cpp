#include "babylight.h"


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const uint8_t	TIMER_DIV			= 8;
const uint8_t	SAMPLE_RATE_DIV		= 200;
const uint16_t	SAMPLE_RATE			= F_CPU / TIMER_DIV / SAMPLE_RATE_DIV * 2;
const uint16_t	DIMMER_TIMEOUT		= SAMPLE_RATE * 0.25;
const uint16_t	WHEEL_TIMEOUT		= SAMPLE_RATE * 0.3;
const uint16_t	SLEEPYEYE_TIMEOUT	= SAMPLE_RATE * 0.4;


volatile		uint16_t			senseSample		= 0;
volatile		uint16_t			timerTicks		= 0;
volatile		uint16_t			secondsAlive	= 0;

static			Events				events(MAX_EVENT_RECORDS);
static			RGB::pixel_t		io_pixel;
static			RGB					rgb(&events, &io_pixel);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Prepare the power switch circuit
static void initMotor(void)
{
	// turn on the power switch
	PWR_PRT		&= ~(1<<PWR_PIN);
	PWR_DDR		|= (1<<PWR_PIN);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Prepare the event system
static void initEvents(void)
{
	// setup eventing system
	events.eventsUnregisterAll();

	events.setTimeBase(SAMPLE_RATE);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Prepare the ADC
static void initAdc(void)
{
	//
	// Prepare ADC
	// pg 138
	power_adc_enable();
	ADMUX	=	(1<<REFS1)	|	// REFS2:0 = 010 for 1.1v reference voltage
				(0<<REFS0)	|
				(0<<ADLAR)	|	// No left adjust
				(0<<REFS2)	|
				(0<<MUX3)	|	// MUX3:0 = 0011 for ADC3
				(0<<MUX2)	|
				(1<<MUX1)	|
				(1<<MUX0);

	ADCSRA	=	(1<<ADEN)	|	// enable adc
				(1<<ADSC)	|	// start a conversion
				(0<<ADATE)	|	// no auto-trigger
				(1<<ADIF)	|	// clear interrupt
				(1<<ADIE)	|	// enable interrupts
				(1<<ADPS2)	|	// set ADC prescaler to a factor 128
				(1<<ADPS1)	|
				(1<<ADPS0);

	ADCSRB	=	(0<<BIN)	|	
				(0<<ACME)	|
				(0<<IPR)	|
				(0<<0)		|
				(0<<0)		|
				(0<<ADTS2)	|
				(0<<ADTS1)	|
				(0<<ADTS0);

	// setup sense input pin
	DIDR0	=	(1<<ADC3D);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Prepare timer0 
static void initPwm(void)
{
	// Setup timer0 for Normal mode with interrupts on overflow
	// enable timer1
	power_timer0_enable();

	GTCCR	=	(0<<TSM)	|
				(0<<PWM1B)	|
				(0<<COM1B1)	|
				(0<<COM1B0)	|
				(0<<FOC1B)	|
				(0<<FOC1A)	|
				(0<<PSR1)	|
				(0<<PSR0);

	TCCR0A	=	(0<<COM0A1)	|
				(0<<COM0A0)	|
				(0<<COM0B1)	|
				(0<<COM0B0)	|
				(0<<0)		|
				(0<<0)		|
				(1<<WGM01)	|	// CTC
				(0<<WGM00);

	TCCR0B	=	(0<<FOC0A)	|
				(0<<FOC0B)	|
				(0<<0)		|
				(0<<0)		|
				(0<<WGM02)	|
				(0<<CS02)	| // PCK/64 = 250kHz / 250 ~= 1KHz
				(1<<CS01)	|
				(0<<CS00);

	OCR0A	=	SAMPLE_RATE_DIV-1;
	TIMSK	=	(1<<OCIE0A);

	// setup output pins
	RGB_DDR		|=	(1<<RGB_RED) | (1<<RGB_GRN) | (1<<RGB_BLU);
	RGB_PRT		|=	(1<<RGB_RED) | (1<<RGB_GRN) | (1<<RGB_BLU);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Registers the display RGB event
void startRGB(void)
{
	events.registerHighPriorityEvent(eDisplayRGB, 0, EVENT_STATE_NONE);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// delays execution for the specified # of seconds while allowing for the eventing
// system to function and the watchdog to be reset.
static void pause(uint16_t seconds)
{
	uint16_t delay = secondsAlive;
	while (secondsAlive - delay < seconds)
	{
		wdt_reset();
		events.doEvents();
	}
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Immitates a PWM switching system to show color patterns
void eDisplayRGB(eventState_t state)
{
	static uint8_t idx = 0;
	if (0 == idx)
	{
		// turn off all colors
		RGB_PRT |=	(1<<RGB_RED) | (1<<RGB_GRN) | (1<<RGB_BLU);
	}
	else
	{
		// turn on specific colors
		if (io_pixel.red >= idx)
			RGB_PRT &= ~(1<<RGB_RED);
		if (io_pixel.grn >= idx)
			RGB_PRT &= ~(1<<RGB_GRN);
		if (io_pixel.blu >= idx)
			RGB_PRT &= ~(1<<RGB_BLU);
	}

	idx -= 2;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Turns on the motor and lets the system settle for a few seconds
static void warmUpBattery(void)
{
	motorOn();

    pause(1);

	// show each color
	rgb.fadeIn(40, 0, 0, DIMMER_TIMEOUT);
	rgb.fadeTo(0, 40, 0, DIMMER_TIMEOUT);
	rgb.fadeTo(0, 0, 40, DIMMER_TIMEOUT);
	rgb.fadeOut(DIMMER_TIMEOUT);

	// show bright white
	rgb.fadeIn(60, 60, 60, DIMMER_TIMEOUT);
	pause(1);

    rgb.fadeOut(DIMMER_TIMEOUT);
    pause(1);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Convert ADC readings to a color
static void eMapVoltageToColor(eventState_t state)
{
	ADCSRA |= (1<<ADSC);

	// put CPU in ADC sleep mode so we can get a clean conversion
	sleep_enable();
	set_sleep_mode(SLEEP_MODE_ADC);
	sleep_cpu();

	// ADC = (Vin * 1024) / Vref
	// BatToAdc = (VBatt/BAT_MAX * ADC_REF) => Vin

	// map the voltage that was sampled to a color pattern
	if (senseSample >= ADC_BAT_NEW)				// blue
	{
		rgb.fadeTo(0, 0, 60, DIMMER_TIMEOUT);
	}
	else if (senseSample >= ADC_BAT_GOOD)		// blue-green
	{
		rgb.fadeTo(0, 30, 60, DIMMER_TIMEOUT);
	}
	else if (senseSample >= ADC_BAT_OK)			// green
	{
		rgb.fadeTo(0, 60, 0, DIMMER_TIMEOUT);
	}
	else if (senseSample >= ADC_BAT_LOW)		// yellow
	{
		rgb.fadeTo(60, 60, 0, DIMMER_TIMEOUT);
	}
	else if (senseSample >= ADC_BAT_CRIT)		// orange
	{
		rgb.fadeTo(60, 28, 0, DIMMER_TIMEOUT);
	}
	else										// red
	{
		rgb.fadeTo(60, 0, 0, DIMMER_TIMEOUT);
	}
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Shows the state of the battery for a few seconds
void showBatteryLevel(void)
{
	// setup eventing system to display the battery voltage color chart
	events.registerEvent(eMapVoltageToColor, SAMPLE_RATE / 2, EVENT_STATE_NONE);

	// show battery level for a few seconds
	pause(3);

	// wait for battery level to turn off
	events.eventsUnregisterEvent(eMapVoltageToColor);
	rgb.fadeOut(DIMMER_TIMEOUT);

	power_adc_disable();

	// wait a couple of seconds with the lights off
	pause(2);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Dances the RGB LED through various colors
static void showColorWheel(void)
{
	secondsAlive = 0;

	// show dancing color while in normal mode
	while(secondsAlive < ALIVE_TIMEOUT)
	{
		switch (TCNT0 & 0x07)
		{
			case 0:
				rgb.fadeTo(MAX, MAX/4, MAX/2, WHEEL_TIMEOUT);
				break;
			case 1:
				rgb.fadeTo(MAX/4, MAX/3, MAX/3, WHEEL_TIMEOUT);
				break;
			case 2:
				rgb.fadeTo(MAX/2, MAX/4, 0, WHEEL_TIMEOUT);
				break;
			case 3:
				rgb.fadeTo(MAX/3, 0, MAX/2, WHEEL_TIMEOUT);
				break;
			case 4:
				rgb.fadeTo(MAX, MAX/4, 0, WHEEL_TIMEOUT);
				break;
			case 5:
				rgb.fadeTo(MAX/2, MAX/2, 0, WHEEL_TIMEOUT);
				break;
			case 6:
				rgb.fadeTo(MAX/3, MAX/2, MAX/4, WHEEL_TIMEOUT);
				break;
			case 7:
				rgb.fadeTo(MAX/4, MAX, 0, WHEEL_TIMEOUT);
				break;
		}
		//pause(5);
	}

    // dim to off
	rgb.fadeOut(WHEEL_TIMEOUT);
	pause(2);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Puts the system in low-power mode (shuts off the motor) and does a fade on the
// red LED
void showSleepState(void)
{
	// disable digital input disable
	DIDR0 = 0;

	// shut down some stuff
	motorOff();
	power_adc_disable();
	wdt_disable();


	// sleep for the specified timeout
	secondsAlive = 0;
	while(secondsAlive < SLEEP_TIMEOUT)
	{
		rgb.fadeIn(14, 0, 0, SLEEPYEYE_TIMEOUT);
		pause(1);
		rgb.fadeOut(SLEEPYEYE_TIMEOUT * 1.4);
		pause(10);
	}
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Puts the CPU into a deep power down mode where everything is halted.
void powerSave(void)
{
	// set IO ports as input ports for reduced power consumption
	RGB_DDR &= ~((1<<RGB_RED) | (1<<RGB_GRN) | (1<<RGB_BLU));
	PWR_DDR &= ~(1<<PWR_PIN);
	SNS_DDR &= ~(1<<SNS_PIN);

	cli();

	while(1)
	{
		// setup for deep sleep
		sleep_enable();
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		sleep_cpu();
	}
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// MAIN
int main(void)
{
	power_timer1_disable();
	power_usi_disable();

	initMotor();
	initEvents();
	initAdc();
	initPwm();

	// initialize the RGB effects
	startRGB();

	sei();

    // enable 1s watchdog timer
    wdt_enable(WDTO_2S);

	// warm-up the system
	warmUpBattery();

    // shows the battery level for a period of time
    showBatteryLevel();

    // shows color wheel for a period of time
    showColorWheel();

	// set sleep mode & warn user
	showSleepState();

	// put cpu into low power mode forever
	powerSave();

	while(1)
	{
		wdt_reset();
		sleep_cpu();
	}

	return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Just let this wakeup the CPU
ISR(WDT_vect)
{
	//NO-OP
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ADC conversion ready interrupt handler
ISR(ADC_vect)
{
	// save voltage sample value
	senseSample = (ADCH << 8) | (ADCL);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Timer0 compare A interrupt handler (10KHz)
ISR(TIM0_COMPA_vect)
{
	// call the event sync root
	events.sync();

	// increment the timerticks
	if (timerTicks++ > SAMPLE_RATE - 1)
	{
		timerTicks = 0;
		secondsAlive++;
	}
}

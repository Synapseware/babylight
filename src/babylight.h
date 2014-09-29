#ifndef __BABYLIGHT_H__
#define __BABYLIGHT_H__


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>


#include <events/events.h>
#include <effects/rgb.h>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#define RGB_PRT		PORTB
#define RGB_DDR		DDRB
#define RGB_RED		PB2
#define RGB_GRN		PB1
#define RGB_BLU		PB0

#define SNS_PRT		PORTB
#define SNS_DDR		DDRB
#define SNS_PIN		PB3

#define PWR_PRT		PORTB
#define PWR_DDR		DDRB
#define PWR_PIN		PB4
#define motorOn()	(PWR_PRT |= (1<<PWR_PIN))
#define motorOff()	(PWR_PRT &= ~(1<<PWR_PIN))

#define ALIVE_TIMEOUT		(60 * 60 * 1.5)
#define SLEEP_TIMEOUT		(60 * 60 * 0.5)


#define MAX 12
#define MIN 1



// ADC = (Vin * 1024) / Vref
// BatToAdc = (VBatt/BAT_MAX * ADC_REF) => Vin
// http://madscientisthut.com/wordpress/wp-content/uploads/2011/01/AA-100mA.png
// Voltage through divider:
//	Vout = (Vin * R1)/(R1 + R2)
#define R1				68000
#define R2				150000

#define ADC_REF			1.10

// Vin		VDiv (voltage through divider)
// 1.60 => 1.10
#define BAT_MAX			1.60
// 1.40 => 0.96
#define BAT_NEW			1.40
// 1.30 => 0.89
#define BAT_GOOD		1.30
// 1.20 => 0.83
#define BAT_OK			1.20
// 1.00 => 0.69
#define BAT_LOW			1.00
// 0.80 => 0.55
#define BAT_CRIT		0.8

//Vo= Vin*R2/(R1+R2)
// 0.96 => 894
#define ADC_BAT_NEW		(((BAT_NEW * R2)/(R1+R2)) * 1024) / ADC_REF
// 0.89 => 829
#define ADC_BAT_GOOD	(((BAT_GOOD * R2)/(R1+R2)) * 1024) / ADC_REF
// 0.83 => 773
#define ADC_BAT_OK		(((BAT_OK * R2)/(R1+R2)) * 1024) / ADC_REF
// 0.69 => 642
#define ADC_BAT_LOW		(((BAT_LOW * R2)/(R1+R2)) * 1024) / ADC_REF
// 0.55 => 512
#define ADC_BAT_CRIT	(((BAT_CRIT * R2)/(R1+R2)) * 1024) / ADC_REF


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Programming header:

	Blue	SCK
	Green	MISO
	Yellow	MOSI
	Orange	Reset
	Red		+5V
	Brown	GND

 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


void eDisplayRGB(eventState_t state);

#endif

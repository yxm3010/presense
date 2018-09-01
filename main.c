
#include <avr/io.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/sleep.h>
uint16_t ADC_Result;
uint8_t ws_threshold;
uint8_t ws1_upcount;
uint8_t ws1_dncount;
uint8_t ws2_upcount;
uint8_t ws2_dncount;
uint8_t ei0_loop;
int pd_counter;
volatile bool ws1;
volatile bool ws2;
volatile bool ei0;
bool engine_on;

int main(void){

	//Initialize LED function
		ADC_Result = 0;
		ws_threshold = 194;
		ws1_upcount = 0;
		ws1_dncount = 0;
		ws2_upcount = 0;
		ws2_dncount = 0;
		ei0_loop = 0;
		pd_counter = 0;
		ws1 = false;
		ws2 = false;
		ei0 = false;
		engine_on = false;
	
	//Set Port B pin 0, pin 1, and pin 6 to output
		DDRB |= 1<<PORTB0 | 1<<PORTB1 | 1<<PORTB6;
	
	//Set Port D pin 2 (INT0 pin) to input
		DDRD &= ~(1<<PIND2);
	
	//Set Port D pin 7 to input
		DDRD &= ~(1<<PIND7);
	
	//Initialize the ADC
	//Enable interruputs function in ADC
		ADCSRA |= 1<<ADIE;
	
	//Enable the ADC feature (p.319 Datasheet)
		ADCSRA |= 1<<ADEN;
		
	//Set ADC prescaler to 16 (i.e. sample at 62.5kHz) (p.319 Datasheet)
	//By default System Clock rate @1Mhz. ADC needs to run between 50kHz - 100kHz.
		ADCSRA |= 1<<ADPS2;
		
	//Right adjust the 8-bit ADC conversion results.
		ADMUX |= 1<<ADLAR;
	
	//Specify that we are connecting Vcc as AREF pin
		ADMUX |= 1<<REFS0;
	
	//Enable interrupts function in INT0
		EIMSK |= 1<<INT0;
	
	//Set INT0 pin to Logical Change Mode (p. 89 datasheet)
		EICRA |= 1<<ISC01;
	
	//Enable the global interrupts
		sei(); 
	
	//Start the first conversion
		ADCSRA |= 1<<ADSC;
	
	//High Side to pin 6 Port B	
		PORTB |= 1<<PINB6;
	
	//Select Power-down sleep mode (p.68)
		//SMCR |= 1<<SM1;
		set_sleep_mode(1<<SM1);
		
	//Set TC1 source to internal clock and prescaler to 64
		TCCR1B |= 1<<CS11 | 1<<CS10;
	

while (1)
{	
	engine_on = !bit_is_clear(PIND,2);
	
 	if (ADC_Result > ws_threshold)
	{ 
		ws1_upcount++; //for debounce
	}
	else
	{
		ws1_dncount++; //for debounce
	}
	
	if (ws1_upcount == 5)
	{
		//Weight sensor 1 ON
		ws1 = true;
		ws1_upcount = 0;
	} else if (ws1_dncount == 1)
	{
		//Weight sensor 1 OFF
		ws1 = false;
		ws1_dncount = 0;
	}
	
	if (ws1||ws2)
	{
		PORTB |= 1<<PINB0;
	}
	else
	{
		PORTB &= ~(1<<PINB0);
	}
	
	if ((ws1||ws2) & !engine_on)
	{
		PORTB ^= 1<<PINB1;
		_delay_ms(10);
	}
	else
	{
		PORTB &= ~(1<<PINB1);
	}
	

	if(!(ws1||ws2) && !engine_on){
		
		if(TCNT1 > 15625){
			PORTB |= 1<<PINB0;
			_delay_ms(5);
			PORTB &= ~(1<<PINB0);
			TCNT1 = 0;
			pd_counter ++;

		}
		if(pd_counter == 10){
			pd_counter = 0;
			sleep_enable();
			sleep_cpu();
		}

	}else
	{
		pd_counter = 0;
	}
	
	
	// Add code to prevent back to back ei0 read
	
	// Add code to calibrate the system to current sensed weight + 1kg if >1kg; 
	// Otherwise set it to default value of 1kg
	
	if(ei0)
	{
		cli();
		PORTB |= 1<<PINB1;
		_delay_ms(5);
		PORTB &= ~(1<<PINB1);
		_delay_ms(10);
		PORTB |= 1<<PINB1;
		_delay_ms(50);
		PORTB &= ~(1<<PINB1);
		_delay_ms(5);
		ei0_loop = 0;
		ei0 = false;
		sei();
	}
	//Start the next conversion
		ADCSRA |= 1<<ADSC;
}
}


//INT0 Interrupt Routine
ISR(INT0_vect)
{
	ei0 = true;
	sleep_disable();
}

//ADC Interrupt Routine
ISR(ADC_vect)
{
	ADC_Result = ADCH;
	
	switch(ADMUX)
	{
		case 0x60://0b01100000
	//Check if ws1 is greater than threshold. Apply debouncing.
			if (ADC_Result > ws_threshold)
			{ 
				ws1_upcount++; //for debounce
			}
			else
			{
				ws1_dncount++; //for debounce
			}
			
			if (ws1_upcount == 5)
			{
				//Weight sensor 1 ON
				ws1 = true;
				ws1_upcount = 0;
			} else if (ws1_dncount == 1)
			{
				//Weight sensor 1 OFF
				ws1 = false;
				ws1_dncount = 0;
			}
			//Do next ADC
			ADMUX = 0x61;
			break;	
			
		case 0x61://0b01100001
	//Check if ws2 is greater than threshold.
		 	if (ADC_Result > ws_threshold)
		 	{ 
				ws2_upcount++; //for debounce
			}
			else
			{
				ws2_dncount++; //for debounce
			}
			
			if (ws2_upcount == 5)
			{
				//Weight sensor 2 ON
				ws2 = true;
				ws2_upcount = 0;
			}else if(ws2_dncount == 1)
			{
				//Weight sensor 2 OFF
				ws2 = false;
				ws2_dncount = 0;
			}
			//Do next ADC
			ADMUX = 0x60;
		 	break;
		default:
			break;
		}
}
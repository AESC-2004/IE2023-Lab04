/*
 * Lab04.c
 *
 * Created: 29/03/2025 10:16:44 p. m.
 * Author: ang50
 */
/*********************************************************************************************************************************************/
// Encabezado (Libraries)
#define F_CPU 1000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// Variables y constantes
// TIMERS
#define T0VALUE			178
#define T1VALUE			3036
#define T3VALUE			44703
// Multiplexación
#define DISP0			5
#define DISP1			4
#define LEDS			6
uint8_t MUX_SEQUENCE	= 0b00000001;
// Botones de conteo
#define COUNTUP			1
#define COUNTDWN		2
uint8_t COUNTUP_LAST	= 0;
uint8_t COUNTDWN_LAST	= 0;
// Alarma
#define ALARM			7
uint8_t	ALARM_ACTIVE    = 0;
uint8_t ALARM_500ms		= 0;
uint8_t ALARM_166ms		= 0;
// Arreglos para lógica de Bit Swizzling
const uint8_t PORTD_PINS[]	= {PORTD0, PORTD1, PORTD2, PORTD3, PORTD4, PORTD6, PORTD7};
const uint8_t LEDS_BITS[]	= {0,1,2,3,4,5,6};
// Valores HEX para Displays
const uint8_t DISP7SEG[16] = {
	0x5F, 0x06, 0x9B, 0x8F, 0xC6, 0xCD, 0xDD, 0x07, 0xDF, 0xC7,
	0xD7, 0xDC, 0x59, 0x9E, 0xD9, 0xD1
};
// ADC
uint16_t ADC_VALUE		= 0;
// Variables de conteo
uint8_t COUNT			= 0;
uint8_t DISP0_VAL		= 0;
uint8_t DISP1_VAL		= 0;



/*********************************************************************************************************************************************/
// Function prototypes
void SETUP();
void initTMR0();
void initTMR1();
void initTMR3();
void initPCINT();
void initADC();
void bitSwizzling(uint8_t VALUE);

/*********************************************************************************************************************************************/
// Main Function
int main(void)
{
	SETUP();
	
	while (1)
	{
		// Acomodamos la lectura del ADC a HEX
		//Paso 1: Dividir Nibbles del ADCH
		uint8_t H_NIBBLE = (ADC_VALUE >> 4) & 0x0F;
		uint8_t L_NIBBLE = ADC_VALUE & 0x0F; 
		// Paso 2: Subida de cada nibble a DISP0 y DISP1
		DISP1_VAL = DISP7SEG[H_NIBBLE];
		DISP0_VAL = DISP7SEG[L_NIBBLE];
		
		//Revisamos si COUNT > ADC. Si sí, encendemos la alarma. Si no, la apagamos.
		if (COUNT > ADC_VALUE)
		{
			ALARM_ACTIVE = 1;
		} else if (COUNT <= ADC_VALUE)
		{
			ALARM_ACTIVE = 0;
		}
		
		// Para la alarma, revisamos si ALARM_ACTIVE, ALARM_500ms, y ALARM_166ms ESTÁN ENCENDIDOS, para ENCENDER o no la alarma.
		if (ALARM_ACTIVE == 1 && ALARM_500ms == 1 && ALARM_166ms == 1)
		{
			PORTC = (1 << ALARM);
		}
		else
		{
			PORTC = (0 << ALARM);
		}
	}
}

/*********************************************************************************************************************************************/
// NON-Interrupt subroutines
void SETUP()
{
	// Desactivamos interrupciones
	cli();
	// Desactivamos UART1
	UCSR1B			= 0x00;  
	// PORTD: Salida de displays y LEDs							|	PORTD: XXXX XXXX
	DDRD			= 0xFF;
	PORTD			= 0x00;
	// PORTB: Transistores para multiplexación y PINCHANGE		|	PORTB: 0XXX 0110
	DDRB	= (1 << DISP0) | (1 << DISP1) | (1 << LEDS);
	PORTB	= (1 << COUNTUP) | (1 << COUNTDWN);
	// PORTC: PC7 para alarma									|	PORTC: X000 0000
	DDRC	= (1 << ALARM);
	PORTC	= 0;
	// Ajustamos el Prescaler global para F_CPU = 1MHz
	CLKPR			= (1 << CLKPCE);
	CLKPR			= (0 << CLKPCE) | (0 << CLKPS3) | (1 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0);
	// Algunos ajustes de interrupciones
	initTMR0();
	// (Para la alarma, je)
	initTMR1();
	initTMR3();
	//
	initPCINT();
	initADC();
	// Rehabilitamos interrupciones
	sei();
}

void initTMR0()
{
	// Usaremos TIM0 en modo NORMAL con PS de 64
	TCCR0B	= (0 << CS02) | (1 << CS01) | (1 << CS00);
	TIMSK0	= (1 << TOIE0);
	TCNT0	= T0VALUE;
}

void initTMR1()
{
	// Usaremos TIM1 en modo NORMAL con PS de 8
	TCCR1B	= (0 << CS12) | (1 << CS11) | (0 << CS10);
	TIMSK1	= (1 << TOIE1);
	TCNT1	= T1VALUE;
}

void initTMR3()
{
	// Usaremos TIM3 en modo NORMAL con PS de 8
	TCCR3B	= (0 << CS32) | (1 << CS31) | (0 << CS30);
	TIMSK3	= (1 << TOIE3);
	TCNT3	= T3VALUE;
}

void initPCINT()
{
	// El ATMEGA32U4 solo cuenta con un vector de Pinchange (PB)
	PCICR	= (1 << PCIE0);
	PCMSK0	= (1 << COUNTUP) | (1 << COUNTDWN);
}

void initADC()
{
	// Usaremos el ADC a 125kHz (PS de 8) con Auto Trigger en Free Running Mode con vector de interrupción
	// Activamos solamente ADC6, REF como VCC y CON justificación izquierda.
	ADMUX	= (0 << REFS1) | (1 << REFS0) | (1 << ADLAR) | (0 << MUX4) | (0 << MUX3) | (1 << MUX2) | (1 << MUX1) | (0 << MUX0);
	// Definimos Auto Trigger Source como Free Running mode
	ADCSRB	= (0 << ADTS3) | (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0);
	// Ajustamos el ADC Interrupt Enable, el ADC Auto Trigger, y el prescaler del ADC a 8 
	ADCSRA	= (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (0 << ADIF) | (1 << ADIE) | (0 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void bitSwizzling(uint8_t VALUE)	// Definimos un parámetro local "VALUE" (Se ingresará COUNT)
{
	// Debemos distribuir COUNT así: {PD7, PD6, PD4, PD3, PD2, PD1, PD0, PE6}
	// Paso 1: Creamos máscaras
	PORTD &= ~((1 << PORTD7) | (1 << PORTD6) | (1 << PORTD4) | (1 << PORTD3) | (1 << PORTD2) | (1 << PORTD1) | (1 << PORTD0));
	PORTE &= ~(1 << PORTE6);
	
	// Paso 2: Ciclo for de arreglo para PORTD
	for (uint8_t i = 0; i < 7; i++)
	{
		if (VALUE & (1 << LEDS_BITS[i]))
		{
			PORTD	|= (1 << PORTD_PINS[i]);
		}
	}
	
	// Paso 3: Arreglo de PORTE6
	if (VALUE & (1 << 7))
	{
		PORTE		|= (1 << PORTE6);
	}
}
/*********************************************************************************************************************************************/
// Interrupt routines
ISR(TIMER0_OVF_vect)
{
	// Recargamos TCNT0
	TCNT0	= T0VALUE;
	// Verificamos qué debe sacar PORTD según la secuencia de multiplexación
	if (MUX_SEQUENCE == 0b00000001)
	{
		// Si DISP0 estaba encendido, ahora queremos mostrar DISP1
		// Apagamos DISP0
		PORTB			&= ~(1 << DISP0);
		// Cargamos DISP1_VAL en PORTD
		PORTD			= DISP1_VAL;
		// Encendemos DISP1
		PORTB			|= (1 << DISP1);
		//Y actualizamos el valor de MUX_SEQUENCE
		MUX_SEQUENCE	= 0b00000010;
	} else if (MUX_SEQUENCE == 0b00000010)
	{
		// Si DISP1 estaba encendido, ahora queremos mostrar LEDS
		// Apagamos DISP1
		PORTB			&= ~(1 << DISP1);
		// Cargamos COUNT en PORTD (Usamos la función de Bit Swizzling creada)
		bitSwizzling(COUNT);
		// Encendemos LEDS
		PORTB			|= (1 << LEDS);
		//Y actualizamos el valor de MUX_SEQUENCE
		MUX_SEQUENCE	= 0b00000100;
	} else if (MUX_SEQUENCE == 0b00000100)
	{
		// Si LEDS estaba encendido, ahora queremos mostrar DISP0
		// Apagamos LEDS
		PORTB			&= ~(1 << LEDS);
		// Cargamos DISP0_VAL en PORTD
		PORTD			= DISP0_VAL;
		// Encendemos DISP0
		PORTB			|= (1 << DISP0);
		// Y actualizamos el valor de MUX_SEQUENCE
		MUX_SEQUENCE	= 0b00000001;		
	}
}

ISR(PCINT0_vect)
{
	// Revisamos qué botón fue presionado
	// 0 LÓGICO = PRESSED
	// 1 LÓGICO = NOT PRESSED
	if ((PINB & ((1 << COUNTUP))) == (0 << COUNTUP)) 
	{
		// Si COUNTUP fue presionado...
		// Revisamos su estado anterior. Si antes estaba presionado, revisamos COUNTDWN, y...
		// si antes COUNTUP no estaba presionado, actualizamos su estado y ejecutamos COUNT++
		if  (COUNTUP_LAST == 1)
		{	
			
		} else 
		{
			COUNTUP_LAST = 1;
			COUNT++;
		}
	} else if ((PINB & ((1 << COUNTUP))) == (1 << COUNTUP))
	{
		COUNTUP_LAST = 0;
	}
	if ((PINB & ((1 << COUNTDWN))) == (0 << COUNTDWN))
	{
		// Si COUNTDWN fue presionado...
		// Revisamos su estado anterior. Si antes estaba presionado, nos salimos de la rutina, y...
		// si antes COUNTDWN no estaba presionado, actualizamos su estado y ejecutamos COUNT--
		if  (COUNTDWN_LAST == 1)
		{
			
		} else
		{
			COUNTDWN_LAST = 1;
			COUNT--;
		}
	} else if ((PINB & ((1 << COUNTDWN))) == (1 << COUNTDWN))
	{
		COUNTDWN_LAST = 0;
	}
}

ISR(ADC_vect)
{
	// Como el ADC se encuentra en Free Running Mode... solo guardamos el valor de lectura
	ADC_VALUE = ADCH;
}

// Para la alarma, je
ISR(TIMER1_OVF_vect)
{
	// Si pasaron 500ms, hacemos SWAP a ALARM_500ms
	if (ALARM_500ms == 0)
	{
		ALARM_500ms = 1;
	} else if (ALARM_500ms == 1)
	{
		ALARM_500ms = 0;
	}
}

ISR(TIMER3_OVF_vect)
{
	// Si pasaron 166ms, hacemos SWAP a ALARM_166ms
	if (ALARM_166ms == 0)
	{
		ALARM_166ms = 1;
	} else if (ALARM_166ms == 1)
	{
		ALARM_166ms = 0;
	}
}
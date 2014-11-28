/*
 *      __________  ________________  __  _______
 *     / ____/ __ \/ ____/ ____/ __ )/ / / / ___/
 *    / /_  / /_/ / __/ / __/ / __  / / / /\__ \ 
 *   / __/ / _, _/ /___/ /___/ /_/ / /_/ /___/ / 
 *  /_/   /_/ |_/_____/_____/_____/\____//____/  
 *                                      
 *  Copyright (c) 2009 Andreas Krebs <kubi@krebsworld.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 * 
 */



//#include <P89LPC922.h>
//#include "../lib_lpc922/fb_lpc922.h"
#include "fb_app_logi.h"


unsigned char gate_a, gate_b, gate_q;


void write_value_req(unsigned char objno)	// Empfangenes write_value_request Telegramm verarbeiten
{
	write_obj_value(objno,telegramm[7]&0x01);
}


// speichert den aktuellen Wert eines Objektes
// die gate ausg�nge werden nicht verarbeitet!!!
void write_obj_value(unsigned char objno,unsigned int objvalue)
{
	unsigned char gateno, rest;

	gateno=objno/3;
	rest=objno-(gateno*3);

	if(rest==0) {
		if(objvalue==0) gate_a&=0xFF-(1<<gateno);
		else gate_a|=1<<gateno;
	}
	if(rest==1) {
		if(objvalue==0) gate_b&=0xFF-(1<<gateno);
		else gate_b|=1<<gateno;
	}
}



void read_value_req(unsigned char objno)	// Empfangenes read_value_request Telegramm verarbeiten
{
	/* Hier sicherstellen, dass der Objektwert, der durch read_obj_value() gelesen wird,
	 * aktuell ist. Danach das Senden des Objektes anstossen.
	 */
	send_obj_value(objno+64);
}


unsigned long read_obj_value(unsigned char objno)
{
	unsigned char gateno, rest, value=0;

	gateno=objno/3;
	rest=objno-(gateno*3);

	if(rest==0) {
		value=(gate_a>>gateno)&0x01;
	}
	if(rest==1) {
		value=(gate_b>>gateno)&0x01;
	}
	if(rest==2) {
		value=(gate_q>>gateno)&0x01;
	}


	return(value);	// Beispiel return-Wert
}


void restart_app(void)		// Alle Applikations-Parameter zur�cksetzen
{

	P0=0;
	P0M1=0x00;		// Port 0 Modus push-pull f�r Ausgang
	P0M2=0xFF;

	gate_a=0;
	gate_b=0;
	gate_q=0;
	

	
	// Beispiel f�r die Applikations-spezifischen Eintr�ge im eeprom
	EA=0;						// Interrupts sperren, damit flashen nicht unterbrochen wird
	START_WRITECYCLE
	WRITE_BYTE(0x01,0x03,0x00)	// Herstellercode 0x004C = Bosch
	WRITE_BYTE(0x01,0x04,0x4C)
	WRITE_BYTE(0x01,0x0C,0x00)	// PORT A Direction Bit Setting
	STOP_WRITECYCLE
	EA=1;						// Interrupts freigeben
}

/*
 *    _____ ______ __   __________  __  _______ *
 *   / ___// ____// /  / ____/ __ )/ / / / ___/ *
 *   \__ \/ __/  / /  / /__ / __  / / / /\__ \  *
 *  ___/ / /__  / /__/ /__// /_/ / /_/ /___/ /  *
 * /____/_____//____/_/   /_____/\____//____/   *
 *
 *  Copyright (c) 2014, Stefan Haller
 *  Copyright (c) 2008,2009,2013 Andreas Krebs <kubi@krebsworld.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *http://de.wikibooks.org/wiki/C%2B%2B-Programmierung:_Dokumentation_mit_Doxygen
 * http://studiwiki.informatik.uni-stuttgart.de/Softwaretechnik/Studienprojekte/Beispiel_Styleguide
 *http://www.stack.nl/~dimitri/doxygen/commands.html
 */
/**
* @file   fb_app_taster.c
* @author Andreas Krebs <kubi@krebsworld.de>
* @date    2008
*
* @brief Applikation Taster, siehe fb_taster.c
*
*
*/
#include "fb_app_taster.h"

#include  "debug.h"

unsigned int timer,timerflags; /// Timer fuer Schaltverzoegerungen, wird alle 130us hochgezaehlt
__bit delay_toggle; /// um nur jedes 2. Mal die delay routine auszufuehren

unsigned char button_buffer; /// Puffer fuer Taster Werte
//unsigned char object_value[8];
unsigned char LEDSTATE,dimmcompare,dimmwert;
unsigned char timercnt[8],timerbase[8],timerstate[8];
/**
*  Ein oder mehrere Taster wurden gedrueckt oder losgelassen
*
* \param  portval <Beschreibung>
* @return void
*/
void port_changed(unsigned char portval)
{
	unsigned char n=0,oldvalue, buttonpattern;

	oldvalue=dimmcompare;
	do{
		if (oldvalue!=dimmcompare)// dimmcompare wird in der ISR T0 erhöht
		{
			n++;
			oldvalue=dimmcompare;
		}
	}
	while (n<=200);				// warten auf 200 x Timer 0 (0.05ms)= 10ms
	if ((PORT & 0x0F) == portval) {
		for (n=0; n<4; n++) {	// alle 4 Taster einzeln pruefen (koennten ja mehrere gleichzeitig gedrueckt worden sein)
			buttonpattern=1<<n;
			if ((portval & buttonpattern) && !(button_buffer & buttonpattern)) button_changed(n,0);	// Taster losgelassen
			if (!(portval & buttonpattern) && (button_buffer & buttonpattern)) button_changed(n,1);	// Taster gedrueckt
		}
		button_buffer=portval;	// aktuellen port wert in buffer fuer naechsten Vergleich uebernehmen
	}
}




/**
* Taster geaendert
*
* \param  buttonno <Beschreibung>
* \param  buttonval <Beschreibung>
*
* @return void
*/
void button_changed(unsigned char buttonno, __bit buttonval)
{
	unsigned char command,bedienung;
	__bit objval=0;
	//long duration=1;		// falls seitens ETS falsch programmiert 8ms default
	switch ((eeprom[FUNCTION+(buttonno>>1)] >> ((buttonno & 0x01)*4)) & 0x0F) {		// Funktion des Tasters

	/*********************
	 * Funktion Schalten
	 *********************/
	case 1:
		if (buttonval) command = (((eeprom[COMMAND+(buttonno*4)]) >> 6) & 0x03);	// Befehl beim druecken
		else command = (((eeprom[COMMAND+(buttonno*4)]) >> 4) & 0x03);				// Befehl beim loslassen
		switch (command) {	// Befehl des Tasters bei Schalten
		case 1:		// EIN
			objval=1;
			break;
		case 2:		// UM
			objval = read_obj_value(buttonno)&0x01;
			objval = !objval;
			break;
		case 3:		// AUS
			objval=0;
			break;
		}
		if (command) {	// nur wenn EIN, UM oder AUS (0=keine Funktion)
			write_obj_value(buttonno,objval);
			send_obj_value(buttonno);

		switch_led(buttonno, objval);		// LED schalten
		}
		break;


	/***********************
	 * Funktion Dimmen
	 ***********************/
	case 2:
		bedienung=eeprom[COMMAND+(buttonno*4)]&0x30;
		if (buttonval) {	// Taster gedrueckt -> schauen wie lange gehalten
			if ((eeprom[COMMAND+(buttonno*4)]) & 0x04) switch_led(buttonno,0);	// wenn Betuetigungsanzeige, dann gleich beim druecken einschalten
			timercnt[buttonno+4]=eeprom[0xD6+(buttonno*4)];	// Faktor Dauer
			timerbase[buttonno+4]=(eeprom[0xD5+(buttonno*4)]&0x70)>>4;// Basis Dauer zwischen kurz und langzeit
			if (bedienung==0x20) {// umschalten der dimmrichtung...
				if(read_obj_value(buttonno+8)&0x08)bedienung=0x30;//wenn heller, dann dunkler
				else bedienung=0x10;//sonst heller
			}
			if (bedienung==0x10){	// heller
				timerstate[buttonno+4]= ((eeprom[0xD4+(buttonno*4)]&0xF0)>>4)+0x20;	// dimmen
			}
			if(bedienung==0x30){	//  dunkler
				timerstate[buttonno+4]= ((eeprom[0xD4+(buttonno*4)]&0x0F))+0x20;	// dimmen
			}


		}
		else {		// Taster losgelassen
			if ((timerstate[buttonno+4]& 0xF0)==0x20) { // wenn delaytimer noch lauft, dann Schalten, also EIS1 telegramm senden
				if(bedienung==0x20) { //umschalten des Schaltzustandes
					if(read_obj_value(buttonno)& 0x01)bedienung=0x30;//wenn ein, dann aus
					else bedienung=0x10; // sonst ein
				}
				if(bedienung== 0x10){	//  ein
					write_obj_value(buttonno,1);
					send_obj_value(buttonno);
					switch_led(buttonno,1);
				}
				else{	//  aus
					write_obj_value(buttonno,0);
					send_obj_value(buttonno);
					switch_led(buttonno,0);
				}
			}
			else {	// Timer schon abgelaufen (also dimmen), dann beim loslassen stop-telegramm senden
				write_obj_value(buttonno+8,read_obj_value(buttonno+8)& 0x08);	// auch wenn Stopp Telegramm nicht gesendet wird, Objektwert auf 0 setzen
				if ((eeprom[COMMAND+(buttonno*4)] & 0x40) == 0) {	// ... natuerlich nur wenn parameter dementsprechend (0=senden!!!)
					send_obj_value(buttonno+8);// Stop Telegramm senden
				}
			}
			timercnt[buttonno+4]=0;
			timerstate[buttonno+4]=0;
		}
		break;


	/****************************
	 * Funktion Jalousie
	 ****************************/
	case 3:
		if (buttonval) {	// Taster gedrueckt -> schauen wie lange gehalten
			//send_eis(1, buttonno, ((eeprom[0xD3+(buttonno*4)]&0x10)>>4));	// Kurzzeit telegramm immer bei Drücken senden
			write_obj_value(buttonno,((eeprom[0xD3+(buttonno*4)]&0x10)>>4));
			send_obj_value(buttonno);
//codesparen			switch_led(buttonno,1);	// Status-LED schalten (hier nur nie,immer,betätigung)
			timercnt[buttonno+4]=eeprom[0xD5+(buttonno*4)];	// Faktor Dauer
			timerbase[buttonno+4]=(eeprom[0xD4+(buttonno*4)]&0x06)*2;// Basis Dauer zwischen kurz und langzeit
			timerstate[buttonno+4]=((eeprom[0xD3+(buttonno*4)]&0x10)>>4)+0x30;
		}
		else {	// Taster losgelassen
			if (timerstate[buttonno+4] == 0x40) {//innerhalb T2
				write_obj_value(buttonno,((eeprom[0xD3+(buttonno*4)]&0x10)>>4));
				send_obj_value(buttonno);
			}
			else{
				//timercnt[buttonno+4]=0; der ist bereits 0
				timerstate[buttonno+4]=0;// T2 bereits abgelaufen
			}
		}
		break;

		/****************************
		 * Funktion Wertgeber
		 ****************************/

	case 4:	// Wertgeber..
		switch ((eeprom[0xD3+(buttonno*4)]>>4)& 0x07){// Art des Wertgebers holen
		case 0: // ++++++++ Lichtszenenabruf ohne Speicherfunktion
			if(buttonval){
//codesparen				if ((eeprom[COMMAND+(buttonno*4)]) & 0x04) switch_led(buttonno,0);	// wenn Betuetigungsanzeige, dann gleich beim druecken einschalten
				write_obj_value(buttonno+8,eeprom[0xD5+(buttonno*4)]);
				send_obj_value(buttonno+8);
//codesparen				switch_led(buttonno,0);
			}
			break;
		case 1:	// ++++++++ Lichtszenenabruf mit Speicherfunktion
			if(buttonval){// beim drücken
				// Tastendruck länger 5 Sekunden?
				timercnt[buttonno+4]=156;
				timerbase[buttonno+4]=2; //(32ms)
				timerstate[buttonno+4]=0x50;// Betätigung länger 5 sekunden bei Lichtszene

			}
			else{// nach loslassen...
				if (timerstate[buttonno+4]==0x50){// wenn 5 sekunden noch nicht erreicht LZ senden
					write_obj_value(buttonno+8,eeprom[0xD5+(buttonno*4)]);
//					switch_led(buttonno,1);
				}
				else{ // sonst speichern
					write_obj_value(buttonno+8,eeprom[0xD5+(buttonno*4)]|0x80);
				}
				send_obj_value(buttonno+8);
				timerstate[buttonno+4]=0;
				timercnt[buttonno+4]=0;
			}
			break;
/*		case 2: // ++++++  Helligkeitswertgeber
			if (buttonval){
				write_obj_value(buttonno+8,(eeprom[0xD5+(buttonno*4)]<<8)+ eeprom[0xD6+(buttonno*4)]);
				send_obj_value(buttonno+8);
//				switch_led(buttonno,1);
				if(!(eeprom[0xD3+(buttonno*4)]& 0x80)){// schauen ob Verstellung freigegeben
					timercnt[buttonno+4]=156;
					timerbase[buttonno+4]=2; //(32ms)
					timerstate[buttonno+4]=0x70;// Betätigung länger 5 sekunden bei Dimmwert
				}
			}
			else{
				if (timerstate[buttonno+4]==0x80){
					//dimmwert abspeichern kommt noch
				}
				timerstate[buttonno+4]=0;
			}
			break;
		case 3:	// ++++++  Temperaturwertgeber
			if (buttonval){
				write_obj_value(buttonno+8,(eeprom[0xD5+(buttonno*4)]<<8)+ eeprom[0xD6+(buttonno*4)]);
				send_obj_value(buttonno+8);
//				switch_led(buttonno,1);
				if(!(eeprom[0xD3+(buttonno*4)]& 0x80)){// schauen ob Verstellung freigegeben
					timercnt[buttonno+4]=156;
					timerbase[buttonno+4]=2; //(32ms)
					timerstate[buttonno+4]=0x90;// Betätigung länger 5 sekunden bei Dimmwert
				}
			}
			else{
				if (timerstate[buttonno+4]==0xA0){
					//dimmwert abspeichern kommt noch
					//bereits in delay timer geschehen
				}
				timerstate[buttonno+4]=0;
			}
			break;
*/		case 4: // ++++++   Dimmwertgeber
			if (buttonval){
				write_obj_value(buttonno+8,eeprom[0xD5+(buttonno*4)]);
				send_obj_value(buttonno+8);
//codesparen				switch_led(buttonno,1);
				if(!(eeprom[0xD3+(buttonno*4)]& 0x80)){// schauen ob Verstellung freigegeben
					timercnt[buttonno+4]=156;
					timerbase[buttonno+4]=2; //(32ms)
					timerstate[buttonno+4]=0xB0;// Betätigung länger 5 sekunden bei Dimmwert
				}
			}
			else{
				if (timerstate[buttonno+4]==0xC0){
					//dimmwert abspeichern kommt noch
	    			EA=0;		// Interrupts sperren, damit flashen nicht unterbrochen wird
	    			START_WRITECYCLE
	    			WRITE_BYTE(0x01,0xD5+(buttonno*4),read_obj_value(buttonno+8));
	    			STOP_WRITECYCLE // Lichtszene speichern
	    			EA=1;

				}
				timerstate[buttonno+4]=0;
			}
			break;
		}

		break;
	}
}

/*
// bringt den Messwert ins Sendeformat EIS 5 oder EIS 6
//
// \param  objno
//
// @return sendewert
//
unsigned int sendewert(unsigned char objno)
{
    int eis5temp, eis6temp;
    unsigned char objno_help, n;

    objno_help=objno>>1;

    eis6temp=-5500;
    n=255;

    // Sendeformat EIS 6
    if ((eeprom[0xA4]>>4)&(1<<objno_help))
    {
        while(eis6temp<temp[objno])
        {
            n++;
            eis6temp+=70;
            if (n&0x01) eis6temp++;
        }
        return n;
    }

    // Sendeformat EIS 5
    else
    {
        eis5temp=(temp[objno_help]>>3)&0x07FF;          // durch 8 teilen, da sp�ter Exponent 3 dazukommt
        eis5temp=eis5temp+(0x18 << 8);
        if (temp[objno_help]<0) eis5temp+=0x8000;       // Vorzeichen

        return eis5temp;
    }
}
*/



int eis5conversion(int zahl,unsigned char Typ)
{
	unsigned char exp=0;
	unsigned int wert=0;
	if (Typ==4){// Helligkeitwert
		exp=3;// Da kleinster wert 50 lux*100=5000 ==> 5000/8 (exp=3)
	 	wert=zahl*625;//= 625
	}
	if (Typ==6){// Temperaturwert kleinster wert =1 größter 31

		wert=zahl*100;// Hier reicht uns eine 16bit int var
	}
	if(Typ==8)wert=zahl;
	if (Typ==7){// wenn Dimmwert ( EIS2, also keine Fließkomma)
		wert=zahl;
	}
	else{// fließkomma EIS5 berechnen
	 		while (wert > 2047){//solange Mantisse größer 11 Bit
	 			wert=wert>>1;// Mantisse /2
	 			exp++;// und exponent um 1 erhöhen (ist ein 2^exp)
	 		}
	}
 	return (wert|(exp<<11));// exponent dazu, geht auch bei EIS2 da EXP hier 0 ist.
}
/*
signed int eis5conversion(signed int zahl)// wandelt 32 bit var in eis5 um
{
	unsigned char exp=0;
	if(zahl>=0)
	{
		while (zahl > 2047){//solange Mantisse größer 11 Bit
			zahl=zahl>>1;// Mantisse /2
			exp++;// und exponent um 1 erhöhen (ist ein 2^exp)
		}
	}
	else
	{
		while (zahl < -2048){//solange Mantisse größer 11 Bit
			zahl=zahl >> 1;// Mantisse /2
			zahl=zahl|0x8000;// signed bit reparieren
			exp++;// und exponent um 1 erhöhen (ist ein 2^exp)
		}
		zahl=zahl & 0x87FF;
	}
 	return ((signed int)zahl|(exp<<11));// exponent dazu.
}
*/

unsigned long read_obj_value(unsigned char objno)
{
//	unsigned int retvalue;
	if(objno<8)	return((bitobject>>objno)&0x01);
	//else if (objno == 16) return dimmwert;	// Helligkeit Status LEDs
	else return(object_value[objno-8]);

}


void write_obj_value(unsigned char objno, unsigned int objval)
{
	if(objno<8){
		if(!objval)bitobject&=(~((0x01)<<objno));
		else bitobject|=((0x01)<<objno);
	}
	else {
		object_value[objno-8]=objval;
	}
}



/**
* Ausgaenge schalten gemaess EIS 1 Protokoll (an/aus)
*
* \param void
* @return void
*/
void write_value_req(unsigned char objno)
{
	unsigned char objtype;
	if (objno<12) {					// max 12 objekte (0-11)
		objtype=eeprom[eeprom[COMMSTABPTR]+objno*3+4];
		if (objtype<=6){
			write_obj_value(objno,telegramm[7]& 0x3F); //bit 6+7 löschen (0x40,0x80)
		}
		if (objtype==7)write_obj_value(objno,telegramm[8]);
		if (objtype==8)write_obj_value(objno,telegramm[9]+(telegramm[8]<<8));
		if ((objno<4) && ((eeprom[COMMAND+(objno*4)]) & 0x07) <4) switch_led(objno,telegramm[7]&0x01);	// LED nur schalten, wenn nicht auf Bet�tigungsanzeige parametriert
	}
	else if (objno==16)	// Helligkeitsobjekt LEDs, (Objekte 12-15 sind unsichtbar)
	{
		dimmwert = telegramm[8];
	}
}


/**
* Objektwert lesen wurde angefordert, read_value_response Telegramm zurücksenden
*
*
* @return
*
*/
void read_value_req(unsigned char objno)
{
			send_obj_value(objno+0x40);
}



/**
* LEDs schalten entsprechend der parametrierung
*
* \param  ledno <Beschreibung>
* \param  onoff <Beschreibung>
*
* @return void
*/
void switch_led(unsigned char ledno, __bit onoff)
{
	unsigned char command,ledvar;

	if (ledno<4) {
		command = ((eeprom[COMMAND+(ledno*4)]) & 0x07);	// Befehl der Status LED
		switch (command) {
		case 0:		// immer AUS
			onoff=0;
			break;
		case 1:		// immer an
			onoff=1;
			break;
		// case 2 ist Statusanzeige, onoff bleibt also unveraendert
		case 3:		// LED = invertierte Statusanzeige
			onoff=!onoff;
			break;
/*		case 4:		// LED = Betaetigungsanzeige
			onoff=1;	// erstmal an beim druecken der Taste
			timerstate[ledno]= 0x10;
			timerbase[ledno]=1;
			switch (eeprom[LED_DURATION]) {		// dann ueber delay-timer aus
			case 38:	// 0,75 sec
				timercnt[ledno]=47;
				break;
			case 118:	// 2,25 sec
				timercnt[ledno]=141;
				break;
			case 150:	// 3 sec
				timercnt[ledno]=188;
				break;
			}
*/		}
		ledvar=LEDSTATE;
		ledvar&= ~(1<<(ledno+4));	// LEDs sind an Pin 4-7
		ledvar |= ((onoff<<(ledno+4)) | 0x0F);	// unteren 4 bits immer auf 1 lassen !!!
		LEDSTATE=ledvar;
	}
}


void timer0_int  (void) __interrupt (1) // Interrupt T0 für soft PWM LED
{
  dimmcompare++;
  TF0=0;
	if((dimmcompare) <= dimmwert) PORT = LEDSTATE | 0x0F;// LEDs ein
	else PORT = 0x0F;//LEDs aus
	// unteren 4 bits immer auf 1 lassen !!!  //LEDSTATE=0x0F;
} // timer0_int



/**
* zaehlt alle 8ms die Variable Timer hoch und prueft records
*
* \param void
* @return void
*/
const unsigned char tele_repeat_value[8]={63,125,188,250,25,38,50,94};	// 3Bit: 500ms, 1s, 1.5s, 2s, 200ms, 300ms, 400ms, 750ms

void delay_timer(void)
{
	unsigned char objno, delay_value,ledvar,tmp,m,n;
	unsigned int i_tmp;
	i_tmp;
//	long delval;
//	long duration=1;
	ledvar;
	RTCCON=0x60;
	RTCH=0x00;//RTCH=0x01;
	RTCL=0xE6;//RTCL=0xCD;
	RTCCON=0x61; //	start_rtc(8) RTC neu starten mit 4ms
// +++++++  Hier werden alle timer  gemäß ihrer basis decremntiert
	timer++;
	timerflags = timer&(~(timer-1));// flanke generieren
	for(n=0;n<16;n++){
		if(timerflags & 0x0001){// positive flags erzeugen und schieben
			for(m=0;m<8;m++){// die timer der reihe nach checken und dec wenn laufen
				if( timercnt[m]){
					if ((timerbase[m]& 0x0F)==n){// wenn die base mit der gespeicherten base übereinstimmt
						timercnt[m]=timercnt[m]-1;// den timer[m]decrementieren
					}//end if(timerbase...
				}//end if(timercount..)
			}// end  for(m..
		}// end if (timer...
		timerflags = timerflags>>1;
	}//end for (n=...

	for(objno=0;objno<8;objno++) {
		if(!timercnt[objno]) {	// ... es ist also soweit
			delay_value=timerstate[objno]& 0x0F;

			switch(timerstate[objno] & 0xF0){
//		if(delay_state) {			// 0x00 = delay Eintrag ist leer
			case 0x10:
			//	if (objno<4) {	// LED bei Betaetigungsanzeige nach eingestellter Zeit ausschalten
			//		ledvar=LEDSTATE;
			//		ledvar &= ~(1<<(objno+4));	// LEDs sind an Pin 4-7
			//		ledvar |= 0x0F;				// unbedingt taster pins wieder auf 1
			//		LEDSTATE=ledvar;
			//		timerstate[objno]=0;
			if(!sequence)sequence=1;
			break;
			case 0x20:	// Dimmen 0xF0 -> 0xD0 | 0x20
				timerstate[objno] |=0xD0;
			case 0xF0:
				write_obj_value(objno+4,delay_value);
				send_obj_value(objno+4);		// dimmkommando senden

				if(!(eeprom[0xD5+((objno-4)*4)]& 0x08)){// wenn Telegrammwiederholung eingeschalten

				timercnt[objno]=tele_repeat_value[eeprom[0xD5+((objno-4)*4)]& 0x07];
//				timercnt[objno]=tmp;
				timerbase[objno]=0;
				}
				else{
					timercnt[objno]=0;
					timerstate[objno]=0;
				}
			break;

			//	else {	// timer-Eintraege 4-7 sind die Abfragen wie lange Taster gedrueckt, bzw. wann er losgelassen wurde
			case 0x30:	// 0x30, 0x31 für langzeit telegramm senden
						write_obj_value(objno+4,delay_value & 0x01);
						send_obj_value(objno+4);				// Langzeit Telegramm senden

						// *** timercnt neu laden für Dauer Lamellenverstellung ***
						timercnt[objno]=eeprom[DEL_FACTOR2+((objno-4)*4)];	// Faktor Dauer	T2
						timerbase[objno]=(eeprom[0xD4+((objno-4)*4)]&0x60)>>3;	// Basis Dauer T2
						if (timercnt[objno]) {	// wenn keine lamellenverstellzeit dann nix tun
							timerstate[objno]=((eeprom[COMMAND+((objno-4)*4)]&0x10)>>4)+0x40; // 0x40,0x41 fuer ende T2 (lamellenvestellzeit)
						}
						else {
							timercnt[objno]=0;
							timerstate[objno]=0;
						}
			break;
			case 0x40:

						timerstate[objno]=0; // wenn T2 abgelaufen dann nichts mehr machen
						timercnt[objno]=0;
			break;
			case 0x50: // Wertgeber Lichtszene lang drücken zum speichern
//codesparen						switch_led(objno-4,1);
						timerstate[objno]=0x60;
			break;
			case 0x60:// Wertgeber Lichtszene verstellen
			break;
/*
			case 0x70: // Wertgeber Helligkeit langdrücken
			case 0x80: // Wertgeber Helligkeit verstellen
				timerbase[objno]=0;
				timercnt[objno]=tele_repeat_value[(eeprom[0xD4+((objno-4)*4)]& 0x30)>>4];// schrittweite und zeit zw. telegrammen
				i_tmp=(eeprom[0xD5+4*(objno-4)])<<8;
				i_tmp +=eeprom[0xD6+4*(objno-4)];
				if (i_tmp==0x39FD || i_tmp==0x3828)	//Differenz ausgleichen
				{
					i_tmp--;
				}
				i_tmp -= 0x27;

				if(!(i_tmp&0x0800)){
					i_tmp=0x03C94;
				}
				else{
//codesparen					switch_led(objno-4,1);
				}
				write_obj_value(objno+4,i_tmp);// wir rechnen immer mit exp 2^7
				timerstate[objno]=0x80;
    			EA=0;		// Interrupts sperren, damit flashen nicht unterbrochen wird
    			START_WRITECYCLE
    			WRITE_BYTE(0x01,0xD5+((objno-4)*4),read_obj_value(objno+4)>>8);
    			WRITE_BYTE(0x01,0xD6+((objno-4)*4),read_obj_value(objno+4)& 0xFF);
    			STOP_WRITECYCLE // Luxwert speichern
    			EA=1;
				send_obj_value(objno+4);

    			break;

			case 0x90: // Wertgeber Temperatur langdrücken
			case 0xA0: // Wertgeber Temperatur verstellen
				timerbase[objno]=0;
				timercnt[objno]=tele_repeat_value[(eeprom[0xD4+((objno-4)*4)]& 0x30)>>4];// schrittweite und zeit zw. telegrammen
				i_tmp=(eeprom[0xD5+4*(objno-4)]&0x07)<<8;
				i_tmp +=eeprom[0xD6+4*(objno-4)];
				if(i_tmp>=0x0032){
					i_tmp -= 0x32;
//codesparen					switch_led(objno-4,1);
				}
				else{
					i_tmp=0x07D0;
				}
				write_obj_value(objno+4,i_tmp|0x0800);// wir rechnen immer mit exp 2^1
				timerstate[objno]=0xA0;
    			EA=0;		// Interrupts sperren, damit flashen nicht unterbrochen wird
    			START_WRITECYCLE
    			WRITE_BYTE(0x01,0xD5+((objno-4)*4),read_obj_value(objno+4)>>8);
    			WRITE_BYTE(0x01,0xD6+((objno-4)*4),read_obj_value(objno+4)& 0xFF);
    			STOP_WRITECYCLE // Temperaturwert speichern
    			EA=1;
				send_obj_value(objno+4);

			break;
*/
			case 0xB0: // Wertgeber Dimmer langdrücken
			case 0xC0: // Wertgeber Dimmer verstellen
					tmp=read_obj_value(objno+4);//
					m=eeprom[0xD4+((objno-4)*4)]& 0x3F;// schrittweite und zeit zw. telegrammen
					n= m & 0x0F;  // Schrittweite
					m=m>>4;// zeit zwischen telegrammen
					if (!tmp){
						tmp=255;
					}
					else{
						if (tmp>=n)tmp-=n;
						else tmp=0;
					}
					write_obj_value(objno+4,tmp);
//codesparen					switch_led(objno-4,1);
					send_obj_value(objno+4);		// dimmkommando senden
					timerstate[objno]=0xC0;
					timercnt[objno]=tele_repeat_value[m];// 0,5/1/1,5/2 Sec.
					timerbase[objno]=0;
			break;
			default:
			}// ende switch (timerstate...
		}// ende if(!timercnt...
	}//ende for(objno=0....
}


/**
* Alle Applikations-Parameter zuruecksetzen und Empfang starten
*
*
* \param void
* @return void
*/
void restart_app(void)
{
	unsigned char n;
	__bit write_ok=0;

//	AUXR1  |= CLKLP;

	// Pin 0-3 fuer Taster
	for (n=0;n<4;n++) {
		SET_PORT_MODE_BIDIRECTIONAL(n)
	}

	// Pin 4-7 fuer LEDs
	for (n=4;n<8;n++) {
		SET_PORT_MODE_PUSHPULL(n)
	}

	PORT=0x0F;			// Taster auf high, LEDs zunächst aus

	button_buffer=0x0F;	// Variable für letzten abgearbeiteten Taster Status

//	RTCCON=0x60;	//stop_rtc();
//	RTCH=0x00;
//	RTCL=0xE6;
	RTCCON=0x81;	//start_rtc(8);		// RTC neu mit 8ms starten

	timer=0;			// Timer-Variable, wird alle 8ms inkrementiert

	EA=0;		// Interrupts sperren
	// Applikations-spezifische eeprom Eintraege schreiben
	START_WRITECYCLE
	WRITE_BYTE(0x01,0x03,0x00)	// Herstellercode 0x0004 = Jung
	WRITE_BYTE(0x01,0x04,0x04)
//	WRITE_BYTE(0x01,0x05,0x10)	// Devicetype 0x1052 = Jung Tastsensor 2092
//	WRITE_BYTE(0x01,0x06,0x52)
//	WRITE_BYTE(0x01,0x07,0x01)	// Versionsnummer
	WRITE_BYTE(0x01,0x0C,0x00)	// PORT A Direction Bit Setting
	WRITE_BYTE(0x01,0x0D,0xFF)	// Run-Status (00=stop FF=run)
//	WRITE_BYTE(0x01,0x12,0x9A)	// COMMSTAB Pointer
	STOP_WRITECYCLE
//	START_WRITECYCLE;
//	WRITE_BYTE(0x00,0x60,0x2E);	// system state: all layers active (run), not in prog mode
//	STOP_WRITECYCLE;

	for (n=0;n<15;n++) write_obj_value(n,0);		// Objektwerte alle auf 0 setzen

	for (n=0;n<8;n++) timercnt[n]=0;		// delay records loeschen



	// set timer 0 autoreload 0.05ms
	TR0=0;
	TMOD &= 0xF0;
	TMOD |= 0x02;// T0 autoreload
	TH0=0x47;
	TL0=0x47;
	TR0=1;
	 // set timer 0 isr priority to 0
	IP0 &= 0xFC; //FC		F6	für flackerfrei bei 1 kanal
	IP0 |= 0x0C; //0C		06	dto.
	IP0H &= 0xF4;//
	IP0H |= 0x04;// 		Timer 1 auf Level 2

	ET0=1;// timer 0 interupt freigeben

	TF0=0; //timer0 flag löschen
	EA=1;// Interrupts freigeben

//	P2M1 &= ~0x80;
//	P2M2 &= ~0x80; // P2.7 bidirektional

}

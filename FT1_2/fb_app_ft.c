/*
 *      __________  ________________  __  _______
 *     / ____/ __ \/ ____/ ____/ __ )/ / / / ___/
 *    / /_  / /_/ / __/ / __/ / __  / / / /\__ \ 
 *   / __/ / _, _/ /___/ /___/ /_/ / /_/ /___/ / 
 *  /_/   /_/ |_/_____/_____/_____/\____//____/  
 *                                      
 *  Copyright (c) 2008-2012 Andreas Krebs <kubi@krebsworld.de>
 *  Copyright (c) 2015		Andreas Krieger 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#define FB_APP_FT_C
#include "fb_app_ft.h"

//#define ROUTERMODE		// 115200,n,8,1 (auskommentieren f�r 19200,e,8,1)

// Global variables now defined only once in fb_app_ft.h unsing switch FB_APP_FT_C. tuxbow.




#define FT_SEND_T_DATACONNECTED_CONF \
	FT_SET_HEADER(rsin[1],0x8E) \
	ft_send_frame();

#define FT_SEND_L_DATA_CONF \
	FT_SET_HEADER(rsin[1],0x2E) \
	rsin[6]=0xB0 + (rsin[6] & 0x0F); \
	rsin[7]=eeprom[ADDRTAB+1]; \
	rsin[8]=eeprom[ADDRTAB+2]; \
	ft_send_frame();

#define FT_SET_HEADER(length, control) \
	rsin[0]=0x68; \
	rsin[1]=length; \
	rsin[2]=length; \
	rsin[3]=0x68; \
	fcb=!fcb; \
	rsin[4]=0xD3+(fcb<<5); \
	rsin[5]=control;

#define FT_SET_BUS_HEADER(length, control) \
	rsout[0]=0x68; \
	rsout[1]=length; \
	rsout[2]=length; \
	rsout[3]=0x68; \
	fcb=!fcb; \
	rsout[4]=0xD3+(fcb<<5); \
	rsout[5]=control;

#ifdef ROUTERMODE

#define FT_SEND_CHAR(sc) \
	SBUF=sc; \
	while(!TI); \
	TI=0; \
	for(n=0;n<10;n++) TI=0;

#else

#define FT_SEND_CHAR(sc) \
	{SBUF=sc; \
	TB8=0; \
	for(n=1;n!=0;n=n<<1) { \
		if (sc & n) TB8=!TB8; \
	} \
	while(!TI); \
	TI=0;}
//for(n=0;n<10;n++) TI=0;
#endif

// FT Acknowledge an PC senden
#define FT_SEND_ACK FT_SEND_CHAR(0xE5);	r_fbc=!r_fbc;



unsigned char timer_data;
unsigned char send_confirm;
volatile __bit	rsout_busy;	// zeigt an dass der rsout belegt ist. Wird nach Senden des selben geloescht.
__bit L_Data_conf_done;
__bit ft_process_var_frame_repeat_request;
__bit ft_ack_request;
__bit r_fbc;
volatile 	__bit rsin_parity_error;
void ft_process_var_frame(void)
{
	unsigned char n;
	__bit write_ok = 0;
    unsigned char crc;
	if (rsin[0] == 0x68 && rsin[3] == 0x68 && rsin[1] == rsin[2])
	{	// Multi Byte
      timer_data = 2;	// timer starting data LED
	  crc=0x00;
	  for(n=0;n<rsin[1];n++)
	  {
		  crc += rsin[n+4];
	  }
	  if((crc==rsin[rsin[1]+4])&&((__bit)(rsin[4]&0x20)==r_fbc))
	  {
		if ((rsin[4] & 0xDF) == 0x53)
		{		// send_Udat
			switch (rsin[5])
			// PEI10 message code
			{
			case 0x11:		// send a telegram on the bus
				//while (fb_state != 0);
				if(!s_telegramm_belegt)
				{
					send_obj_value(0);
					FT_SEND_ACK
					ft_process_var_frame_repeat_request = 0; 	
				}
				else
				{
					// nicht erfolgreich dann wieder setzen
					ft_process_var_frame_repeat_request = 1; 	
				}
				
				break;

			case 0xA9:		// PEI_switch_request	
				FT_SEND_ACK
				if (rsin[6] == 0x00 && rsin[8] == 0x34)
				{
					if (rsin[7] == 0x12)
					{
						if (rsin[9] == 0x56 && rsin[10] == 0x78 && rsin[11] == 0x9A)
							switch_mode = 0x00; // normal mode
						if (rsin[9] == 0x56 && rsin[10] == 0x78 && rsin[11] == 0x8A)
							switch_mode = 0x01; // application layer
						if (rsin[9] == 0x48 && rsin[10] == 0x88 && rsin[11] == 0x0A)
							switch_mode = 0x02; // transport layer remote
						if (rsin[9] == 0x56 && rsin[10] == 0x78 && rsin[11] == 0x0A)
							switch_mode = 0x03; // transport layer local
					}
					if (rsin[7] == 0x18)
					{
						if (rsin[9] == 0x56 && rsin[10] == 0x78 && rsin[11] == 0x0A)
							switch_mode = 0x04; // link layer
					}
				}
				if (rsin[6] == 0x90 && rsin[7] == 0x18 && rsin[8] == 0x34
						&& rsin[9] == 0x56 && rsin[10] == 0x78 && rsin[11] == 0x0A)
				{
					switch_mode = 0x05; // busmonitor mode
					auto_ack = 0;
				}
				else
					auto_ack = 1;
				break;

			case 0x43:		// T_connect_request
				FT_SEND_ACK
				if (switch_mode == 0x03)
				{
					FT_SET_HEADER(0x07, 0x86)
					rsin[6] = (0x00);
					rsin[7] = (0x00);
					rsin[8] = (0x00);
					rsin[9] = (0x00);
					rsin[10] = (0x00);
					ft_send_frame();
				}
				break;

			case 0x44:	// T_Disconcect.req
				FT_SEND_ACK
				if (switch_mode == 0x03)
				{
					FT_SET_HEADER(rsin[1], 0x88)// UNKLAR ###############
					ft_send_frame();
				}
				break;

			case 0x41:		// T_dataConnected.request
				FT_SEND_ACK
				if (switch_mode == TLlocal)
				{ // Transport Layer local
					switch (rsin[12])
					{
					case 0x03:
						switch (rsin[13])
						{
						case 0x00:		// Read_Mask_Version_Req	  
							FT_SEND_T_DATACONNECTED_CONF
							FT_SET_HEADER(0x0C, 0x89)
							rsin[6] = (0x00);
							rsin[7] = (0x00);
							rsin[8] = (0x00);
							rsin[9] = (0x00);
							rsin[10] = (0x00);
							rsin[11] = (0x63);	// DRL L�nge 3 Bytes
							rsin[12] = (0x03);	// 03 40 = Read_Mask_Version_res
							rsin[13] = (0x40);	//
							rsin[14] = (0x00);	// Maskenversion 00 21
							rsin[15] = (0x21);
							ft_send_frame();
							break;
						case 0xD5:	// Read_Property_Value_Req
							if (rsin[14] == 0x01 && rsin[15] == 0x05
									&& rsin[16] == 0x10 && rsin[17] == 0x01)
							{
								FT_SEND_T_DATACONNECTED_CONF
								FT_SET_HEADER(0x0F, 0x89)
								rsin[11] = 0x66;
								rsin[13] = 0xD6;
								rsin[18] = property_5;
								ft_send_frame();
							}
							break;
						case 0xD7:	// Write_Property_Value_Req
							if (rsin[14] == 0x01 && rsin[15] == 0x05
									&& rsin[16] == 0x10 && rsin[17] == 0x01)
							{
								FT_SEND_T_DATACONNECTED_CONF
								if (rsin[18] == 0x01)
									property_5 = 0x02;
								else
									property_5 = 0x01;
								FT_SET_HEADER(0x0F, 0x89)
								rsin[11] = 0x66;
								rsin[13] = 0xD6;
								rsin[18] = property_5;
								ft_send_frame();
							}
							break;

						case 0xD1:		// Authorize_Req
							FT_SEND_T_DATACONNECTED_CONF
							rsin[4] |= 0x80;	// DIR=1 BAU to external device
							//rsin[4]^=0x20;		// toggle FCB

							FT_SET_HEADER(0x0B, 0x89)
							rsin[11] = 0x62;							// 66
							rsin[13] = 0xD2;
							ft_send_frame();
							break;
						}
						break;

					case 0x02:
						switch (rsin[13] & 0xF0)
						{
						case 0x00:	// Read_Memory_Req
							FT_SEND_T_DATACONNECTED_CONF
							ft_send_Read_Memory_Res((rsin[13] & 0x0F), rsin[14],
									rsin[15]);
							break;

						case 0x80:	// Write_Memory_Req

							EA = 0;
							//EX1=0;
							write_ok = 0;
							while (!write_ok)
							{
								START_WRITECYCLE
								;
								for (n = 0; n < (rsin[13] & 0x0F); n++)
								{
									WRITE_BYTE(rsin[14], rsin[15]+n,
											rsin[16+n]);
								}
								STOP_WRITECYCLE
								;
								if (!(FMCON & 0x0F))
									write_ok = 1;// pr�fen, ob erfolgreich geflasht
							}
							EA = 1;

							FT_SEND_T_DATACONNECTED_CONF

							//EX1=1;
							break;
						}
					}
					break;
				}
				break;

			case 0xA7:		// PEI_identify_request
				PEI_identify_req();
				break;
			}
		}
	  } // CRC	und r_fbc
	}
	//rsinpos=0;
}

void ft_process_fix_frame(void)		// frame with fixed length received
{
	unsigned char n;

	if (rsin[0] == 0x10 && rsin[3] == 0x16 && rsin[1] == rsin[2])
	{	// Single Byte
		if ((rsin[1] & 0x0F) == 0x00)
		{	//send_reset received
			FT_SEND_CHAR(0xE5) // ACK ohne das r_fbc zu toggeln
			restart_app();
			// send an ack
		}
		if (rsin[1] == 0x49)
		{		// N_DataConnected.ind received
			FT_SEND_CHAR(0x10)
			FT_SEND_CHAR(0x8B)
			FT_SEND_CHAR(0x8B)
			FT_SEND_CHAR(0x16)
		}
	}
	//rsinpos=0;
}

void ft_send_Read_Memory_Res(unsigned char bytecount, unsigned char adrh,
		unsigned char adrl)
{
	unsigned char n;

	FT_SET_HEADER(bytecount+12, 0x89)

	rsin[6] = 0x0C;
	rsin[7] = 0x00;
	rsin[8] = 0x00;
	rsin[9] = 0x00;
	rsin[10] = 0x00;
	rsin[11] = bytecount + 3;
	rsin[12] = 0x02;
	rsin[13] = 0x40 + bytecount;
	rsin[14] = adrh;
	rsin[15] = adrl;
	if (switch_mode == 0x03)
		for (n = 0; n < bytecount; n++)
			rsin[n + 16] = eeprom[adrl + n];
	ft_send_frame();
}

void ft_process_telegram(void)		// EIB telegram received
{
	unsigned char n;

	tel_was_acked = 0;
	rsout_busy=1;
	if (switch_mode == 0x05)		// busmonitor
	{
		FT_SET_BUS_HEADER((r_telegramm[5]&0x0F)+13, 0x2B)
		rsout[6] = 0x01;	// status
		rsout[7] = 0x22;	// timestamp
		rsout[8] = 0x33;	// timestamp
		for (n = 0; n < (rsout[1] - 4); n++)
			rsout[n + 9] = r_telegramm[n];	// -1
		if(tel_arrived)	// nach Telegramm �bernahme muss tel_arrived noch 1 sein, da es im Empfang byte 0(ausser ac,nack,busy) gel�scht wird.
		{				// Ein gel�schtes tel_arrived bedeutet dass ein n�chstes Telegramm den r_telegramm zumindest angefangen 
			ft_send_bus_frame(); // hat zu �berschreiben!!
	
			if (tel_was_acked)
			{
				FT_SET_BUS_HEADER(0x06, 0x2B)
				rsout[6] = 0x01;
				rsout[7] = 0x33;	// timestamp
				rsout[8] = 0x44;	// timestamp
				rsout[9] = 0xCC;
				ft_send_bus_frame();
			}
		}
	}
	else
	{
		FT_SET_BUS_HEADER((r_telegramm[5]&0x0F)+9, 0x29)
		// +9
		for (n = 0; n < (rsout[1] - 1); n++)
			rsout[n + 6] = r_telegramm[n];	// -1
		if(tel_arrived)	ft_send_bus_frame();// Abfrage tel_arrived siehe Kommentar im busmonitor mode.
	}
	tel_arrived = 0;// hier loeschen, damit nicht erneut ft_process_tel aufgerufen wird

}

void ft_send_bus_frame(void)// send a frame with variable length that is stored in rsout
{
	unsigned char b, n, repeat, frame_length, send_char;
	unsigned int timeout;
	repeat = 4;
	while (repeat--)
	{		// repeat sending frame up to 3 times if not achnowleged
		//if(repeat<3)auto_ack=0;// automatische Best�tigen in der Statemachine abschalten bis �bertragung geschehen ist
		rsout[rsout[1] + 4] = 0;
		for (n = 4; n < (rsout[1] + 4); n++)
			rsout[rsout[1] + 4] += rsout[n];	// checksum berechnen
		rsout[rsout[1] + 5] = 0x16;

		frame_length = rsout[1] + 6;
		send_char = rsout[0];
		for (b = 0; b < frame_length; b++)
		{
			SBUF = send_char;
			
			TB8 = 0;
			for (n = 1; n != 0; n = n << 1)
			{
				if (rsout[b] & n)
					TB8 = !TB8;
			}
			if (ack) tel_was_acked = 1;// falls w�hrend dem seriellen Senden ein ACK am bus kam
			send_char = rsout[b + 1];

			while (!TI)
				;
			TI = 0;

		}

		timeout = 10000;
		while (timeout--)
		{		// give enough time to receive an ack
			if (ft_ack)
			{
				repeat = 0;
				ft_ack = 0;
				LED_run = 0;
				rsout_busy = 0;
				return;
			}
		}
	}
	LED_run = 1;
	rsout_busy = 0;
//	auto_ack=1;
}

void ft_send_frame(void)// send a frame with variable length that is stored in rsin
{
	unsigned char b, n, repeat, frame_length, send_char;
	unsigned int timeout;
	repeat = 4;
	while (repeat--)
	{		// repeat sending frame up to 3 times if not achnowleged
		rsin[rsin[1] + 4] = 0;
		for (n = 4; n < (rsin[1] + 4); n++)
			rsin[rsin[1] + 4] += rsin[n];	// checksum berechnen
		rsin[rsin[1] + 5] = 0x16;

		frame_length = rsin[1] + 6;
		send_char = rsin[0];
		for (b = 0; b < frame_length; b++)
		{
			SBUF = send_char;
			TB8 = 0;
			for (n = 1; n != 0; n = n << 1)
			{
				if (rsin[b] & n)
					TB8 = !TB8;
			}
			if (ack) tel_was_acked = 1;// fals w�hrend dem seriellen Senden ein ACK am bus kam
			send_char = rsin[b + 1];
			while (!TI)
				;
			TI = 0;

		}

		timeout = 10000;
		while (timeout--)
		{		// give enough time to receive an ack
			if (ft_ack)
			{
				repeat = 0;
				ft_ack = 0;
				LED_run = 0;
				return;
			}
		}
	}
	LED_run = 1;
}

void ft_send_fixed_frame(unsigned char controlfield)// send a frame with fixed length (single byte)
// that is passed as parameter
{
	unsigned char n, r;

	r = 0;
	while (r < 4)
	{
		rsin[0] = 0x10;
		rsin[1] = controlfield;
		rsin[2] = controlfield;
		rsin[3] = 0x16;
//		ES=0;
		for (n = 0; n < 4; n++)
		{
			FT_SEND_CHAR(rsin[n])
		}
//		ES=1;
		//if (ft_get_ack()) r=4;
		r++;
	}
}

void PEI_identify_req(void)
{
	unsigned char n;

	FT_SEND_ACK

	FT_SET_HEADER(0x0A, 0xA8)
	rsin[6] = eeprom[ADDRTAB + 1];
	rsin[7] = eeprom[ADDRTAB + 2];
	rsin[8] = 0x00;
	rsin[9] = 0x01;
	rsin[10] = 0x00;
	rsin[11] = 0x00;
	rsin[12] = 0xE4;
	rsin[13] = 0x5A;
	rsin[14] = 0;
	ft_send_frame();
}

void ft_rs_init(void)
{
	SSTAT |= 0x40;	// TI wird am Ende des Stopbits gesetzt
	BRGCON |= 0x02;	// Baudrate Generator verwenden aber noch gestoppt
#ifdef ROUTERMODE
			SCON=0x50;// Mode 1 f�r no parity (nur f�r 115200 Baud am Router erforderlich!!!)
			BRGR1=0x00;// Baudrate = cclk/((BRGR1,BRGR0)+16) = 19200 = 01 70
			BRGR0=0x30;// 115200 =  00 30
#else
	SCON = 0xD0;	// Mode 3, receive enable f�r even parity
	BRGR1 = 0x01;	// Baudrate = cclk/((BRGR1,BRGR0)+16) = 19200 = 01 70
	BRGR0 = 0x70;	// 115200 =  00 30

#endif

	BRGCON |= 0x01;	// Baudrate Generator starten
}

void serial_int(void) __interrupt (4) __using (1) // Interrupt on received char from serial port
{
	unsigned char rxc;

	ES = 0; /*????*/
	if (RI)
	{
		RI = 0;
		if (TF0)
		{      // time between 2 bytes was too long, discard previous frame.
			rsinpos = 0;
			rsin_parity_error=0;
		}
		rxc = SBUF;     // store byte in rsbuf
		ACC=rxc;	// rxc in den Akku laden zum Bilden des parity bits in hardware
		if(P != RB8)rsin_parity_error=1;
//      TI=0; //????
		if(!rsin_parity_error)
		{
			if (rsinpos == 0 && rxc == 0xE5)
			{  // in case of ack, set the ft_ack bit
				ft_ack = 1;
			}
			else	//else increment buffer pointer, if possible
			{
				rsin[rsinpos] = rxc;
				/* check if frame complete */
				if (rxc == 0x16 && rsinpos == (rsin[1] + 5))
					rsin_stat = RSIN_VARFRAME;
				if (rsin[0] == 0x10 && rsinpos == 3)
					rsin_stat = RSIN_FIXFRAME;
				if (rsin_stat != RSIN_NONE)
				{
					/* frame complete, copy it to rsin */
					rsinpos = 0;   // ready to receive next frame.
				}
				else
				{            // increment buffer pointer if there is space left
					if (rsinpos < sizeof(rsin) - 1)
					{
						rsinpos++;			
					}
				}
			}//end if(rsinpos == 0 ...
		}// end if (!rsin_parity_error...
		TR0 = 0;
#ifdef ROUTERMODE
		TH0=0xCC;   // set timer to max idle time between 2 bytes = 33 bit
#else
		/*    that is 1716�s at 19200 bps.
		 timer increments each 0.27�sec, we must divide by 1716/0.27 = 6355
		 TL0 is prescaler / 32 . 6355/32 = 199  .  256-199=57=0x39 */
		TH0 = 0x39;
#endif
		TL0 = 0xFF;
		TF0 = 0;
		TR0 = 1;
	}
	ES = 1; /*????*/
}

__bit build_tel(unsigned char objno)
{
	__bit build_ok = 1;
	unsigned char n;
	__bit repeatflag;

	repeatflag = objno & 0x20;
	build_ok = !repeatflag;
	for (n = 3; n < (rsin[1] - 2); n++)
		s_telegramm[n] = rsin[n + 6];

	s_telegramm[0] = 0xB0 + (rsin[6] & 0x0F);
	s_telegramm[1] = eeprom[ADDRTAB + 1];	// PA high
	s_telegramm[2] = eeprom[ADDRTAB + 2];	// PA low
	s_telegramm_belegt=1;
	//if(SP>stackmax)stackmax=SP;

	return build_ok;  // TODO geh�rt da nicht immer 1 ?
}

// ermittelt die Position einer GA in der GA-Tabelle, 0xFF falls nicht gefunden

unsigned char gapos_in_gat(unsigned char gah, unsigned char gal)
{
	gah;
	gal;
	return 1;						//GA wird immer wird geackt
}

void set_pa(void)
{
	while (fb_state != 0)
		;		// warten falls noch gesendet wird
	EA = 0;
	FMCON = 0x00;				// load command, leert das pageregister
	FMADRH = 0x1D;
	FMADRL = ADDRTAB + 1;
	FMDATA = s_telegramm[8];
	FMDATA = s_telegramm[9];	// n�chstes Byte, da autoinkrement
	FMCON = 0x68;// write command, schreibt pageregister ins flash und versetzt CPU in idle fuer 4ms
	EA = 1;
}

void write_value_req(void)
{

}

void read_value_req(void)
{

}

unsigned long read_obj_value(unsigned char objno)
{
	objno;
	return 0;
}

/**
 *  Der Versand des Objekts objno wurde abgeschlossen.
 *  Der Versand war erfolgreich bei success==1, erfolglos bei success==0.
 */
void send_obj_done(unsigned char objno, __bit success)
{
	objno;
	if (success)send_confirm = 1;
	else send_confirm=0;
	
	s_telegramm_belegt = 0;
//	// TODO
//	FT_SET_HEADER(rsin[1],0x2E);
//	rsin[6]=0xB0 + (rsin[6] & 0x0F);
//	rsin[7]=eeprom[ADDRTAB+1];
//	rsin[8]=eeprom[ADDRTAB+2];
//
//	ft_send_frame();
}

void ft_send_L_Data_conf()

{
	unsigned char n;
	FT_SET_BUS_HEADER((s_telegramm[5]&0x0F)+9, 0x2E)
	// +9
	for (n = 1; n < (rsout[1] - 1); n++)
	rsout[n + 6] = s_telegramm[n];	
	rsout[6]=0xB0 + (s_telegramm[0] & 0x0F);
	tel_acked = 0;
	L_Data_conf_done=1;
	//ft_send_bus_frame();// 

}

void restart_app(void)		// Alle Applikations-Parameter zur�cksetzen
{
//	P0M1 = 0x00;
//	P0M2 = 0x00;
	P0M1=0x00;		// Port 0 Modus push-pull f�r Ausgang
	P0M2=0xFF;
	P0=0;
	if (eeprom[ADDRTAB + 1] == 0 && eeprom[ADDRTAB + 2] == 0)
	{
		s_telegramm[8] = 0xFF;
		s_telegramm[9] = 0xFF;
		set_pa();
	}

	ft_rs_init();			// serielle Schnittstelle initialisieren f�r FT1.2
	rsinpos = 0;
	send_confirm = 0;
	ES = 1;					// enable serial interrupt

	switch_mode = 0x00;		// normal mode
	fcb = 0;
	r_fcb=0;
	property_5 = 0x01;
	transparency = 1;	// auch fremde Gruppentelegramme werden verarbeitet

	//auto_ack=0;		// telegramme nicht per ACK best�tigen
	RTCH = 0x0E;		// Real Time Clock auf 65ms laden
	RTCL = 0xA0;// (RTC ist ein down-counter mit 128 bit prescaler und osc-clock)
	RTCCON = 0x61;	// ... und starten

	LED_run = 1;
	LED_data = 1;

}

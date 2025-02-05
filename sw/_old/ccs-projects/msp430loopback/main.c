/*
 * BlackChat Firmware Ver. 0
 * Russell Crowe
 * Patrick LLoyd
 *
 * Semi-interrupt driven.
 * Receiver and transmitter polling runs at highest interrupt priority.
 * Receiving from UART has second highest interrupt priority,
 * other interrupts may nest in this interrupt.
 * Clearing outgoing message buffer by sending to transmitter buffer
 * and clearing incomming message buffer by sending to UART,
 * as well as handling user interface, run in the background.
 *
 * Interrupts start in the "on" state, but receiver not enabled until
 * start char "HT" received from UART
 */

#include <msp430f2617.h>
#include "VirtualWire.h"
#include <stdint.h>

#define NUL 0
#define ACK 6
#define BEL 7
#define BS 8
#define HT 9
#define LF 10
#define CR 13
#define NAK 21
#define ESC 27
#define DEL 127

const uint8_t nul = NUL;

const uint8_t ack = ACK;

const uint8_t bel = BEL;

const uint8_t bs = BS;

const uint8_t ht = HT;

const uint8_t lf = LF;

const uint8_t cr = CR;

const uint8_t nak = NAK;

const uint8_t esc = ESC;

const uint8_t del = DEL;

uint8_t outgoing_buffer[27];

uint8_t outgoing_length = 1;

uint8_t incoming_buffer[28];

uint8_t incoming_length = 27;

uint8_t char_buffer = NUL;

const uint8_t char_length = 1;

uint8_t welcome_buffer[] = {" \r\nWelcome to BlackChat.\r\n"};

uint8_t message_buffer[] = {" Message: "};

uint8_t received_buffer[] = {" Received: "};

uint8_t ready_to_tx = false;

uint8_t started = false;

void clock_setup();

void usci_uart_setup();

//void read_line(uint8_t* str);

void send_line(uint8_t* str, uint8_t length);

void send_char(uint8_t character);

//uint8_t get_length(uint8_t* str);

//void null_terminate(uint8_t* str, uint8_t length);

void process_message();

int main(void) {
	WDTCTL = WDTPW | WDTHOLD;			// Stop watchdog timer

	P5DIR |= BIT0 + BIT1;
	P5OUT |= BIT0 + BIT1;

	clock_setup();

	usci_uart_setup();

	vw_set_ptt_inverted(true);

	vw_setup(1000);

	__bis_SR_register(GIE);

	outgoing_buffer[0] = 0; // outgoing message number

	incoming_buffer[27] = 255;

	while( !started );

	send_line(welcome_buffer, 28);

	send_line(message_buffer, 10);

	vw_rx_start();

	while(1) {
		if( vw_get_message(incoming_buffer, &incoming_length) ) {
			if( incoming_length != 1 )
				process_message();
		} else if( ready_to_tx == true ) {
			//vw_rx_stop();
			P5OUT &= ~BIT0;
			uint8_t good;
			do {
				do {
					vw_rx_stop();
					vw_send(outgoing_buffer, outgoing_length);
					vw_wait_tx();
					vw_rx_start();
				} while( !vw_wait_rx_max(400) );
				good = vw_get_message(incoming_buffer, &incoming_length);
			} while ( incoming_length == 1 && !good );
			// have good one char message, bad >1 char message or good >1 char message
			if( incoming_length == 1 ) {
				if( incoming_buffer[0] == ACK ) {
					ready_to_tx = false;
					outgoing_length = 1;
					send_line(message_buffer, 10);
					outgoing_buffer[0] += 1;
					P5OUT ^= BIT0;
					vw_rx_start();
				} else
					process_message();
			} else if( good )
				process_message();
			else
				vw_wait_rx_max(500);
			incoming_length = 27;
		}
	}
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
	//__bis_SR_register(GIE);

	char_buffer = UCA0RXBUF;

	if( started == false && char_buffer == HT )
		started = true;
	else if( started == true && ready_to_tx == false ) {
		if( char_buffer == CR || char_buffer == LF ) {
			if( outgoing_length != 1 ) {
				ready_to_tx = true;
				send_char(CR);
				send_char(LF);
			} else
				send_char(BEL);
		} else if( char_buffer == BS ) {
			if( outgoing_length == 1 ) {
				send_char(BEL);
			} else {
				--outgoing_length;
				send_char(BS);
				send_char(' ');
				send_char(BS);
			}
		} else if( outgoing_length == 27 ) {
			send_char(BEL);
		} else {
			outgoing_buffer[outgoing_length] = char_buffer;
			++outgoing_length;
			send_char(char_buffer);
		}
	} else {
		send_char(BEL);
	}
	/*

	uint8_t* pstr = outgoing_buffer;

	do {
		while( !(IFG2&UCA0RXIFG) );
		*pstr = UCA0RXBUF;
	} while( *pstr++ != '\n' );

	*pstr = '\0';

	ready_to_tx = true;*/
}

void clock_setup() {
	BCSCTL1 |= XTS + DIVA_2;                    // Activate XT high freq xtal
	BCSCTL3 |= LFXT1S_2;               // 3 � 16MHz crystal or resonator

	uint16_t i;

	do {
		IFG1 &= ~OFIFG;                         // Clear OSCFault flag
		for (i = 0xFF; i > 0; i--);             // Time for flag to set
	} while (IFG1 & OFIFG);                     // OSCFault flag still set?

	BCSCTL2 |= SELM_3;                        // MCLK = XT HF XTAL (safe)
}

void usci_uart_setup() {
	P3SEL = 0x30;
	UCA0CTL1 |= UCSWRST;
	UCA0CTL1 |= UCSSEL_1;
	UCA0BR0 = 26;
	UCA0MCTL |= UCBRF_1 + UCOS16;
	UCA0CTL1 &= ~UCSWRST;
	IE2 |= UCA0RXIE;
}

/*void read_line(uint8_t* str) {
	uint8_t* pstr = str;

	while( !(IFG2&UCA0RXIFG) );
	do {
		while( !(IFG2&UCA0RXIFG) );
		*pstr = UCA0RXBUF;
	} while( *pstr++ != '\n' );

	*pstr = '\0';
}*/

void send_line(uint8_t* str, uint8_t length) {
	uint8_t i;
	for( i = 1; i != length; ++i ) {
		while( !(IFG2&UCA0TXIFG) );
		UCA0TXBUF = str[i];
	}
}

void send_char(uint8_t character) {
	while( !(IFG2&UCA0TXIFG) );
	UCA0TXBUF = character;
}

void process_message() {
	uint8_t i;
	vw_rx_stop();
	P5OUT &= ~BIT1;
	vw_send(&ack,1);
	if( incoming_buffer[0] != incoming_buffer[27] ) {
		send_char(CR);
		for( i = 50; i != 0; --i)
			send_char(' ');
		send_char(CR);
		send_line(received_buffer, 11);
		send_line(incoming_buffer, incoming_length);
		send_char(CR);
		send_char(LF);
		send_line(message_buffer, 10);
		send_line(outgoing_buffer, outgoing_length);
		incoming_buffer[27] = incoming_buffer[0];
	}
	incoming_length = 27;
	P5OUT ^= BIT1;
	vw_rx_start();
}

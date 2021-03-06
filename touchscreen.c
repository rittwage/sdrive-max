#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions
#include "touchscreen.h"
#include "tft.h"
#include "display.h"

u16 EEMEM MINX = 0xffff;
u16 EEMEM MINY = 0xffff;
u16 EEMEM MAXX = 0xffff;
u16 EEMEM MAXY = 0xffff;

#define TS_MINX eeprom_read_word(&MINX)
#define TS_MINY eeprom_read_word(&MINY)
#define TS_MAXX eeprom_read_word(&MAXX)
#define TS_MAXY eeprom_read_word(&MAXY)

extern unsigned int MAX_X;
extern unsigned int MAX_Y;
extern struct display tft;

int map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// "Ruhezustand" aktivieren
static void setIdling() {
 XM_DDR &= ~(1<<XM); XM_PORT |= (1<<XM);	// X- = Hi-Z
	      YM_PORT &= ~(1<<YM);	// Y- = L
 XP_DDR &= ~(1<<XP); XP_PORT &= ~(1<<XP);	// X+ = Z
	      YP_PORT &= ~(1<<YP);	// Y+ = L
 _delay_us(0.5);
}

// Ausgangszustand wiederherstellen
static void restorePorts() {
 XP_PORT |= (1<<XP);	// RS high
 //YM_PORT &=~(1<<YM);	// CS high oder low??
 YM_PORT |= (1<<YM);	// CS high
 XM_DDR |= (1<<XM);	// X- (D0): Ausgang
 YM_DDR |= (1<<YM);	// Y- (CS): Ausgang
 XP_DDR |= (1<<XP);	// X+ (RS): Ausgang
 YP_DDR |= (1<<YP);	// Y+ (D1): Ausgang
}

char isTouching() {
 cli();	//no interrupts during change of port directions!
 setIdling();
 char ret=!(XM_PIN & (1<<XM));	// Drück-Zustand einlesen (TRUE wenn LOW)
 restorePorts();
 sei();
 return ret;
}

uint16_t readTouch(uint8_t b) {
 //ADCSRA=0x9F;	// A/D-Wandler aktivieren mit Teiler 128 -> Taktfrequenz 125 kHz
 ADCSRA=0x87;	// no IRQ, A/D-Wandler aktivieren mit Teiler 128 -> Taktfrequenz 125 kHz
 if (b) {	// Y messen
  XM_DDR &=~(1<<XM); XM_PORT &=~(1<<XM);	// X- = Z
  YM_DDR |= (1<<YM); YM_PORT &=~(1<<YM);	// Y- = L
  XP_DDR &=~(1<<XP); XP_PORT &=~(1<<XP);	// X+ = Z
  YP_DDR |= (1<<YP); YP_PORT |= (1<<YP);	// Y+ = H
  ADMUX = (0x40 | XP);	// ADC2, sonst wie unten
 }else{		// X messen
  XM_DDR |= (1<<XM); XM_PORT &=~(1<<XM);	// X- = L
  YM_DDR &=~(1<<YM); YM_PORT &=~(1<<YM);	// Y- = Z
  XP_DDR |= (1<<XP); XP_PORT |= (1<<XP);	// X+ = H
  YP_DDR &=~(1<<YP); YP_PORT &=~(1<<YP);	// Y+ = Z
  ADMUX = (0x40 | YM);	// ADC3, Referenz 5 V, rechts ausgerichtet
 }
 _delay_us(0.5);
 ADCSRA |= (1<<ADSC);	// A/D-Wandler starten
 while (ADCSRA&(1<<ADSC));	// warten bis fertig
 uint16_t ret=ADC;
// restorePorts();
 return ret;
}

struct TSPoint p;

struct TSPoint getPoint () {
  cli();	//disable interrupts
  setIdling();
#if defined(HX8347G)
    p.x = map(readTouch(1), TS_MINX, TS_MAXX, 0, MAX_X);
    p.y = map(readTouch(0), TS_MINY, TS_MAXY, 0, MAX_Y);
#else
    p.x = map(readTouch(0), TS_MINX, TS_MAXX, 0, MAX_X);
    p.y = map(readTouch(1), TS_MINY, TS_MAXY, 0, MAX_Y);
#endif
  restorePorts();
  sei();
  return(p);
}

struct TSPoint getRawPoint () {
	cli();
	setIdling();

#if defined(HX8347G)
	p.x = readTouch(1);
	p.y = readTouch(0);
#else
	p.x = readTouch(0);
	p.y = readTouch(1);
#endif

	restorePorts();
	sei();
	return(p);
}

/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <screen.h>
#include <list.h>

#include <sched.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

char SHIFT_pressed = 0;

char char_map[] =
{
      '\0','\0','1','2','3','4','5','6',
      '7','8','9','0','\'','�','\0','\1',             // \1 = TAB
      'q','w','e','r','t','y','u','i',
      'o','p','`','+','\0','\0','a','s',
      'd','f','g','h','j','k','l','�',
      '\0','�','\2','�','z','x','c','v',              // \2 = SHIFT
      'b','n','m',',','.','-','\0','*',
      '\0','\0','\0','\0','\0','\0','\0','\0',
      '\0','\0','\0','\0','\0','\0','\0','7',
      '8','9','-','4','5','6','+','1',
      '2','3','0','\0','\0','\0','<','\0',
      '\0','\0','\0','\0','\0','\0','\0','\0',
      '\0','\0'
};

int zeos_ticks = 0;

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks ++;
  
  schedule();
}

void keyboard_routine()
{
  unsigned char keyboardValue, characterCode, character, keyPressed;
  keyboardValue = inb(0x60);
  characterCode = keyboardValue & 0x7F;    // lower 7 bits
  character = char_map[characterCode];
  keyPressed = (keyboardValue & 0x80) == 0;       // bit 7 to see if the key is pressed or released
 
  switch (character) {        
    case '\1' : 
      if (!keyPressed) {}
      else {
        if (SHIFT_pressed) {      // SHIFT + TAB
          previous_screen();
        }
        else {                    // TAB
          next_screen();
        }
      }
      break;
    case '\2' : 
      if (!keyPressed) SHIFT_pressed = 0;
      else SHIFT_pressed = 1;
      break;
  }

}

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void clock_handler();
void keyboard_handler();
void system_call_handler();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);
  
  setSysenter();

  set_idt_reg(&idtR);
}


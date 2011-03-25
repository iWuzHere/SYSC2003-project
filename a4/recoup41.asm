; SYSC2003 Recoup 4.1
;
; Brendan MacDonell (100777952) and Imran Iqbal (100794182)

#include "DP256reg.asm"

                ORG     $1000   ; Data segment.


                ORG     $4000   ; Code segment.
                
                JSR	init
                ;Turn on all the LEDs
                LDD	#1
                PSHD
                LDD	#0
                JSR	setLED
                PULD
                JSR	delay
                
                LDD	#1
                PSHD
                LDD	#1
                JSR	setLED
                PULD
                JSR	delay
                
                LDD	#1
                PSHD
                LDD	#2
                JSR	setLED
                PULD
                JSR	delay
                
                LDD	#1
                PSHD
                LDD	#3
                JSR	setLED
                PULD
                JSR	delay
                
                ;Turn them all off now
                LDD	#0
                PSHD
                LDD	#0
                JSR	setLED
                PULD
                LDD	#0
                PSHD
                LDD	#1
                JSR	setLED
                PULD
                LDD	#0
                PSHD
                LDD	#2
                JSR	setLED
                PULD
                LDD	#0
                PSHD
                LDD	#3
                JSR	setLED
                PULD
                
                ;Test seven segment Display
                
                LDD	#9
testLoop:
		PSHD
		LDY	#1
		PSHY
		JSR	set7Segment
		PULY
		PULD
		JSR	delay
		DBNE	D,testLoop
		
		; Clear the 7 seg display
		LDY	#0
		PSHY
		JSR	set7Segment
		PULY
		
		BCLR	PTM, #$08
		
		BRA *

; void init(void)
;
; Initialize all hardware (LEDs and the 7-segment display) for
; recoup assignment 4.1.
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
init:
                PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.

                BSET    DDRK, #$0F      ; Enable output to the LEDs on PortK.
                BSET    PTM,  #$08      ; Set U7_EN high to enable the 7-seg.
                MOVB    #$0F, DDRT      ; Set the output on PT0..3 for the 7-seg.

                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
                RTS

; void delay(void)
;
; Delay for approximately a second.
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
delay:
        	PSHX				;2 cycles
		LDY	#5			;2 cycles
DLoop:
		LDX	#$ffff			;2 cycles
		JSR	D1MS			;4 cycles
		DEY				;1 cycle
		BNE	DLoop			;3 cycles/1 cycle
	
		LDX	#$4236			;2 cycles
		JSR	D1MS			;4 cycles

		PSHX				;2 cycles
		PULX				;2 cycles

		PULX				;2 cycles
		RTS				;5 cycles	
;This delays by 8X + 3, value in X be >= 1
D1MS:
		NOP				;1 cycle
		NOP				;1 cycle
		NOP				;1 cycle
		NOP				;1 cycle
		DEX				;1 cycle
		BNE	D1MS			;3 cycles/1 cycle
		RTS				;5 cycles
	
; void setLED(unsigned byte index, boolean on)
;
; Set the LED, as indicated by an index between 0 and 3 inclusive.
; 'on' indicates whether the LED should be turned on (1 = true)
; or off (0 = false).
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
index           EQU     3
ledIsOn         EQU     7

setLED:
		PSHD                    ; Save our arg[0]
                PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.
                
                ; Set up a mask to identify the LED to enable.
                LDAA    #1              ; Initialize the mask to 1.
                LDAB    index,X         ; Load the index into B.
                INCB			
                BRA     startLEDShift   ; Jump to the first loop test.

setLEDShift:    LSLA                    ; Shift the LED bit left.
startLEDShift:  DBNE    B, setLEDShift  ; Loop `index' times.

                TST     ledIsOn,X       ; Enable (1) or disable(0) the LED?
                BEQ     disableLED      ; Disable the LED if necessary.
                ORAA    PortK           ; OR the mask with PortK.
                BRA     doSetLED        ; Finish the subroutine.
disableLED:     COMA                    ; Negate the mask.
                ANDA    PortK           ; AND off the index with the mask.

doSetLED:       STAA    PortK           ; Update Port K.

                
                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
                LEAS	2,SP
                RTS

; TODO: WTF DOES THE ON PARAMETER DO?
;
; void set7Segment(char number, boolean on)
;
; Display a number on the 7 segment display. 'number' is the number
; to display on the 7 segment, while 'on' indicates ?????
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
number          EQU     3
Seg7IsOn        EQU     7

set7Segment:
                PSHD                    ; Save arg[0]
		PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.
                
                TST     Seg7IsOn,X      ; Enable (1) or disable(0) the 7-segment display.
		BNE	Continue7Seg    ; Skip clearing if the 7-seg is on.
		MOVB	#$0F, PTT	; Set the BCD value to be invalid (turn it off.)
		BRA	Done7Seg	; End the routine.
Continue7Seg:		
                LDAA    number,X        ; Load the number.
                ANDA    #$0F            ; Mask off any excess bits.
                BCLR    PTT,#$0F        ; Mask off the BCD value on PTT
                ORAA    PTT             ; OR the Port value with the number.
                STAA    PTT             ; Store the value back to PTT.

Done7Seg:               
                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
                LEAS 	2,SP
                RTS

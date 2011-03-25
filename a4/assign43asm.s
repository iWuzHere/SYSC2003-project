        .include "DP256Reg.s"
		.area text

; void delay(void)
;
; Delay for approximately a second.
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
_delay::
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

_DELAY50M::
; Generate a 50 ms delay
DELAY50M:
          pshx
          ldx  #49998      ; delay 50,000 usecs,
          jsr  D1MS     ; call usec delay
          pulx
          rts
;
;

; Generate a 10 ms delay
DELAY10M:                            ; jsr=4cyles
          pshx             ; 2 cycles ,save x
          ldx  #9998       ; 2 cycles,delay 9998 usec + 2 for this routine
          jsr  D1MS     ; call usec delay, this delay offset in sub
          pulx             ; 3 cycles restore x
          rts              ; 5 cycles
;
;
; Generate a 1 ms delay
DELAY1MS:
                           ; jsr=4cyles
          pshx             ; 2 cycles ,save x
          ldx  #998       ; 2 cycles,delay 9998 usec + 2 for this routine
          jsr  D1MS       ; call usec delay, this delay offset in sub
          pulx             ; 3 cycles restore x
          rts              ; 5 cycles
		  
; void LoadStrLCD( char *s )
; Loads characters into the LCD until it hits a null terminate
_LoadStrLCD::
			 ; pointer is recieved in D
			 PSHX
			 TFR D,X
LSLStart:
			 ldaa 0, x
			 tsta
			 beq LSLEnd
			 jsr LCD2PP_Data
			 jsr DELAY50M
			 inx
			 bra LSLStart
LSLEnd:
			 PULX
			 RTS

; void moveLCDCursor( byte address )
; changes position in DDRAM to address
_moveLCDCursor::
			PSHX
			TBA
			jsr LD2PP_Instruction
			jsr DELAY10M
			PULX
			RTS

; void clearLCD(void)
; Clears the LCD.
_clearLCD::	
			ldaa     #$01		; Clear display = 00000001
			jsr      LD2PP_Instruction           
			jsr      DELAY10M
			jsr	   	 DELAY10M
			RTS

; void LCD2PP_Init( void )
; 
; Sets up the LCD
_LCD2PP_Init::	; Note : Use 4-bit init sequence (not 8-bit)  Page 3 LCD_spec.pdf
		; Bottom table contains sequence of instructions
		; Each row in the table represents one WRITE to the LCD instruction register (via Port P)
		;	First instruction involves only a 4-bit instruction (one WRITE)
		;	Following instructions involve 8 bit instruction, therefore
		;		2 * 4-bit writes
	; "System init"
	; Setup Port T for output
          movb #$CF,DDRT        ; setup port T
          movb #$00,PTT         ; all low
	; Disable SPI AND setup SPI1 as four output bits
	  bset  DDRP,#$0F   	; set P0-3 as outputs
          bclr  SPI1CR1,#$40  	; Disable SP by turning SPI1 off

          movb #$FE,DDRM        ; set PM1-7 as outputs
          movb #$00,PTM         ; D.P.(PM2) = Off, U7_EN(PM3)= low,
                                ; SS0*(PM4), SS1*(PM5), SS2*(PM6) = Low
                                ; Heat(PM7) = Off

          bclr    PTT,#$0E  ; select lcd commands Cs=0 En=0

          jsr      DELAY50M
          ldaa     #$02		; Set to 4-bit operation (0010)
          jsr      LCD2PP_4     ; This first instruction is only 4 bits long!!!  Rest are 8 bits.  
          jsr      DELAY50M

          ldaa     #$2c		; Function Set = 001(D/L)NF** where D/L = 0(4-bit) N=1(2-lines) F=0(font=5x7 dots)
          jsr      LD2PP_Instruction         
          jsr      DELAY10M         

          ldaa      #$0e	; Display On/off Control = 00001DCB where D=1(display on) C=1(cursor on) B=0 (blink off)
          jsr      LD2PP_Instruction          
          jsr      DELAY10M          
                
          ldaa     #$01		; Clear display = 00000001
          jsr      LD2PP_Instruction           
          jsr      DELAY10M
		  jsr	   DELAY10M 
		           
          ldaa     #$80		; DDRAM Address Set = 01xxxxxx where xxxxxx = address
          jsr      LD2PP_Instruction
          jsr      DELAY10M        

; Reset Lcd states to rest
         bclr    PTT,#$0E ; turn all signals off on lcd
          rts
	

;
;-----------------------------------------------
; Lcd Routines
;
; Write a byte to the LCD Data Register
LCD2PP_Data::
      bset  PTT,#$04     ; select lcd data buffer RS=1
      jsr   LCD_W_8        ; write byte
      rts

; Write a byte to the LCD Instruction Register (leaves LCD in Data mode)
LD2PP_Instruction:
        bclr   PTT,#$04        ; select lcd command buffer
        jsr    LCD_W_8           ; wait
        bset   PTT,#$04        ; select data buffer
        rts

LCD2PP_4:			; Destroys a and b
	 bset  	PTS,#$80	; set U21_EN high so that latch becomes transparent
         jsr      DELAY1MS      ; delay     
         ldab     PTP              ; Port P
         andb     #$f0             ; get only bits 4 - 7
         anda     #$0f             ; get data
         aba
         staa     PTP              ; save data 
	; For LCD's write cycle, Enable must pulse high and then low (for specified time)
         bclr     PTT,#$08       ; enable low
         jsr      DELAY1MS         ; delay for LCD
         bset     PTT,#$08       ; latch data
         jsr      DELAY1MS         ; delay for LCD 
         bclr     PTT,#$08           ; enable low
         jsr      DELAY1MS
	 bclr  PTS,#$80    ; set U21_EN low to isolate LCD from parallel control (outputs are latched)
         rts
;
;
; Lcd Write 8 bit Data , lower 4 bits first in acc A   (Destroys A)
LCD_W_8:					
         psha                     ; save a 
         lsra                     ; shift upper 4 bits to lower
         lsra
         lsra
         lsra
         jsr      LCD2PP_4        ; write upper 4 bits to lcd
         pula
         jsr      LCD2PP_4         ; write lower 4 bits to lcd
         rts

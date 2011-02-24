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
		
;*********************************************************************************
_putcharSc0:: 
; int putcharSc0(char c)
;Accepts: Character for output in B
;Returns: The character printed.
;Given a character, polls 'Transmit Data Register Empty Flag'(TDRE) of SC0SR1
;until it goes high.  Then places new character in SC0DRL data register for output.
;*********************************************************************************
	;If SC0SR1 Bit 7 is high then we may write another byte for output
	brclr SC0SR1, #$80, _putcharSc0
	;Write next character to the Data Register
	stab SC0DRL
	rts
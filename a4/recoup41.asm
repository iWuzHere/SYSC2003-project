; SYSC2003 Recoup 4.1
;
; Brendan MacDonell (100777952) and Imran Iqbal (100xxxxxx)

#include "DP256reg.asm"

                ORG     $1000   ; Data segment.


                ORG     $4000   ; Code segment.

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
                PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.
                PSHA                    ; Store used registers.

delayLoop:      LDAA    #100            ; Set the counter to loop 100 times.
                JSR     delay10ms
                DBNE    A, delayLoop

                PULA                    ; Restore clobbered registers.
                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
                RTS

; void delay10ms(void)
;
; Delay for 10 milliseconds.
delay10ms:
                PSHD                    ; Store D so we can use it as a counter.
                LDD     #10000          ; Set the counter to loop 10000 times.

delay10Loop:    CPD     #0              ; Perform useless operations to delay.
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                CPD     #0
                NOP
                DBNE    D, delay10Loop

                PULD                    ; Restore used registers.
                RTS                     ; Return from the subroutine.

; void setLED(unsigned byte index, boolean on)
;
; Set the LED, as indicated by an index between 0 and 3 inclusive.
; 'on' indicates whether the LED should be turned on (1 = true)
; or off (0 = false).
;
; NOTE: This subroutine uses C-style calling conventions. Pass
;       all of the parameters on the stack.
index           EQU     4
ledIsOn         EQU     5

setLED:
                PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.
                PSHD                    ; Store used registers.

                ; Set up a mask to identify the LED to enable.
                LDAA    #1              ; Initialize the mask to 1.
                LDAB    index,SP        ; Load the index into B.
                BRA     startLEDShift   ; Jump to the first loop test.

setLEDShift:    LSLA                    ; Shift the LED bit left.
startLEDShift:  DBNE    B, setLEDShift  ; Loop `index' times.

                TST     ledIsOn,SP      ; Enable (1) or disable(0) the LED?
                BEQ     disableLED      ; Disable the LED if necessary.
                ORAA    PortK           ; OR the mask with PortK.
                BRA     doSetLED        ; Finish the subroutine.
disableLED:     COMA                    ; Negate the mask.
                ANDA    PortK           ; AND off the index with the mask.

doSetLED:       STAA    PortK           ; Update Port K.

                PULD                    ; Restore used registers.
                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
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
number          EQU     4
Seg7IsOn        EQU     5

set7Segment:
                PSHX                    ; Store X.
                TFR     SP, X           ; Use X as the base pointer.
                PSHD                    ; Push used registers.

                LDAA    number,SP       ; Load the number.
                ANDA    #$0F            ; Mask off any excess bits.
                BCLR    PTT,#$F0        ; Mask off the BCD value on PTT
                ORAA    PTT             ; OR the Port value with the number.
                STAA    PTT             ; Store the value back to PTT.

                PULD                    ; Restore used registers.
                LEAS    ,X              ; Point SP to the base of the frame.
                PULX                    ; Restore X.
                RTS

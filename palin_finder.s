.global _start

//This program checks if input is a palindrome. It is case-insensitive and ignores spaces.
//If input is a palindrome: 5 rightmost LEDs lights up, and JTAG UART output is "Palindrome detected"
//If input is not a palindrome: 5 leftmost LEDs light up, and JTAG UART output is "Not a palindrome"
//Run on ARMv7 DE1-SoC using cpulator.

//Main
_start:
	BL check_input //Finds the length of the input-string
	BL cap_input //Converts input to uppercase
	BL check_palindrom //Checks if input is a palindrome, ignoring spaces
	
	b _exit //Stops the program
	

//Finds the length of the string, and stores it to R12 register
check_input:
	LDR R0,=input          //Loads input to R0
	MOV R1,#0              //Counter set to 0
check_loop:
	LDRB R2,[R0],#1        //Loads one byte from input-string at the time to R2
	
	CMP R2,#0			   
	BEQ check_done         //Terminates at NULL
	ADD R1, R1, #1		   //Increment counter R1
	
	B check_loop			   //Repeat loop for next character in input
	
check_done: 
	MOV R12, R1		   //Save length to R12 register
	BX LR			   //Branch back to main
	

//Capitalizes all characters in input
cap_input:
	LDR R0,=input       //Load input to R0
	MOV R1,#0           //Initialize pointer/index
cap_loop: 
	CMP R1, R12         
	BEQ cap_done        //Branch back to main if the whole string has been capitalized
	LDRB R3, [R0,R1]    //Load byte(character) indexed by R1 to R3
	CMP R3, #'Z'        
	BGT lowercase       //Branch if character is lowercase (Z divides upper and lower case in ascii)
	ADD R1, R1, #1      //Increment pointer
	B cap_loop          //Repeat loop for every character
lowercase: 
	SUB R3,R3,#32       //Subtract 32 to convert to uppercase
	STRB R3, [R0, R1]   //Store capitalized character back to string
	B cap_loop          
cap_done:
	BX LR               //Branch back to main

	
//Checks if string is palindrome, ignoring spaces
check_palindrom:
	LDR R0,=input                                                   //Load mono-cased input to R0
	MOV R1,#0                                                       //Left index
	MOV R2,R12                                                      
	SUB R2, R2,#1                                                   //Right index
	CMP R2,#0                                                       //Check if string is too short (Minimum length 2)
	BLE is_no_palindrom                                             
pal_loop:           
	LDRB R3, [R0,R1]                                                //Load R1th byte to R3
	LDRB R4, [R0,R2]                                                //Load R2th byte to R3
	
	//Ignore space by skipping
	CMP R3, #32         
	BEQ skip_space_begin
	CMP R4, #32
	BEQ skip_space_end
	
	CMP R3, R4
  	BNE is_no_palindrom                                         //If characters don't match, it's not a palindrome
	
	CMP R1, R2
	BGE is_palindrom                                            //If R1 greater or equal to R2, whole string has been checked and is palindrome
	
	ADD R1, R1, #1                                              //Increment index1
	SUB R2, R2, #1                                              //Decrement index2
	
	B pal_loop                                                 

skip_space_begin:
	ADD R1,R1,#1
	B pal_loop
	

skip_space_end:
	SUB R2,R2,#1
	B pal_loop


is_palindrom:
	//LED
	MOV R5, #0x1F
	LDR R6, =0xFF200000
	STRB R5, [R6]
	
	//JTAG OUTPUT
	LDR R0,=0xFF201000
	LDR R1,=palindrom_output
	B jtag_loop
	
is_no_palindrom:
	//LED (10 leds, need two addresses to be addressed)
	MOV R5, #0xE0
	LDR R6, =0xFF200000
	STRB R5, [R6]
	ADD R6,#1
	MOV R5,#0x3
	STRB R5, [R6]
	
	//JTAG OUTPUT
	LDR R0,=0xFF201000
	LDR R1,=not_palindrom_output
	B jtag_loop

jtag_loop:
	LDRB R2,[R1],#1
	CMP R2, #46
	BEQ _exit                   //End program on output string character '.'
	STR R2, [R0]
	B jtag_loop
	
_exit:
	// Branch here for exit
	b .
	
.data
.align
	input: .asciz "Grav ned den varg"
	palindrom_output: .asciz "Palindrome detected."
	not_palindrom_output: .asciz "Not a palindrome."
.end
	
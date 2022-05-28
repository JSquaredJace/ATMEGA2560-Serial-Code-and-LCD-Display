/*
 * Lab05 - Serial Code and LCD Display
 * Author : Jace Johnson, Dr. Randy Hoover
 * Created: 3/5/2020 4:42:51 PM
 *	  Rev 1 3/5/2020
 * Description:	ATMEGA2560 communicates with computer over SSH and prints
 *		strings and characters to a connected LCD screen. The
 *		controller sends a prompt to the computer to send a string.
 *		The received string is printed to the next line of the LCD
 *		screen, then is echoed back to the computer. A string that is
 *		longer than the length of the screen (16 chars) is split into
 *		two lines on the LCD screen. A string that is longer than both
 *		lines of the LCD screen combined (32 chars) is not printed or
 *		echoed, and an error message is printed on the LCD screen.
 *		Sending the ^C (Ctrl + c) command causes the controller to
 *		clear the screen. This code also includes commented hard code
 *		for printing individual characters to each line of the LCD as
 *		well as commented hard code for printing a string to the top
 *		line of the LCD.
 *
 * Hardware:	ATMega 2560 operating at 16 MHz
 *		LCD screen
 *		10 KOhm potentiometer (POT)
 *		jumper wires
 * Configuration shown below
 *
 * ATMega 2560	       LCD Screen
 *  PORT   pin             pin      	 
 * -----------         ----------
 * |	     |	  GND--|K	|
 * |	     |	   5V--|A	|
 * | A7    29|---------|D7	|
 * | A6    28|---------|D6	|
 * | A5    27|---------|D5	|
 * | A4    26|---------|D4	|
 * |         |         |	|
 * | B1    52|---------|E	|
 * |	     |	  GND--|RW	|   10KOhm Potentiometer
 * | B0    53|---------|RS	|	    POT
 * |         |         |	|	-----------
 * |	     |	       |      V0|-------|W	5V|--5V
 * |	   5V|----5V---|VDD	|	|      GND|--GND
 * |	  GND|---GND---|VSS	|	-----------
 * -----------	       ----------
 */

#define F_CPU 16000000

#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#define USART_BAUDRATE 57600
#define BAUD_PRESCALE F_CPU / (USART_BAUDRATE * 16UL) - 1

#define MAX_INPUT 40

//  Helpful LCD control defines  //
#define LCD_Reset              0b00110000          // reset the LCD to put in 4-bit mode //
#define LCD_4bit_enable        0b00100000          // 4-bit data - can't set the line display or fonts until this is set  //
#define LCD_4bit_mode          0b00101000          // 2-line display, 5 x 8 font  //
#define LCD_4bit_displayOFF    0b00001000          // set display off  //
#define LCD_4bit_displayON     0b00001100          // set display on - no blink //
#define LCD_4bit_displayON_Bl  0b00001101          // set display on - with blink //
#define LCD_4bit_displayCLEAR  0b00000001          // replace all chars with "space"  //
#define LCD_4bit_entryMODE     0b00000110          // set curser to write/read from left -> right  //
#define LCD_4bit_cursorSET     0b10000000          // set cursor position


//  For two line mode  //
#define LineOneStart 0x00
#define LineTwoStart 0x40 //  must set DDRAM address in LCD controller for line two  //

//  Pin definitions for PORTB control lines  //
#define LCD_EnablePin 1
#define LCD_RegisterSelectPin 0

//prototypes for functions provided by Dr. Randy Hoover
void LCD_init(void);
void LCD_E_RS_init(void);
void LCD_write_4bits(uint8_t);
void LCD_EnablePulse(void);
void LCD_write_instruction(uint8_t);
void LCD_write_char(char);

// Timer 0 initialization prototypes //
void initializeTimers();

// USART0 initialization prototypes //
void InitUSART0();
int uart_putchar0(char c, FILE* stream);
int uart_getchar0(void);

//Prototypes for functions provided by Jace Johnson
void LCD_write_str(char arr[MAX_INPUT], int* LCDLine);
void LCD_clear_line(int* line);
int getInput0(char input[MAX_INPUT]);
int checkInput(char input[MAX_INPUT]);
int checkClearInput(char input[MAX_INPUT]);
int checkInputLen(char input[MAX_INPUT]);
void outputLine(char input[MAX_INPUT], int* LCDLine, int returnStatus);
void printErr(int* LCDLine);
void outputHardLine(char input[MAX_INPUT]);
void printHardErr();
void outputHardChars(char char1, char char2);

static FILE USART0_OUT = FDEV_SETUP_STREAM(uart_putchar0, NULL, 
	_FDEV_SETUP_WRITE);
static FILE USART0_IN = FDEV_SETUP_STREAM(NULL, uart_getchar0, 
	_FDEV_SETUP_READ);

int main(void)
{
	DDRB = 0x23;	//setup pins in ports A and B as outputs
	DDRA = 0xF0;
	
	//Initialize the LCD for 4-bit mode, two lines, and 5 x 8 dots
	//Inits found on Page 26 of datasheet and Table 7 for function set 
	//instructions
	LCD_init();
	
	//initialize timer 0 to toggle PORTB pin 0x20 for LCD screen
	initializeTimers();	
	//initialize USART 0 for transmit and recieve
	InitUSART0();
    
	int LCDLine = 0;
	int retStat = 0;
	char line[MAX_INPUT] = "this is fun";
	
	//print single character on each line of LCD
	/*
	char Char1 = 'W';		//char for line 1
	char Char2 = 'J';		//char for line 2
	outputHardChars(Char1, Char2);	//print chars to screen
	while(1){}			//do nothing
	*/
	
	//print single line to LCD
	/*
	outputHardLine(line);	//output line
	while(1){}   		//do nothing
	*/
	
	//serial communication controlled LCD printing
	while(1)
	{	
		//prompt user
		fprintf(&USART0_OUT, "Enter a string or command: ");
		//get line from serial port
		retStat = getInput0(line);
		//check line and output to screen and serial port
		outputLine(line, &LCDLine, retStat);
		//delay half a second
		_delay_ms(500);
	}
	return 1;
}

/*
 * ISR:  TIMER0_OVF_vect
 *  Interrupt for timer0 overflow. Toggles bit 6 of PORTB every 500 ms.
 *
 *  returns:	none
 */
ISR(TIMER0_OVF_vect) {
	PORTB ^= 0x20;	//toggle PORTB pin 6
	//reset timer for 500 ms
   	TCNT0 = (unsigned int)65536 - (unsigned int)(0.5 * 16000000.0 / 256.0); 
   	return;
}

//  Important notes in sequence from page 26 in the KS0066U datasheet - initialize the LCD in 4-bit two line mode //
//  LCD is initially set to 8-bit mode - we need to reset the LCD controller to 4-bit mode before we can set anyting else //
void LCD_init(void)
{
    //  Wait for power up - more than 30ms for vdd to rise to 4.5V //
    _delay_ms(100);
    
    //  Note that we need to reset the controller to enable 4-bit mode //
    LCD_E_RS_init();  //  Set the E and RS pins active low for each LCD reset  //
    
    //  Reset and wait for activation  //
    LCD_write_4bits(LCD_Reset);
    _delay_ms(10);
    
    //  Now we can set the LCD to 4-bit mode  //
    LCD_write_4bits(LCD_4bit_enable);
    _delay_us(80);  //  delay must be > 39us  //
    
    
    
    ////////////////  system reset is complete - set up LCD modes  ////////////////////
    //  At this point we are operating in 4-bit mode
    //  (which means we have to send the high-nibble and low-nibble separate)
    //  and can now set the line numbers and font size
    //  Notice:  we use the "LCD_wirte_4bits() when in 8-bit mode and the LCD_instruction() (this just
    //  makes use of two calls to the LCD_write_4bits() function )
    //  once we're in 4-bit mode.  The set of instructions are found in Table 7 of the datasheet.  //
    LCD_write_instruction(LCD_4bit_mode);
    _delay_us(80);  //  delay must be > 39us  //
    
    //  From page 26 (and Table 7) in the datasheet we need to:
    //  display = off, display = clear, and entry mode = set //
    LCD_write_instruction(LCD_4bit_displayOFF);
    _delay_us(80);  //  delay must be > 39us  //
    
    LCD_write_instruction(LCD_4bit_displayCLEAR);
    _delay_ms(80);  //  delay must be > 1.53ms  //
    
    LCD_write_instruction(LCD_4bit_entryMODE);
    _delay_us(80);  //  delay must be > 39us  //
    
    //  The LCD should now be initialized to operate in 4-bit mode, 2 lines, 5 x 8 dot fonstsize  //
    //  Need to turn the display back on for use  //
    LCD_write_instruction(LCD_4bit_displayON);
    _delay_us(80);  //  delay must be > 39us  //

}

void LCD_E_RS_init(void)
{
    //  Set up the E and RS lines to active low for the reset function  //
    PORTB &= ~(1<<LCD_EnablePin);
    PORTB &= ~(1<<LCD_RegisterSelectPin);
}

//  Send a byte of Data to the LCD module  //
void LCD_write_4bits(uint8_t Data)
{
    //  We are only interested in sending the data to the upper 4 bits of PORTA //
    PORTA &= 0b00001111;  //  Ensure the upper nybble of PORTA is cleared  //
    PORTA |= Data;  // Write the data to the data lines on PORTA  //
    
    //  The data is now sitting on the upper nybble of PORTA - need to pulse enable to send it //
    LCD_EnablePulse();  //  Pulse the enable to write/read the data  //
}

//  Write an instruction in 4-bit mode - need to send the upper nybble first and then the lower nybble  //
void LCD_write_instruction(uint8_t Instruction)
{
    //  ensure RS is low  //
    //PORTB &= ~(1<<LCD_RegisterSelectPin);
    LCD_E_RS_init();  //  Set the E and RS pins active low for each LCD reset  //
    
    LCD_write_4bits(Instruction);  //  write the high nybble first  //
    LCD_write_4bits(Instruction<<4);  //  write the low nybble  //
    
    
}

//  Pulse the Enable pin on the LCD controller to write/read the data lines - should be at least 230ns pulse width //
void LCD_EnablePulse(void)
{
    //  Set the enable bit low -> high -> low  //
    //PORTB &= ~(1<<LCD_EnablePin); // Set enable low //
    //_delay_us(1);  //  wait to ensure the pin is low  //
    PORTB |= (1<<LCD_EnablePin);  //  Set enable high  //
    _delay_us(1);  //  wait to ensure the pin is high  //
    PORTB &= ~(1<<LCD_EnablePin); // Set enable low //
    _delay_us(1);  //  wait to ensure the pin is low  //
}

//  write a character to the display  //
void LCD_write_char(char Data)
{
    //  Set up the E and RS lines for data writing  //
    PORTB |= (1<<LCD_RegisterSelectPin);  //  Ensure RS pin is set high //
    PORTB &= ~(1<<LCD_EnablePin);  //  Ensure the enable pin is low  //
    LCD_write_4bits(Data);  //  write the upper nybble  //
    LCD_write_4bits(Data<<4);  //  write the lower nybble  //
    _delay_us(80);  //  need to wait > 43us  //
    
}

/*
 * Function:	LCD_write_str
 *  Writes the input string to the input line on the LCD screen. wraps line if
 *  it is too large for one line. Does not check if the string is too large 
 *  for two lines and will continue line wrapping. Checking if a string is too
 *  large for two LCD lines will be handled outside of this function.
 *
 *  arr		char[]	string to be written to LCD screen
 *  line	int*	LCD screen line to be cleared
 *
 *  returns:  none
 */
void LCD_write_str(char arr[MAX_INPUT], int* LCDLine){
	int i = 0;	//array index counter
	int count = 0;	//LCD line wrapping counter
	
	//set line to write
	if(*LCDLine == 0){	//write to line 1
		LCD_write_instruction(LCD_4bit_cursorSET | LineOneStart);
	}
	else{			//write to line 2
		LCD_write_instruction(LCD_4bit_cursorSET | LineTwoStart);
	}
	_delay_us(80);		//short delay for LCD to update
	
	//loop to write chars to line until null terminator is encountered
	while(arr[i] != '\0'){
		LCD_write_char(arr[i]);	//write current char
		i++;			//increment index counter for array
		count++;		//increment line wrapping counter
		
		if(arr[i] != '\0'){
			//check if line needs to be wrapped
			if(count > 15){
				count = 0;		//reset line wrapping counter
				*LCDLine ^= 0x01;	//update current LCD line
				
							//change LCD line
				if(*LCDLine == 0){	//select LCD line 1
					LCD_write_instruction(LCD_4bit_cursorSET | LineOneStart);
				}
				else{			//select LDC line 2
					LCD_write_instruction(LCD_4bit_cursorSET | LineTwoStart);
				}
				_delay_us(80);		//short delay for LCD to update
			}
		}
	}
	*LCDLine ^= 0x01;	//change current LCD line
	return;
}

/*
 * Function:	LCD_clear_line
 *  Clears a line of the LCD screen based on the input pointer, then corrects 
 *  the value the pointer is pointing at to be the line that was cleared.
 *
 *  line	int*	LCD screen line to be cleared
 *
 *  returns:  none
 */
void LCD_clear_line(int* line){
	LCD_write_str("                ", line);	//write clear line to LCD
	*line ^= 0x01;					//go to line that was cleared
	return;
}
/*
 * Function:  InitTimer0
 *  Sets up timer0 to output a PWM signal to the OC0B port (G5) with the
 *  following settings:	8-bit phase corrected PWM
 *                      non inverted
 *                    	256 prescaler
 *
 *  returns:  none
 */

/*
 * Function:	initializeTimers
 *  Sets up timer0 to run with a 256 prescaler and enables timer 0 overflow
 *  interrupt.
 *
 *  returns:	none
 */
void initializeTimers(){
   	TCCR0A = 0x00;
   	TCCR0B |= (1<<CS12); 	//turn timer0 on with 256 prescaler
 
   	sei();    		//Enable global interrupts by setting global interrupt enable
                		//bit in SREG
   	
   	TIMSK0 = (1 << TOIE0);	//enable timer0 overflow interrupt(TOIE0)
   	TCNT0 = 65536 - 1;	//timer0 overflow in one clock cycle
   	
   	return;
}

/*
 * Function:	InitUSART0
 *  Sets up USART0 to use a baudrate equal to the global constant 
 *  USART_BAUDRATE, and use 8 bit character frames and async mode.
 *
 *  returns:	none
 */
void InitUSART0(){
	UCSR0B |= 0x18;	//Enable RX and TX
	UCSR0C |= 0x06;	//Use 8 bit character frames in async mode
	
	//set baud rate (upper 4 bits should be zero)
	UBRR0L = BAUD_PRESCALE;
	UBRR0H = (BAUD_PRESCALE >> 8);
	return;
}

/*
 * Function:	uart_putchar0
 *  Function to send chars to USART0. Used to setup USART0_OUT as a file
 *  pointer to the serial port so that data can be sent there.
 *
 *  c		char	character to be transmitted through the serial line
 *  stream	FILE*	pointer for USART0 output
 *
 *  returns:	0	successful function run
 */
int uart_putchar0(char c, FILE* stream){
	if(c == '\n') uart_putchar0('\r', stream);	//change newlines to return 
							//carriage
	loop_until_bit_is_set(UCSR0A, UDRE0);		//wait until transmitter is 
							//ready for new data
	UDR0 = c;					//transmit next character
	return 0;
}

/*
 * Function:	uart_getchar0
 *  Function to get chars from USART0. Used to setup USART0_IN as a file 
 *  pointer to the serial port so that data can be received from USART0.
 *
 *  returns:	int	value recieved from serial communication port, USART0
 */
int uart_getchar0(void){
	uint8_t temp;
	while(!(UCSR0A & (1 << RXC0)));	//wait for serial value to enter UDR
	temp = UDR0;			//get value from UDR and return it
	return temp;
}


/*
 * Function:	getInput0
 *  Gets line from USART0 and stores in input array, then returns the number
 *  of characters stored in the input array.
 *
 *  input	char[]	array to be filled by string from USART0
 *
 *  returns:	int	number of chars in input array
 */
int getInput0(char input[MAX_INPUT]){
	int inputVal;
	inputVal = fscanf(&USART0_IN, "%[^\r]s ", input);	//get input line
	fgetc(&USART0_IN);					//clear input buffer
	
	return inputVal;
}

/*
 * Function:	checkInput
 *  Checks input string to see if Ctrl+C was pressed or if input is too long.
 *  Returns a status value depending on if either of these are true
 *
 *  input	char[]	string to be checked
 *
 *  returns:	0	status is normal
 *		1	status is clear screen (Ctrl+C is entered)
 *		2	status is input line is too long
 */
int checkInput(char input[MAX_INPUT]){
	int status = 0;
	
	status = checkClearInput(input);//check if Ctrl+C is entered
	if(status != 0){		//return 1 if true
		return status;
	}
	
	status = checkInputLen(input);	//check if input line is too long
					//set status to 2 if true
	
	return status;			//return status;
}

/*
 * Function:	checkClearInput
 *  Checks input string. If input string contains only Ctrl+C, then the LCD 
 *  screen is cleared. A status value is returned.
 *
 *  input	char[]	string to be checked
 *
 *  returns:	0	screen was not cleared (Ctrl+C is not entered)
 *		1	screen is cleared (Ctrl+C is entered)
 */
int checkClearInput(char input[MAX_INPUT]){
	int temp = 0;
	
	if(strcmp(input, "\x03") == 0){	//if Ctrl+C is entered, clear the screen
		LCD_clear_line(&temp);	//clear line 1
		temp = 1;
		LCD_clear_line(&temp);	//clear line 2
		return 1;		//return clear line status
	}
	return 0;			//Ctrl+C was not entered
}

/*
 * Function:	checkInputLen
 *  Checks input string. If input string is too long to be outputted to both 
 *  lines of the LCD screen, 2 is returned, else 0 is returned
 *
 *  input	char[]	string to be checked
 *
 *  returns:	0	string will fit on LCD screen
 *		2	string will not fit on LCD screen
 */
int checkInputLen(char input[MAX_INPUT]){
	for(int i = 0; i < 33; i++){	//for loop to check for end of string
		if(input[i] == '\0'){	//if end of string is encountered, return 0
			return 0;
		}
	}
	//if string is longer than two lines of the LCD screen, return 2
	return 2;
}

/*
 * Function:	outputLine
 *  Check the status of the input line and output accordingly. If status is 0
 *  (normal status), write the input string to the input line of the LCD 
 *  screen. If status is 1 (clear screen), output nothing. If status is 2 
 *  (string is too long), then output an error message. Also checks if return
 *  was pressed without any other input (input string is empty) and changes the
 *  LCD line without clearing if true.
 *
 *  input	char[]	string to be written to the LCD screen
 *  LCDLine	int*	line of the LCD screen to write the string to 
 *		(will wrap to the other line if too long for one 
 *		line)
 *  returnStatus	int	number of chars in input string (without null 
 *				terminator)
 *
 *  returns:	none
 */
void outputLine(char input[MAX_INPUT], int* LCDLine, int returnStatus){
	int status = 0;
	
	if(returnStatus == 0){		//change LCD line if return is pressed without a string
		strcpy(input, "");	//write nothing to line (without clearing)
		LCD_write_str(input, LCDLine);
		return;			//return from function
	}
	
	status = checkInput(input);	//get the ststus of the input string
	
	if(status == 0){		//if string is normal, output to LCD
		LCD_clear_line(LCDLine);	//clear LCD line and write string to line
		LCD_write_str(input, LCDLine);
		//return input line to USART0
		fprintf(&USART0_OUT, "Your str is: %s \n\r", input);
	}
	if(status == 2){		//if string is too long, print error message
		printErr(LCDLine);	//print error message and set LCDline to 0
	}
	return;
}

/*
 * Function:	printErr
 *  Write error message to LCD screen and set LCDLine to 0
 *
 *  LCDLine	int*	line of the LCD screen to write the string to
 *
 *  returns:	none
 */
void printErr(int* LCDLine){
	char errMsg[MAX_INPUT] = "Error:";
	
	*LCDLine = 0;			//write to LCD line 1
	LCD_clear_line(LCDLine);	//clear line
	LCD_write_str(errMsg, LCDLine);	//print top half of error message
	
	strcpy(errMsg, "Line Too Long");
	*LCDLine = 1;			//write to LCD line 2
	LCD_clear_line(LCDLine);	//clear line
	LCD_write_str(errMsg, LCDLine);	//write bottom half of error message
	
	return;
}

/*
 * Function:	outputHardLine
 *  Prints hard coded string to LCD screen. Checks the status of the input 
 *  line and output accordingly. If status is 0 (normal status), write the
 *  input string to the input line of the LCD screen. If status is 1 (clear 
 *  screen), output nothing. If status is 2 (string is too long), then output 
 *  an error message.
 *
 *  input	char[]	string to be written to the LCD screen
 *
 *  returns:	none
 */
void outputHardLine(char input[MAX_INPUT]){
	int status = 0;
	int LCDLine = 0;		//write to line 1 of LCD screen
	
	status = checkInput(input);	//check input string
	
	//if status is 0, clear the first line of the LCD screen and write input
	//line to LCD screen. String is wrapped if it is too long for one line 
	if(status == 0){
		LCD_clear_line(&LCDLine);	//clear line
		LCD_write_str(input, &LCDLine);	//write line
	}
	//if status is 2 (line is too long), write error message to LCD screen
	if(status == 2){
		printHardErr();
	}
	return;
}

/*
 * Function:	printHardErr
 *  Write error message to LCD screen. Used for hard coded strings
 *
 *  returns:	none
 */
void printHardErr(){
	char errMsg[MAX_INPUT] = "Error:";
	int LCDLine = 0;			//set line to 1
	LCD_clear_line(&LCDLine);		//clear line
	LCD_write_str(errMsg, &LCDLine);	//write first half of error message
	
	strcpy(errMsg, "Line Too Long");
	LCDLine = 1;				//set line to 2
	LCD_clear_line(&LCDLine);		//clear line
	LCD_write_str(errMsg, &LCDLine);	//write second half of error message
	
	return;
}

/*
 * Function:	outputHardChars
 *  Write a single char to each line of the LCD screen
 *
 *  char1	char	char to be written to first line
 *  char2	char	char to be written to second line
 *
 *  returns:	none
 */
void outputHardChars(char char1, char char2){
	//  Write a single character  //
	// line one	 //
	LCD_write_char(char1);
	
	// line two  //
	LCD_write_instruction(LCD_4bit_cursorSET | LineTwoStart);	//change line
	_delay_us(80);   //  delay must be > 37us  - datasheet forgets to mention this //
	LCD_write_char(char2);
	return;
}

# ATMEGA2560-Serial-Code-and-LCD-Display
Embedded C program for ATMEGA 2560 created for CENG447 Lab 5. ATMEGA2560 communicates with computer over SSH and prints strings and characters to a connected LCD screen. The controller sends a prompt to the computer to send a string. The received string is printed to the next line of the LCD screen, then is echoed back to the computer. A string that is longer than the length of the screen (16 chars) is split into two lines on the LCD screen. A string that is longer than both lines of the LCD screen combined (32 chars) is not printed or echoed, and an error message is printed on the LCD screen. Sending the ^C (Ctrl + c) command causes the controller to clear the screen. This code also includes commented hard code for printing individual characters to each line of the LCD as well as commented hard code for printing a string to the top line of the LCD.

# Demo Video
A less compressed version can be found in the main folder of this repo.

https://user-images.githubusercontent.com/103338215/215163268-dce280ff-2a69-401f-8299-fde09bf2dad1.mp4


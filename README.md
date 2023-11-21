# NET1

DEMO (outdated): https://www.youtube.com/watch?v=r1xb2h-P_Vc
DEMO (indated?): https://youtu.be/knWo9potAe0


## Building and Running
- Extract the zip folder in dev/lib
- Open VS solution, build all three projects in x64 Release, gprotobufs don't like debug
- Go to the built exe's, copy welcomemats.txt over
- You'll also need to copy some dlls over to the exe's, should just be the ones in AuthServer (refer to my setup in the demo vid @5:17)
- Load the 2 sql tables into a schema called 'authschema', and have MySQL80 running
- Start the exe's up in this order: AuthServer.exe -> TCPServerWithSelect.exe -> TCPClient.exe

## Building and Running (Outdated)
- Open the VS solution, and build the server and client projects in x64 Debug or Release
- Find the built exe's, and copy the welcomemats.txt over to the folder they're in (the server needs it)
- Run the server exe first, then as many (within reason) clients as you want!
- All controls are explained in the program
    


## Issues to Watch Out For
- Word wrap works with user input, but if changing the screen size does anything to the current user input (its wordwrap), it'll break (only visually)
- Clicking somewhere in the console will not allow the user to input, right click anywhere in the window to undo this
- SOMETIMES the client doesn't start up right, might then need to right click other server exe windows to update them, then relaunch the client. Seeing the welcome mat and a full welcome message is a sign of success


## Design Choices
- This was a less complex project, so only 1 design choice stands out to me
### Non-Blocking Input
- I didn't want to TOUCH threads, so I created an "input buffer"
- I used conio.h's _kbhit() and _getch() to get key inputs into the window
- These inputs were evaluated, and if deemed valid, were added to the input buffer
- The input buffer was deleted and printed any time the window updated, whether with a received message, or the input buffer changing
- Some other nitty-gritty stuff was used to keep track of how wide the console was, and how to deal with word wrap (not window-resize instigated word wrap)

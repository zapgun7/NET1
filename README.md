# NET1

DEMO: https://www.youtube.com/watch?v=r1xb2h-P_Vc


## Building and Running
- Open the VS solution, and build the server and client projects in x64 Debug or Release
- Find the built exe's, and copy the welcomemats.txt over to the folder they're in (the server needs it)
- Run the server exe first, then as many (within reason) clients as you want!
- All controls are explained in the program
    


## Issues to Watch Out For
- Word wrap works with user input, but if changing the screen size does anything to the current user input (its wordwrap), it'll break (only visually)
- Clicking somewhere in the console will not allow the user to input, right click anywhere in the window to undo this


## Design Choices
- This was a less complex project, so only 1 design choice stands out to me
# Non-Blocking Input
- I didn't want to TOUCH threads, so I created an "input buffer"
- I used conio.h's _kbhit() and _getch() to get key inputs into the window
- These inputs were evaluated, and if deemed valid, were added to the input buffer
- The input buffer was deleted and printed any time the window updated, whether with a received message, or the input buffer changing
- Some other nitty-gritty stuff was used to keep track of how wide the console was, and how to deal with word wrap (not window-resize instigated word wrap)

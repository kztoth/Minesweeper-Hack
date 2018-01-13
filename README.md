# MineSeeper Hack
I was sick of losing games of Mine Sweeper to just a legitimate 50/50 chance. I thought that there must be some way to remove this chance and just make it so each game could be won with enough logic. This lead me to add a much needed hint hotkey to the Windows 7 version of the game. 

## Use
When you want a hint you can mark squares with the ? icon and then just press the H key. 

## Implementation
I decided the best way to go about this was to create a DLL that could be injected into the game. I could have just reverse engineered some data structures and printed that out to console but I thought this way would be more immersive. Also simple changes to the structures would not take effect until there was some other update to push to the screen. Therefore, I reverse engineered the code of the game so that I could find the functions for both right and left clicking on squares, as well as the data structures to pass to each, so that I could call them from my own thread. 
When you press the hint key the DLL looks through the memory to find which squares are marked and compares them to mine locations. It then calls the right click function to mark the bombs with a flag followed by calling the left click function to click the cleared spots.
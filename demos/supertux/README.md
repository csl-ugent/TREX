# Supertux
This demo localises the instructions that modify the number of coins the player has in the game of SuperTux.

## Set-up instructions
To run this demo, you will need to build SuperTux. And install the `scanmem` tool.

* SuperTux is included as a submodule in this folder. Follow the instructions outlined in [SuperTux' INSTALL.md](supertux/INSTALL.md) to build SuperTux.

* You can install `scanmem` using your system's package manager. For example: `apt-get install scanmem`

## Running the demo

When launching the demo, a window will open with the game SuperTux.
You should start a level and collect some coins.
In the terminal window where you launched the demo you will be required to enter the current number of coins displayed at the top of the game window.
Do this repeatedly after collecting more coins.
Once the tool has collected sufficient information to locate the instructions of interest, it will continue to the next stage and you are no longer required to enter any information on the command line.
However, you should keep playing the game and collecting coins.
Once the tool has found relevant instructions, it will insert them in the database.
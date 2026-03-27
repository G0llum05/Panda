# How to Debug this project

# Setup
Start the server:
`./startDebugServer.sh`

In another terminal start the client:
`./startDebugClient.sh`

## Connect to the server
`target remote localhost:8080`
Wait a while for the connection, slow af.

## How to use GDB
To enable the text-user-interface and view the source:
`tui la src`

If you can't see the source, just add a random breakpoint and continue;
`b 18`
`c`
It will display (magically) the src.

Now to remove a breakpoint use:
`clear lineNumber`

To display a variable use:
`display varName`

To change TUI window use (to scroll the command line rather than the code):
`focus next`

Seek more informations online, not every command is implemented by the gdb-server.

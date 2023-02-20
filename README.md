# ITA BPM software

## server

This folder contains the code which runs on the Eclypse-Z7 and manages direct interface with the readout board.

## client

This folder contains the code run on a remote machine which connects to the server on the Eclypse-Z7.  The client remotely controls the server, including configuration of the hardware and starting and stopping the DAQ, and directly recieves the data to record to a file.  
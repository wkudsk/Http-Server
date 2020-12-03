# Assignment 2 - multi-threaded HTTP server with logging

Work done by William Kudsk (wkudsk) for CSE 130, Fall 2019

httpserver is a program written in C++ that functions just the same as the cat linux command.
 - first use the Makefile to create an executable.
 - execute the program passing it a hostname or IP address, and optionally a port.
 - In addition, throw flags -N to declare amount of threads and -l if you want to log
 - Example: ./httpserver -N 6 -l logFile localhost 8080 
 - Use ctrl+C to terminate the server
 - A client must connect to the server with a PUT or GET call.
 - The client must not have a / before the resource name, and the client must send an end of file message if it does not have a content length.
 
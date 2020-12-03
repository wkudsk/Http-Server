# Assignment 1 - httpserver

Work done by William Kudsk (wkudsk) for CSE 130, Fall 2019

httpserver is a program written in C++ that functions just the same as the cat linux command.
 - first use the Makefile to create an executable.
 - execute the program passing it a hostname or IP address, and optionally a port.
 - Use ctrl+C to terminate the server
 - A client must connect to the server with a PUT or GET call.
 - The client must not have a / before the resource name, and the client must send an end of file message if it does not have a content length.
 
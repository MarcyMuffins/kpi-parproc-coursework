# Paralell processing/Inverted index coursework project

## UML Diagrams

The UML folder holds the images as well as the source code for the UML diagrams, written in the [PlantUML](https://plantuml.com/) language.
These are subject to change.

## Client & Server code

In the code folder you'll find folders for both the client and the server.

The client is able to connect to a local inverted index server and query words, print out the list of files the words were found it, and print out one of the files. If admin mode is selected, the client can put in a folder to scan for text files, and then send all the files over to the server for storage.

The server consists of three main structures:

 - The main thread creates the inverted indexm the scheduler thread and listens for connections.

 - Connected clients are sent into a thread pool queue to wait for an available thread. Priority goes to those who connected first.

 - The scheduler fetches the list of currently processed files and compares it with every file in the database directory. New files are added to the index. (It is possible that a file is added between fetching processed file and adding new files but the function that adds new files already checks if it exists, so at most you'll get a wrong log message.)

## TODO:

 - Finalize server and client inputs (db path, debug mode, etc)

 - More application protocol comments

 - Pray to Dennis Ritchie that the code works

## Problems

 - Currently logging is (theoretically) slow, as every time a thread wants to log something it opens and closes the file. 

 - Detached threads

## Future improvements

 - Log queue

 - Replace all small buffers with the main buffer

 - Server termination option

 - Proper client UUID instead of incrementing numbers

# Parallel processing/Inverted index coursework project

## UML Diagrams

The UML folder holds the images as well as the source code for the UML diagrams, written in the [PlantUML](https://plantuml.com/) language.
These are subject to change.

## Client & Server code

In the code folder you'll find folders for both the client, the java client, the server and the server testing program.

The client is able to connect to a local inverted index server and query words, print out the list of files the words were found it, and print out one of the files. If admin mode is selected, the client can put in a folder to scan for text files, and then send all the files over to the server for storage.

The server consists of three main structures:

 - The main thread creates the inverted index, the scheduler thread and listens for connections.

 - Connected clients are sent into a thread pool queue to wait for an available thread. Priority goes to those who connected first.

 - The scheduler fetches the list of currently processed files and compares it with every file in the database directory. New files are added to the index. (It is possible that a file is added between fetching processed file and adding new files but the function that adds new files already checks if it exists, so at most you'll get a wrong log message.)

## Build instructions

### C++ Client, Server and Server Tester:
1. Download and install [Visual Studio](https://visualstudio.microsoft.com/) with support for C++ 20.
2. Download and extract the repository to a location convenient to you.
3. Navigate to `kpi-parproc-coursework-main\code\InvertedIndexServer`
4. Open `InvertedIndexServer.sln` in Visual Studio
5. On top, select `Debug` and change the configuration to `Release`
6. Go to `Project` -> `Properties` -> `Configuration Properties` -> `General`
7. Make sure that the `C++ Language Standard` field is set to `ISO C++ 20 Standard`
8. Click `Build` -> `Build Solution`
9. Go back to the folder with the solution.
10. Navigate to `\x64\Release` .
11. Launch the needed `.exe` file from the command line.

### Java client
1. Download and install OpenJDK 21 binaries from [the website.](https://adoptium.net/)
2. Download and extract the repository to a location convenient to you.
3. Navigate to `kpi-parproc-coursework-main\code\InvertedIndexClientJava`
4. Open `cmd` or Powershell in the folder.
5. Run `javac Main.java`
6. Run `jar cfe InvertedIndexJavaClient.jar Main Main.class`
7. A file named `InvertedIndexJavaClient.jar` should be created in the folder.
8. Run `java -jar InvertedIndexJavaClient.jar` in the command line to start the java client.

## TODO:

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

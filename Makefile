CC=g++
FLAGS=-Wall
TARGET=main

# Dependencies
WEBSERVER_ROOT=webserver

OBJECTS=$(TARGET).o $(WEBSERVER_ROOT).o

build: $(OBJECTS)
	$(CC) $(FLAGS) -o $(TARGET) $(OBJECTS)


main.o:
	$(CC) $(FLAGS) -c -o $(TARGET).o $(TARGET).cpp 

$(WEBSERVER_ROOT).o:
	$(CC) $(FLAGS) -c -o $(WEBSERVER_ROOT).o $(WEBSERVER_ROOT).cpp 


clean:
	rm -rf $(TARGET) $(OBJECTS)
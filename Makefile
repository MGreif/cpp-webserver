CC=g++
FLAGS=-Wall
TARGET=main
OBJECTS=$(TARGET).o

build: $(TARGET).o
	$(CC) $(FLAGS) -o $(TARGET) $(OBJECTS)


main.o:
	$(CC) $(FLAGS) -c -o $(TARGET).o $(TARGET).cpp 

clean:
	rm -rf $(TARGET) $(OBJECTS)
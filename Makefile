CC = gcc
CFLAGS = -g -lpthread

TARGET1 = Server/myftpserver
TARGET2 = Client/myftp

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(TARGET1).c
	$(CC) $(CFLAGS) -o $(TARGET1) $(TARGET1).c

$(TARGET2): $(TARGET2).c
	$(CC) $(CFLAGS) -o $(TARGET2) $(TARGET2).c

clean:
	$(RM) $(TARGET1)
	$(RM) $(TARGET2) 

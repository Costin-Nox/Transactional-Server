#! /usr/bin/make

# Makefile for CMPT 431 - assignment 2 (SERVER)

# Including of non standard library files:
#   INCLUDEDIR is where the header files can be found
#   LIBDIR is where the library object files can be found
INCLUDEDIR=/usr/include/
LIBDIR=/usr/lib

# If you have more source files add them here 
SOURCE= CMPT431-Assignment3.cpp LogHandler.cpp Server.cpp ServerTools.cpp Session.cpp TransactionManager.cpp Client.cpp ReplicationManager.cpp

# The compiler we are using 
CC= g++

# The flags that will be used to compile the object file.
# If you want to debug your program,
# you can add '-g' on the following line
CFLAGS= -O3 -g -pedantic -std=c++11

# The name of the final executable 
EXECUTABLE= Server

# The basic library we are using add the other libraries you want to link
# to your program here 

# Linux (default)
LDFLAGS = -lpthread -lboost_system -lboost_thread -lboost_serialization -lboost_filesystem -lboost_iostreams

# If you have other library files in a different directory add them here 
INCLUDEFLAG= -I. -I$(INCLUDEDIR) -Iinclude/
LIBFLAG= -L$(LIBDIR)

# Don't touch this one if you don't know what you're doing 
OBJECT= $(SOURCE:.cpp=.o)

# Don't touch any of these either if you don't know what you're doing 
all: $(OBJECT) depend
	$(CC) $(CFLAGS) $(INCLUDEFLAG) $(LIBFLAG) $(OBJECT) -o $(EXECUTABLE) $(LDFLAGS)

depend:
	$(CC) -M $(SOURCE) > depend

$(OBJECT):
	$(CC) $(CFLAGS) $(INCLUDEFLAG) -c -o $@ $(@:.o=.cpp)

clean_object:
	rm -f $(OBJECT)

clean:
	rm -f $(OBJECT) depend $(EXECUTABLE)

include depend

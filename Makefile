PROGRAM=youtube-downloader-ui
CC=gcc
PKGCONFIG=pkg-config
PACKAGES=gtk+-3.0
FLAGS=`$(PKGCONFIG) --cflags $(PACKAGES)` -Wall
LIBS=`$(PKGCONFIG) --libs $(PACKAGES)`

all: $(PROGRAM)

clean:
	-rm -f $(PROGRAM).o

$(PROGRAM): $(PROGRAM).o
	$(CC) $^ -o $@ $(LIBS)

$(PROGRAM).o: $(PROGRAM).c
	$(CC) -c $^ -o $@ $(FLAGS)

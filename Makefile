CC=gcc
FLAGS= -Wall 
CFLAGS += $(shell pkg-config --cflags json-c)
CFLAGS += -lreadline
LDFLAGS += $(shell pkg-config --libs json-c)
LDFLAGS += $(shell pkg-config --libs libcurl)
LDFLAGS += -Iinclude
all: apish
apish: clean request.o command.o util.o
	${CC} ${FLAGS} ${CFLAGS} ${LDFLAGS} command.o request.o util.o src/apish.c -o apish -lcurl

request.o:
	${CC} ${FLAGS} ${CFLAGS} ${LDFLAGS} -c src/request.c -o request.o

command.o:
	${CC} ${FLAGS} ${CFLAGS} ${LDFLAGS} -c src/command.c -o command.o

util.o:
	${CC} ${FLAGS} ${CFLAGS} ${LDFLAGS} -c src/util.c -o util.o


clean:
	find -name '*.o' -exec rm -f {} \;
	rm -f apish

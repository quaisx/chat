files := simplechat.cxx
CC = g++-10
CFLAGS = -w -std=c++20 -g
LDLIBS = -lpanel -lncurses -lpthread -lredis++ -lhiredis
all: title simplechat 

#" this is a comment
# $< is the file that has been changed and will trigger a build


.cxx.o:
	@echo "file " $< " changed"
	$(CC) ${CFLAGS} -c $<

title:
	@echo "Buidling Kuna chat using redis.io"
	@echo "Be patient, it will only take a snap to build"

simplechat: $(files) 
	$(CC) $(CFLAGS) $@.cxx $(LDLIBS) -o $@ 

clean:
	@echo "removing build artifacts"
	rm -f chat.o connector.o chat

.PHONE: all

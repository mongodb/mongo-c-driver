all: libmongoc-1.0.so


OBJECTS :=
OBJECTS += mongoc-client.o
OBJECTS += mongoc-event.o
OBJECTS += mongoc-uri.o


HEADERS :=
HEADERS += mongoc.h
HEADERS += mongoc-client.h
HEADERS += mongoc-client-private.h
HEADERS += mongoc-event-private.h
HEADERS += mongoc-flags.h
HEADERS += mongoc-host-list.h
HEADERS += mongoc-uri.h


SOURCES := 
SOURCES += mongoc-client.c
SOURCES += mongoc-event.c
SOURCES += mongoc-uri.c


DEBUG := -g


OPTIMIZE := -O2


WARNINGS :=
WARNINGS += -Wall
WARNINGS += -Wextra
WARNINGS += -Werror


PKGS :=
PKGS += libbson-1.0


libmongoc-1.0.so: $(OBJECTS)
	$(CC) -fPIC -shared -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(OBJECTS) $(shell pkg-config --libs $(PKGS))


mongoc-uri.o: mongoc-uri.c mongoc-uri.h
	$(CC) -fPIC -c -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(shell pkg-config --cflags $(PKGS)) mongoc-uri.c


mongoc-client.o: mongoc-client.c mongoc-client.h mongoc-event.o mongoc-event-private.h
	$(CC) -fPIC -c -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(shell pkg-config --cflags $(PKGS)) mongoc-client.c


mongoc-event.o: mongoc-event.c mongoc-event-private.h
	$(CC) -fPIC -c -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(shell pkg-config --cflags $(PKGS)) mongoc-event.c


clean:
	rm -f *.o libmongoc-1.0.so

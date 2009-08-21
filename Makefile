
OBJECTS = mongo.o

.c.o:
	gcc -ansi -pedantic -o $@ -c $<

test: $(OBJECTS)  test.o
	gcc -o test $(OBJECTS) test.o

clean:
	-rm $(OBJECTS) test test.o

CC = gcc
CFLAGS = -g -O2 -Wall -ansi -pedantic -c
LDFLAGS = -lallegro -lallegro_acodec -lallegro_audio -lallegro_font -lallegro_image -lallegro_ttf -lm

HEADERS = anim.h input.h memory.h physics.h random.h resource.h utilities.h

OBJECTS = anim.o beeball.o input.o memory.o physics.o random.o resource.o


.PHONY : clean pretty run

beeball : $(OBJECTS)
	$(CC) -o beeball $(OBJECTS) $(LDFLAGS)

anim.o : anim.c anim.h
	$(CC) $(CFLAGS) anim.c

beeball.o : beeball.c $(HEADERS)
	$(CC) $(CFLAGS) beeball.c

input.o : input.c $(HEADERS)
	$(CC) $(CFLAGS) input.c

memory.o : memory.c memory.h
	$(CC) $(CFLAGS) memory.c

physics.o : physics.c physics.h
	$(CC) $(CFLAGS) physics.c

random.o : random.c random.h
	$(CC) $(CFLAGS) random.c

resource.o : resource.c resource.h
	$(CC) $(CFLAGS) resource.c

run : beeball
	./beeball

clean :
	\rm -f $(OBJECTS)

pretty :
	SIMPLE_BACKUP_SUFFIX=".BAK" \indent -kr --no-tabs -l80 *.c *.h
	\rm -f *.BAK


CFLAGS += -Wall -O0 -g -I../libuguu `neon-config --cflags`
LDFLAGS += `neon-config --libs`

LIBUGUU = ../libuguu/libuguu.a
OBJ = wdwk.o webdavscan.o
NAME = webdavscan

$(NAME): $(OBJ) $(LIBUGUU)

$(LIBUGUU):
	$(MAKE) -C ../libuguu

install: $(NAME)
	cp $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

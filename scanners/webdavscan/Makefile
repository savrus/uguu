CFLAGS += -Wall -O3 -I../libuguu `neon-config --cflags`
LDFLAGS += `neon-config --libs`

LIBUGUU = ../libuguu/libuguu.a
OBJ = wdwk.o webdavscan.o
NAME = webdavscan

$(NAME): $(OBJ) $(LIBUGUU)

$(LIBUGUU):
	$(MAKE) -C ../libuguu

install: $(NAME)
	rm -f ../../bin/$(NAME)
	cp $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

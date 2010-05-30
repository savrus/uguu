CFLAGS += -Wall -O3 -I../libuguu
LDFLAGS += -lpq

LIBUGUU = ../libuguu/libuguu.a
OBJ = sqlwk.o sqlscan.o
NAME = sqlscan

$(NAME): $(OBJ) $(LIBUGUU)

$(LIBUGUU):
	$(MAKE) -C ../libuguu

install: $(NAME)
	cp -f $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

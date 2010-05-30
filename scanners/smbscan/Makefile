CFLAGS += -Wall -O3 -I../libuguu
LDFLAGS += -lsmbclient

LIBUGUU = ../libuguu/libuguu.a
OBJ = smbwk.o smbscan.o
NAME = smbscan

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

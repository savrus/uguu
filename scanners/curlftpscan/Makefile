CFLAGS += -Wall -O3 -I../libuguu `curl-config --cflags`
LDFLAGS += `curl-config --libs`

LIBUGUU = ../libuguu/libuguu.a
OBJ = cfwk.o curlftpscan.o ftpparse.o
NAME = curlftpscan

$(NAME): $(OBJ) $(LIBUGUU)

$(LIBUGUU):
	$(MAKE) -C ../libuguu

install: $(NAME)
	cp -f $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

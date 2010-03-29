CFLAGS += -Wall -O3
LDFLAGS += -lpq

OBJ = dt.o buf.o sqlwk.o sqlscan.o
NAME = sqlscan

$(NAME): $(OBJ)

install: $(NAME)
	cp $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

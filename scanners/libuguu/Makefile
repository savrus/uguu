CFLAGS += -Wall -O3

OBJ = dt.o buf.o getpass.o umd5.o stack.o cuckoo.o dtread.o
NAME = libuguu.a

$(NAME): $(OBJ)
	ar rcs $(NAME) $(OBJ)

clean:
	rm -f $(OBJ) $(NAME)


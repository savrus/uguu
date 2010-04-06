CFLAGS += -Wall -O3

OBJ = dt.o buf.o getpass.o
NAME = libuguu.a

$(NAME): $(OBJ)
	ar rcs $(NAME) $(OBJ)

clean:
	rm -f $(OBJ) $(NAME)


CC = gcc
CXX = g++
CALLFLAGS = -Wall -O3 -I../libuguu
CFLAGS += $(CALLFLAGS)
CXXFLAGS += $(CALLFLAGS)
LDFLAGS += 

CCCMD = $(CC) $(CFLAGS)
CXXCMD = $(CXX) $(CXXFLAGS)

LIBUGUU = ../libuguu/libuguu.a
OBJ = ftpparse.o FtpSockLib.o vs_s.o ftpscan.o
NAME = ftpscan

$(NAME): $(OBJ) $(LIBUGUU)
	$(CXXCMD) $(OBJ) $(LIBUGUU) -o $(NAME)

$(LIBUGUU):
	$(MAKE) -C ../libuguu

vs_s.o: vs_s.c vs_s.h
	$(CCCMD) -c vs_s.c -o vs_s.o

ftpparse.o: ftpparse.c ftpparse.h vs_s.h
	$(CCCMD) -c ftpparse.c -o ftpparse.o

FtpSockLib.o: FtpSockLib.cpp FtpSockLib.h ftpparse.h vs_s.h $(LIBUGUU)
	$(CXXCMD) -c FtpSockLib.cpp -o FtpSockLib.o

ftpscan.o: ftpscan.cpp FtpSockLib.h $(LIBUGUU)
	$(CXXCMD) -c ftpscan.cpp -o ftpscan.o

install: $(NAME)
	rm -f ../../bin/$(NAME)
	cp $(NAME) ../../bin/

clean:
	rm -f $(OBJ) $(NAME)

cleanall: clean
	rm -f ../../bin/$(NAME)

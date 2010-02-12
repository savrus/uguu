CC = gcc
CXX = g++
CALLFLAGS = -Wall -O3
CFLAGS += $(CALLFLAGS)
CXXFLAGS += $(CALLFLAGS)
LDFLAGS += 

CCCMD = $(CC) $(CFLAGS)
CXXCMD = $(CXX) $(CXXFLAGS)

ftpscan: dt.o ftpparse.o FtpSockLib.o vs_s.o ftpscan.o
	$(CXXCMD) dt.o ftpparse.o FtpSockLib.o vs_s.o ftpscan.o -o ftpscan

dt.o: dt.c dt.h logpp.h
	$(CCCMD) -c dt.c -o dt.o

vs_s.o: vs_s.c vs_s.h
	$(CCCMD) -c vs_s.c -o vs_s.o

ftpparse.o: ftpparse.c ftpparse.h vs_s.h
	$(CCCMD) -c ftpparse.c -o ftpparse.o

FtpSockLib.o: FtpSockLib.cpp FtpSockLib.h ftpparse.h vs_s.h logpp.h
	$(CXXCMD) -c FtpSockLib.cpp -o FtpSockLib.o

ftpscan.o: ftpscan.cpp FtpSockLib.h dt.h logpp.h
	$(CXXCMD) -c ftpscan.cpp -o ftpscan.o

install: ftpscan
	cp ftpscan ../../bin/

clean:
	rm -f dt.o ftpscan.o ftpparse.o FtpSockLib.o vs_s.o ftpscan

cleanall: clean
	rm -f ../../bin/ftpscan

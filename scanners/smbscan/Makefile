CFLAGS += -Wall -O3
LDFLAGS += -lsmbclient

smbscan: dt.o smbwk.o smbscan.o

install: smbscan
	cp smbscan ../../bin/

clean:
	rm -f dt.o smbwk.o smbscan.o smbscan

cleanall: clean
	rm -f ../../bin/smbscan

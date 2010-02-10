CFLAGS += -Wall -g
LDFLAGS += -lsmbclient

dist: smbscan
	cp smbscan ../../bin/

smbscan: dt.o smbwk.o smbscan.o

clean:
	rm -f dt.o smbwk.o smbscan.o smbscan ../../bin/smbscan

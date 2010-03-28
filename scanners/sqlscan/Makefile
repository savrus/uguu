CFLAGS += -Wall -O0 -g
LDFLAGS += -lpq

OBJ = dt.o buf.o sqlwk.o sqlscan.o

sqlscan: $(OBJ)

install: sqlscan
	cp sqlscan ../../bin/

clean:
	rm -f $(OBJ) sqlscan

cleanall: clean
	rm -f ../../bin/sqlscan

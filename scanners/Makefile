SCANDIRS = smbscan ftpscan webdavscan sqlscan
ALLDIRS = libuguu $(SCANDIRS)

all: $(ALLDIRS)
clean: $(ALLDIRS:%=%-clean)
install: $(SCANDIRS:%=%-install)

$(ALLDIRS)::
	$(MAKE) -C $@

$(ALLDIRS:%=%-clean)::
	$(MAKE) -C $(@:%-clean=%) clean

$(SCANDIRS:%=%-install)::
	$(MAKE) -C $(@:%-install=%) install


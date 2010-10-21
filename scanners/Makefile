#SCANDIRS = smbscan ftpscan webdavscan curlftpscan sqlscan
SCANDIRS = smbscan ftpscan webdavscan curlftpscan
ALLDIRS = libuguu $(SCANDIRS)

all: $(ALLDIRS)
clean: $(ALLDIRS:%=%-clean)
cleanall: $(SCANDIRS:%=%-cleanall) libuguu-clean
install: $(SCANDIRS:%=%-install)

$(ALLDIRS)::
	$(MAKE) -C $@

$(ALLDIRS:%=%-clean)::
	$(MAKE) -C $(@:%-clean=%) clean

$(SCANDIRS:%=%-cleanall)::
	$(MAKE) -C $(@:%-cleanall=%) cleanall

$(SCANDIRS:%=%-install)::
	$(MAKE) -C $(@:%-install=%) install


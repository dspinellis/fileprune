NAME=fileprune
VERSION=$(shell ident $(NAME).c | awk '/Id:/{print $$3} ')
DIR=$(NAME)-$(VERSION)
SRC_BALL=$(DIR).tar.gz
DOC=$(NAME).txt $(NAME).pdf
WEBDIR=/dds/pubs/web/home/sw/unix/$(NAME)
ADDSRC=getopt.c erf.c
ADDFILE=$(ADDSRC) ChangeLog.txt fileprune.jpg

CC=cl
CFLAGS=-Zi

$(NAME).exe: $(NAME).c $(ADDSRC)
	$(CC) fileprune.c $(ADDSRC) setargv.obj

$(NAME).ps: $(NAME).1
	eqn <$? | groff -man -Tps > $@

$(NAME).txt: $(NAME).1
	eqn <$? | groff -man -Tascii | col -b > $@

$(NAME).pdf: $(NAME).ps
	ps2pdf $? $@

src-tarball: $(SRC_BALL)

$(SRC_BALL): $(DOC) $(NAME).1 $(NAME).c $(ADDFILE) Makefile.dist Makefile
	-mkdir $(DIR)
	cp -f $(NAME).1 $(NAME).c $(NAME).pdf $(NAME).txt $(ADDSRC) $(DIR)
	cp -f Makefile.dist $(DIR)/Makefile
	for i in $(NAME).1 $(NAME).c Makefile $(ADDSRC) ; do perl /usr/local/bin/lf.bat $(DIR)/$$i ; done
	tar cvf - $(DIR) | gzip -c >$(SRC_BALL)
	cmd /c rd /q/s "$(DIR)"

web: $(DOC) $(SRC_BALL)
	-chmod 666 $(WEBDIR)/$(NAME).c
	-chmod 666 $(WEBDIR)/$(NAME).1
	cp -f $(SRC_BALL) $(ADDFILE) $(NAME).exe $(NAME).c $(NAME).1 $(NAME).pdf $(WEBDIR)
	sed -e "s/VERSION/${VERSION}/" index.html >${WEBDIR}/index.html

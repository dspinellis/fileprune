NAME=fileprune
PREFIX?=/usr/local
INSTALL?=install
CFLAGS=-D_FILE_OFFSET_BITS=64

all: $(NAME)

$(NAME): $(NAME).c
	$(CC) $(CFLAGS) -o $@ $? -lm

install: $(NAME)
	mkdir -p $(PREFIX)/bin
	$(INSTALL) -s $(NAME) $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	gzip -c $(NAME).1 >$(PREFIX)/share/man/man1/$(NAME).1.gz

clean:
	rm -f $(NAME) $(NAME).o


test: test1 test2

test1:
	mkdir -p tfiles
	for i in `seq 1 30` ; do touch -d "$$i days ago" tfiles/$$i ; done
	expr `./fileprune -c 10 -n tfiles/* | wc -l` == 20
	expr `./fileprune -c 10 -N tfiles/* | wc -l` == 10
	./fileprune -e 2 -c 10 tfiles/*
	test -r tfiles/1
	test -r tfiles/2
	test -r tfiles/3
	test -r tfiles/4
	test -r tfiles/5
	test -r tfiles/6
	test -r tfiles/7
	test -r tfiles/8
	test -r tfiles/15
	test -r tfiles/30
	! test -r tfiles/9
	! test -r tfiles/10
	! test -r tfiles/11
	! test -r tfiles/12
	! test -r tfiles/13
	! test -r tfiles/14
	! test -r tfiles/16
	! test -r tfiles/17
	! test -r tfiles/18
	! test -r tfiles/19
	! test -r tfiles/20
	! test -r tfiles/21
	! test -r tfiles/22
	! test -r tfiles/23
	! test -r tfiles/24
	! test -r tfiles/25
	! test -r tfiles/26
	! test -r tfiles/27
	! test -r tfiles/28
	! test -r tfiles/29
	rm -rf tfiles

test2:
	mkdir -p tfiles
	for i in 1 2 3 30 ; do touch -d "$$i days ago" tfiles/$$i ; done
	./fileprune -e 2 tfiles/*
	test -r tfiles/1
	! test -r tfiles/2
	test -r tfiles/3
	test -r tfiles/30
	rm -rf tfiles

test3:
	mkdir -p tfiles
	touch -d "2 minutes ago" tfiles/2;
	touch tfiles/1;
	./fileprune -F tfiles/*
	! test -r tfiles/1
	test -r tfiles/2
	rm -rf tfiles

test4:
	mkdir -p tfiles
	touch -d "2 minutes ago" tfiles/2;
	touch tfiles/1;
	./fileprune -Y -F tfiles/*
	test -r tfiles/1
	test -r tfiles/2
	rm -rf tfiles


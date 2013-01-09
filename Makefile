NAME=fileprune
PREFIX?=/usr/local
INSTALL?=install

all: $(NAME)

$(NAME): $(NAME).c
	$(CC) $(CFLAGS) -o $@ $? -lm

install: $(NAME)
	$(INSTALL) -s $(NAME) $(PREFIX)/bin
	gzip -c $(NAME).1 >$(PREFIX)/share/man/man1/$(NAME).1.gz

clean:
	rm -f $(NAME) $(NAME).o

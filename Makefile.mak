#
# Makefile for compiling with Microsoft C/++ and nmake
# nmake /f Makefile.mak
#

NAME=fileprune
ADDSRC=getopt.c erf.c

CC=cl

$(NAME).exe: $(NAME).c $(ADDSRC)
	$(CC) $(CFLAGS) /Fe$@ $(NAME).c $(ADDSRC)

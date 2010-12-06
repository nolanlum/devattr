PROG=   devattr
SRCS=   devattr.c
WARNS?= 3

LDADD=  -ldevattr -lprop

MAN8=

CFLAGS += -Wall
#debug
#CFLAGS += -ggdb

.include <bsd.prog.mk>

PROG=   devattr
SRCS=   devattr.c
WARNS?= 3

LDADD=  -ldevattr -lprop

CFLAGS += -Wall
#debug
#CFLAGS += -ggdb

.include <bsd.prog.mk>

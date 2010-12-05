PROG=   devattr
SRCS=   devattr.c
WARNS?= 3

LDADD=    -ldevattr

NOMAN=

CFLAGS += -Wall
#debug
#CFLAGS += -ggdb

.include <bsd.prog.mk>

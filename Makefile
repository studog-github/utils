OS := $(shell uname -s)

ifeq (Linux,${OS})
  ifeq (__uClinux__,$(findstring __uClinux__,${CFLAGS}))
    LIBCFG := $(ROOTDIR)/vendors/$(CONFIG_VENDOR)/libcfg
    CFLAGS += -D ${OS} -Wall -I$(LIBCFG)
    LDLIBS := -L$(LIBCFG) -lcfg $(LDLIBS)
  else
    CC := cc
    CFLAGS += -I/firm/9bit/linux-2.6.11/include
    WARNFLAG := -Wall
    LIBFLAGS :=
  endif
endif

ifeq (SCO_SV,${OS})
CC := cc
WARNFLAG := -w3 -wX
LIBFLAGS := -lm
endif

ifeq (SunOS,${OS})
CC := gcc
WARNFLAG := -Wall
LIBFLAGS := -lm -lelf
endif

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
PROGS := $(SRCS:.c=)
GDBS := $(SRCS:.c=.gdb)

ifneq (__uClinux__, $(findstring __uClinux__,${CFLAGS}))
  CFLAGS := -D ${OS} -O ${WARNFLAG} ${CFLAGS}

%: %.c
	${CC} ${CFLAGS} -o $@ $< ${LIBFLAGS}
endif

all: ${PROGS}

clean:
	@rm -rf ${OBJS} ${PROGS} ${GDBS}

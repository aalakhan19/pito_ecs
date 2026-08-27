CC     = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -pthread

ifeq ($(ATOMIC),1)
    CFLAGS += -DPITO_ECS_OWNED_DELETE_ATOMIC=1
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CC = clang
endif

SRCS   = main.c entity.c systems.c deferred.c set.c threads.c owned_update.c \
         owned_init.c owned_attach.c owned_local.c \
         owned_delete.c

DEPS   = ../pito_ecs.h common.h threads.h
OBJS   = $(SRCS:.c=.o)

all: tests

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tests: $(OBJS)
	$(CC) -o tests $(OBJS) -lm -pthread

.PHONY: clean

clean:
	rm -f tests *.o

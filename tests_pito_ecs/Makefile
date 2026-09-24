CC     = gcc
CFLAGS = -std=c11 -g -Wall -Wextra -Wpedantic -pthread

ifeq ($(ATOMIC),1)
    CFLAGS += -DPITO_ECS_OWNED_DELETE_ATOMIC=1
endif

ifeq ($(AUTO_FLUSH),1)
    CFLAGS += -DPITO_ECS_OWNED_DELETE_AUTO_FLUSH=1
endif

ifeq ($(NDEBUG),1)
    CFLAGS += -DNDEBUG
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CC = clang
endif

SRCS   = main.c entity.c systems.c deferred.c set.c threads.c owned_update.c \
         owned_init.c owned_attach.c owned_local.c \
         owned_delete.c owned_insert.c owned_mixed.c

DEPS   = ../pito_ecs.h common.h threads.h
OBJS   = $(SRCS:.c=.o)

all: tests

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tests: $(OBJS)
	$(CC) -o tests $(OBJS) -lm -pthread

TSAN_CC = $(shell brew --prefix llvm)/bin/clang

tsan:
	$(TSAN_CC) $(CFLAGS) -O1 -fsanitize=thread -DPITO_ECS_TSAN=1 -o tests_tsan $(SRCS) -lm

.PHONY: clean tsan

clean:
	rm -f tests tests_tsan *.o

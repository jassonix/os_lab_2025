CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11
LDFLAGS := -pthread

.PHONY: all clean

all: mutex_without_mutex mutex_with_mutex factorial_mod deadlock

mutex_without_mutex: mutex.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

mutex_with_mutex: mutex.c
	$(CC) $(CFLAGS) -DUSE_MUTEX $< -o $@ $(LDFLAGS)

factorial_mod: factorial_mod.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

deadlock: deadlock.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f mutex_without_mutex mutex_with_mutex factorial_mod deadlock

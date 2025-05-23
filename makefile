# Compiler and flags
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread    # Linker flags for threading

# Targets (executables)
TARGETS = server client

# Source files for each target
server_OBJS = server.o p_cscf.o i_cscf.o hss.o s_cscf.o sip_user.o
client_OBJS = client.o

# Default rule: build both
all: $(TARGETS)

# Build rules for each target
server: $(server_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

client: $(client_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Generic .c to .o rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(TARGETS) *.o

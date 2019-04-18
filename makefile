# Compile settings
CC=gcc
CFLAGS=-Wall -pthread -std=c11 -O2
LFLAGS=-pthread

# Sources
SRC:=src
OBJ:=obj
BIN:=bin

COMMON_SOURCES=$(SRC)/multithreading.c $(SRC)/service.c $(SRC)/system.c
COMMON_HEADERS=$(wildcard $(SRC)/*.h)
COMMON_OBJECTS=$(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(COMMON_SOURCES))

MASTER=$(BIN)/master
SLAVE=$(BIN)/slave

# Specific variables
debug:		CFLAGS +=-DDEBUG -g

all: release


debug release:  $(MASTER) $(SLAVE)

$(MASTER): $(SRC)/master.c $(COMMON_OBJECTS) 
	$(CC) -o $@ $^ $(LFLAGS)

$(SLAVE): $(SRC)/slave.c $(COMMON_OBJECTS) 
	$(CC) -o $@ $^ $(LFLAGS)

$(OBJ)/%.o: $(SRC)/%.c
	mkdir -p $(BIN) $(OBJ)
	$(CC) $(CFLAGS) -c $^ -o $@


# Additional targets
.PHONY: clean

clean:
	rm -rf $(BIN)
	rm -rf $(OBJ)




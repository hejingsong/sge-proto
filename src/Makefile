TARGET=libsgeproto.so

BUILD_DIR=../build
SRC_DIR=core
OBJ_DIR=$(BUILD_DIR)/objs
LIB_DIR=$(BUILD_DIR)/lib

INC_FILES=$(SRC_DIR)/proto.h
OUT_INC_DIR=$(BUILD_DIR)/include/sge

# test
CFILES=$(wildcard $(SRC_DIR)/*.c)
OBJS=$(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o, $(notdir $(CFILES))))

DEBUG=-g

CC=gcc

CWARNSCPP= \
	-Wfatal-errors \
	-Wextra \
	-Wshadow \
	-Wundef \
	-Wwrite-strings \
	-Wredundant-decls \
	-Wdisabled-optimization \
	-Wdouble-promotion \
	-Wmissing-declarations

CWARNGCC= \
	-Wlogical-op \
	-Wno-aggressive-loop-optimizations

CWARNSC= -Wdeclaration-after-statement \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wstrict-prototypes \
	-Wc++-compat \
	-Wold-style-definition

CWARNS = $(CWARNSCPP) $(CWARNSC) $(CWARNGCC)
MYCFLAGS = $(CWARNS)
CFLAGS = $(DEBUG) -Wall -g $(MYCFLAGS) -fno-stack-protector -fno-common -march=native

all: pre_task $(TARGET) copy_inc

$(TARGET): $(OBJS)
	$(CC) -shared -o $(LIB_DIR)/$@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

pre_task:
	mkdir -p $(OUT_INC_DIR) $(OBJ_DIR) $(LIB_DIR)

copy_inc:
	mkdir -p $(OUT_INC_DIR)
	cp $(INC_FILES) $(OUT_INC_DIR)

clean:
	rm -rf $(BUILD_DIR)

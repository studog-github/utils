BIN = t

BUILD_DIR = ./build

SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

CFLAGS += -Werror -Wall -Wextra
CFLAGS += -Wfatal-errors
CFLAGS += -Wconversion
CFLAGS += -Wshadow

all: $(BUILD_DIR)/$(BIN)

$(BUILD_DIR)/$(BIN): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEPS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONY: clean
clean:
	-@$(RM) -r $(BUILD_DIR)

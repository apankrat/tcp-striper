CFLAGS += -Wall -DNDEBUG -g -Iglue/linux
#LDFLAGS += -lcrypto

SRC = \
	alloc.c \
	assert.c \
	map.c \
	event_loop_select.c \
	tcp-striper.c

OBJS = $(patsubst %,obj/%, $(subst .c,.o,$(SRC)))

all: tcp-striper

-include $(OBJS:.o=.d)

obj/%.o: %.c
	@echo c: $<
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $*.c > obj/$*.d
	@sed -e 's/^/obj\//' < obj/$*.d > obj/$*.dd
	@mv obj/$*.dd obj/$*.d

tcp-striper: $(OBJS)
	@echo l: $@
	@$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f tcp-striper $(OBJS) $(OBJS:.o=.d)

CFLAGS += -Wall -DNDEBUG -g \
	-I. \
	-Icore \
	-Idata \
	-Ievent_loop \
	-Iglue/linux

SRC = \
	core/alloc.c \
	core/assert.c \
	data/map.c \
	event_loop/event_loop_select.c \
	data_buffer.c \
	proxy_types.c \
	tcp_striper.c

OBJS = $(patsubst %,_obj/%, $(subst .c,.o,$(SRC)))

all: tcp_striper

-include $(OBJS:.o=.d)

_obj/%.o: %.c
	@echo c: $<
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $*.c > _obj/$*.d
	@sed -e 's/^/_obj\//' < _obj/$*.d > _obj/$*.dd
	@mv _obj/$*.dd _obj/$*.d

tcp_striper: $(OBJS)
	@echo l: $@
	@$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f tcp_striper $(OBJS) $(OBJS:.o=.d)

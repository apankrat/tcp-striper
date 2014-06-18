CFLAGS += -Wall -DNDEBUG -g \
	-I. \
	-Icore/inc \
	-Idata/inc \
	-Ievents/inc \
	-Ipipes/inc \
	-Isystem/inc.linux \
	-Isystem/inc

SRC = \
	core/src/alloc.c \
	core/src/assert.c \
	data/src/map.c \
	events/src.linux/event_loop_select.c \
	pipes/src/io_bridge.c \
	pipes/src/io_buffer.c \
	pipes/src/io_pipe_tcp.c \
	system/src.linux/termio.c \
	system/src/socket_utils.c \
	main.c 
#	connection.c \
#	connection_tcp.c 
#	core/src/alloc.c \
	core/src/assert.c \
	data/src/map.c \
	event_loop/src.linux/event_loop_select.c \
	data_buffer.c \
	proxy_types.c \
	socket_utils.c \
	main.c

OBJS = $(patsubst %,_obj/%, $(subst .c,.o,$(SRC)))

all: tcp-striper

-include $(OBJS:.o=.d)

_obj/%.o: %.c
	@echo c: $<
	@mkdir -p _obj/$(dir $<)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $*.c > _obj/$*.d
	@sed -e 's/^/_obj\//' < _obj/$*.d > _obj/$*.dd
	@mv _obj/$*.dd _obj/$*.d

tcp-striper: $(OBJS)
	@echo l: $@
	@$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -rf tcp-striper _obj

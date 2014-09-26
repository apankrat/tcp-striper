#CFLAGS += -O2

CFLAGS += -Wall -DNDEBUG -g \
	-I. \
	-Icore/inc.linux \
	-Icore/inc \
	-Idata/inc \
	-Ievl/inc \
	-Iio/inc \
	-Isys/inc.linux \
	-Isys/inc

SRC = \
	core/src/alloc.c \
	core/src/assert.c \
	data/src/map.c \
	evl/src.linux/event_loop_select.c \
	io/src/io_bridge.c \
	io/src/io_buffer.c \
	io/src/io_pipe_atx.c \
	io/src/io_pipe_dgm.c \
	io/src/io_pipe_tcp.c \
	io/src/io_serialize.c \
	sys/src.linux/termio.c \
	sys/src/socket_utils.c

EXE = \
	tcp-proxy \
	tcp-relay \
	tests/test-serialize

all: $(EXE)

#
#	the build machinery
#
OBJ = $(patsubst %,_obj/%, $(subst .c,.o,$(SRC)))

EXE_SRC = $(patsubst %,%.c, $(EXE))
EXE_OBJ = $(patsubst %,_obj/%, $(subst .c,.o,$(EXE_SRC)))

-include $(OBJ:.o=.d) $(EXE_OBJ:.o=.d)

#

_obj/%.o: %.c
	@echo c: $<
	@mkdir -p _obj/$(dir $<)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $*.c > _obj/$*.d
	@sed -e 's/^/_obj\//' < _obj/$*.d > _obj/$*.dd
	@mv _obj/$*.dd _obj/$*.d

$(EXE): $(OBJ) $(EXE_OBJ)
	@echo l: $@
	@$(CC) $(LDFLAGS) $(OBJ) $(patsubst %,_obj/%.o,$@) -o $@

clean:
	rm -rf _obj $(EXE)

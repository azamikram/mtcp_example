include mk/mtcp.mk

MKDIR_P = mkdir -p

BUILD_DIR=build

APP=client
SRCS=main.c

OBJS=$(SRCS:.c=.o)
F_OBJS=$(addprefix $(BUILD_DIR)/, $(notdir $(OBJS)))
DIRS=$(BUILD_DIR)

CC=gcc -g -O3 -Wall -Werror -fgnu89-inline

CFLAGS += -DAPP_E

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

all: $(DIRS) $(APP)

$(DIRS):
	$(MSG) "   MKDIR $@"
	$(HIDE) ${MKDIR_P} $@

$(OBJS): %.o: %.c
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC} -o $(BUILD_DIR)/$(@F)

$(APP): $(OBJS) ${MTCP_FLD}/lib/libmtcp.a
	$(MSG) "   LD $<"
	$(HIDE) ${CC} ${F_OBJS} ${LIBS} ${UTIL_OBJ} -o $(BUILD_DIR)/$@

clean:
	$(MSG) "   CLEAN $(APP) $(F_OBJS)"
	$(HIDE) rm -f *~ ${APP} ${F_OBJS} log_*

	$(MSG) "   CLEAN $(DIRS)"
	$(HIDE) rm -rf   ${DIRS}

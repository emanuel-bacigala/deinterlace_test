BIN = main
OBJS= $(BIN).o

CFLAGS+=-Wall -O2
LDFLAGS+=-L/opt/vc/lib/ -L/opt/vc/src/hello_pi/libs/ilclient
LDFLAGS+=-lbcm_host -lopenmaxil -lvcos -lvchiq_arm -lilclient
LDFLAGS+=-lGLESv2 -lEGL -lX11 -lXext
LDFLAGS+=-lrt -lpthread

INCLUDES+=-I /opt/vc/include/IL -I /opt/vc/src/hello_pi/libs/ilclient -I/opt/vc/include
INCLUDES+=-I /opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
INCLUDES+=-DRASPBERRY_PI  -DSTANDALONE -D__STDC_CONSTANT_MACROS \
   -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC \
   -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
   -U_FORTIFY_SOURCE -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT \
   -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST\
   -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM

all: $(BIN)

%.o: %.c
	@rm -f $@
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@

$(BIN): $(OBJS)
	$(CC) -s -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

clean:
	@rm -f $(OBJS)
	@rm -f $(BIN)


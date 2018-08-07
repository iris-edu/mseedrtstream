
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use


MAJOR_VER = 1
MINOR_VER = 7
CURRENT_VER = $(MAJOR_VER).$(MINOR_VER)
COMPAT_VER = $(MAJOR_VER).$(MINOR_VER)

LIB_SRCS = timeutils.c genutils.c strutils.c \
           logging.c network.c statefile.c config.c \
           portable.c connection.c

LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_DOBJS = $(LIB_SRCS:.c=.lo)

LIB_A = libdali.a
LIB_SO_FILENAME = libdali.so
LIB_SO_ALIAS = $(LIB_SO_FILENAME).$(MAJOR_VER)
LIB_SO = $(LIB_SO_FILENAME).$(CURRENT_VER)
LIB_DYN_ALIAS = libdali.dylib
LIB_DYN = libdali.$(CURRENT_VER).dylib

all: static

static: $(LIB_A)

shared: $(LIB_SO)

dynamic: $(LIB_DYN)

$(LIB_A): $(LIB_OBJS)
	rm -f $(LIB_A)
	ar -crs $(LIB_A) $(LIB_OBJS)

$(LIB_SO): $(LIB_DOBJS)
	rm -f $(LIB_SO) $(LIB_SO_ALIAS) $(LIB_SO_FILENAME)
	$(CC) $(CFLAGS) -shared -Wl,-soname -Wl,$(LIB_SO_ALIAS) -o $(LIB_SO) $(LIB_DOBJS)
	ln -s $(LIB_SO) $(LIB_SO_ALIAS)
	ln -s $(LIB_SO) $(LIB_SO_FILENAME)

$(LIB_DYN): $(LIB_DOBJS)
	rm -f $(LIB_DYN) $(LIB_DYN_ALIAS)
	$(CC) $(CFLAGS) -dynamiclib -compatibility_version $(COMPAT_VER) -current_version $(CURRENT_VER) -install_name $(LIB_DYN_ALIAS) -o $(LIB_DYN) $(LIB_DOBJS)
	ln -sf $(LIB_DYN) $(LIB_DYN_ALIAS)

clean:
	rm -f $(LIB_OBJS) $(LIB_DOBJS) $(LIB_A) $(LIB_SO) $(LIB_SO_ALIAS) \
	      $(LIB_SO_FILENAME) $(LIB_DYN) $(LIB_DYN_ALIAS)

install:
	@echo
	@echo "No install method, copy the library, header files, and"
	@echo "documentation to the preferred install location"
	@echo

.SUFFIXES: .c .o .lo

# Standard object building
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# Standard object building for dynamic library components using -fPIC
.c.lo:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

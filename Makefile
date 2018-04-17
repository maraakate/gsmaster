CXX = g++

# default to Linux
OSTYPE=linux

# default to x86 architecture
MACHTYPE=x86

# whether to use CURL for HTTP List DL
USE_CURL=yes

ifeq ($(DEBUG),1)
CFLAGS = -g -O0
else
CFLAGS = -O2 -fomit-frame-pointer
endif

CFLAGS +=-Wall -Werror

WCFLAGS =
ifeq ($(OSTYPE), linux)
WLFLAGS = -ldl -lrt
endif

ifeq ($(USE_CURL),yes)
CFLAGS+=-DUSE_CURL
CURLCFLAGS = -Ilibcurl/include
CURLLFLAGS = -L$(OSTYPE)/$(MACHTYPE) -lcurl
endif

SERVER = master.o \
	gamestable.o \
	dk_essentials.o \
	gsmalg.o \
	curl_dl.o


OBJECTS = $(SERVER)

all: gsmaster

clean:
	rm -f *.o

gsmaster: $(OBJECTS)
	$(CXX) $(OBJECTS) $(CURLLFLAGS) -lm $(WLFLAGS) -o gsmaster

%.o: %.cpp
	$(CXX) $(CFLAGS) $(WCFLAGS) $(CURLCFLAGS) -c $< -o $@

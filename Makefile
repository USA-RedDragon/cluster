CC = gcc
CFLAGS = -Wall -Werror -Wno-unused-parameter -I/opt/vc/include -lshapes -lm -laldl -lpigpio -lpthread -lrt -L/opt/vc/lib -lbrcmOpenVG -lbrcmEGL

V = @

all: cluster

cluster: cluster.c
	@echo + cc cluster.c
	$(V)$(CC) $(CFLAGS) -o $@ cluster.c

clean:
	@echo + clean
	$(V)rm -rf cluster

install:
	@echo + install cluster to /usr/bin/cluster
	$(V)cp cluster /usr/bin/cluster

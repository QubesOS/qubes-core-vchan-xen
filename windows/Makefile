OUTDIR = bin/$(ARCH)
CFLAGS += -Iinclude
LDFLAGS += -lxencontrol

all: $(OUTDIR) $(OUTDIR)/libvchan.dll

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/libvchan.dll: src/dllmain.c src/io.c src/init.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -lxenvchan -shared -o $@

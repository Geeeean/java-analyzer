CC = gcc
CFLAGS = -I$(IDIR)

CONFIG = analyzer.conf
SRCDIR = src
IDIR = include
BUILDDIR = build
BINDIR = bin
LDIR = lib
LIBS = 

TARGET = analyzer

DEPS := $(wildcard $(IDIR)/*.h)

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJ := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	@mkdir -p $(BUILDDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	@cp $(CONFIG) $(BINDIR)/
	$(CC) -o $(BINDIR)/$@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(BUILDDIR)/*.o *~ core $(IDIR)/*~ 

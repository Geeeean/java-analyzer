CC = gcc
CFLAGS = -I$(IDIR)

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
	@$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	@$(CC) -o $(BINDIR)/$@ $^ $(CFLAGS) $(LIBS)
	@echo Build complete, the binary file is in ./bin

.PHONY: clean

clean:
	@rm -f $(BUILDDIR)/*.o *~ core $(IDIR)/*~ 
	@echo Cleaned: ./build ./bin 

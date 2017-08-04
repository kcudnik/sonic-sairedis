WARNINGS = \
	-Wall \
	-Wcast-align \
	-Wcast-qual \
	-Wconversion \
	-Wdisabled-optimization \
	-Werror \
	-Wextra \
	-Wextra \
	-Wfloat-equal \
	-Wformat-nonliteral \
	-Wformat-security \
	-Wformat-y2k \
	-Wformat=2 \
	-Wimport \
	-Winit-self \
	-Winline \
	-Winvalid-pch \
	-Wlong-long \
	-Wmissing-field-initializers \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wno-aggregate-return \
	-Wno-missing-noreturn \
	-Wno-padded \
	-Wno-switch-enum \
	-Wno-unused-parameter \
	-Wpacked \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wshadow \
	-Wstack-protector \
	-Wstrict-aliasing=3 \
	-Wswitch \
	-Wswitch-default \
	-Wunreachable-code \
	-Wunused \
	-Wvariadic-macros \
	-Wwrite-strings

all: closefd

closefd: closefd.c
	gcc -g $(WARNINGS) closefd.c -o closefd

.PHONY: clean

clean:
	rm -f *.o closefd *.rec *~ .*.swp *.rec

# objcopy -O binary closefd.o binfile

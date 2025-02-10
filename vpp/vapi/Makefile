
all: test

test: *.c *.h
	 gcc -o test vapi.c vapi_c_test.c \
		 -I ../../ \
		 -lvlib -lvlibapi -lvppapiclient -lvlibmemoryclient -lvppinfra -lpthread -lsvm \
		 -lcheck -lm -lrt -lsubunit


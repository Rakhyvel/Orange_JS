run:
	gcc *.c util/*.c -Wall -o BloodOrange
	./BloodOrange test/*.orng
verbose:
	gcc *.c util/*.c -Wall -o BloodOrange -DVERBOSE
	./BloodOrange test/*.orng
clean:
	rm BloodOrange
run:
	gcc *.c util/*.c -Wall -o BloodOrange
	./BloodOrange test/*.orng

verbose:
	gcc orangec/*.c util/*.c -Wall -o BloodOrange -DVERBOSE
	./BloodOrange test/*.orng

git-commit:
	git add .
	git commit -m "$(msg)"

git-push:
	git push origin master

clean:
	rm BloodOrange
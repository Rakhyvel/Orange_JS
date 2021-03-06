run:
	gcc Orangec/*.c util/*.c -Wall -o orangec
	./orangec test/*.orng

verbose:
	gcc Orangec/*.c util/*.c -Wall -o orangec -DVERBOSE
	./orangec test/*.orng

git-commit:
	git add .
	git commit -m "$(msg)"

git-push:
	git push origin master

clean:
	rm orangec
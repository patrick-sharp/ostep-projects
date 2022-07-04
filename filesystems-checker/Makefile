all: xcheck create-test-imgs

create-test-imgs: create-test-imgs.c
	gcc -o create-test-imgs create-test-imgs.c -Wall -Werror -O

xcheck: xcheck.c
	gcc -o xcheck xcheck.c -Wall -Werror -O

clean:
	rm xcheck create-test-imgs tests/*.img tests-out/* && rmdir tests-out

test:
	make && ./create-test-imgs && ./test-xcheck.sh

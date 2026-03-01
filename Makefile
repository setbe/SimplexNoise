CC=clang

.PHONY: clean

example: example.cpp
	$(CC) -std=c++14 $^ -o $@ -lm -lstdc++

clean:
	rm -rf example

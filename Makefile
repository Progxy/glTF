debug: example.c
	gcc -std=c11 -Wall -Wextra -pedantic -g -D"_DEBUG_MODE_" example.c -o out/example

gltf: example.c
	gcc -std=c11 -Wall -Wextra example.c -o out/example
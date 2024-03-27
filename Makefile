debug: example.c
	gcc -std=c11 -Wall -Wextra -pedantic -g -D"_DEBUG_MODE_" example.c -o out/example

gltf: example.c
	gcc -std=c11 -Wall -Wextra example.c -o out/example

gltf-lib: include/gltf_loader.c
	gcc -std=c11 -Wall -Wextra -pedantic -fPIC -shared -D"_GLTF_LIB_" include/gltf_loader.c -o out/libgltf.so 

gltf-lib-debug: include/gltf_loader.c
	gcc -std=c11 -Wall -Wextra -pedantic -fPIC -shared -g -D"_DEBUG_MODE_" -D"_GLTF_LIB_" include/gltf_loader.c -o out/libgltf.so 
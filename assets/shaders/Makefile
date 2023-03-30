FRAGMENT_SHADER=$(addsuffix .frag.spv, $(basename $(wildcard *.frag)))
VERTEX_SHADER=$(addsuffix .vert.spv, $(basename $(wildcard *.vert)))
COMPUTE_SHADER=$(addsuffix .comp.spv, $(basename $(wildcard *.comp)))

%.frag.spv: %.frag
	glslc $< -o $@ -I./shader_utils

%.vert.spv: %.vert
	glslc $< -o $@ -I./shader_utils

%.comp.spv: %.comp
	glslc $< -o $@ -I./shader_utils

shaders : $(VERTEX_SHADER) $(FRAGMENT_SHADER) $(COMPUTE_SHADER)

clean :
	rm -f $(VERTEX_SHADER) $(FRAGMENT_SHADER) $(COMPUTE_SHADER)

.PHONY: shaders

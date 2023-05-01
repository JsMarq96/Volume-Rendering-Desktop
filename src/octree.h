#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <stdint.h>

#include "gigavoxels.h"
#include "gl3w.h"
#include "glcorearb.h"
#include "shader.h"
#include "texture.h"


namespace Octree {

    #define NON_LEAF 0
    #define FULL_LEAF 1
    #define ENPTY_LEAD 2

    struct sOctreeNode {
        uint32_t is_leaf;
        uint32_t child_indexes;
        uint32_t __padding[2];
    };

    struct sGPUOctree {
        uint32_t  SSBO;
        size_t    element_size;
        size_t    byte_size;

        inline void bind(const uint32_t location) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, location, SSBO);
        }
    };

    inline void generate_octree_from_3d_texture(const sTexture &volume_texture, 
                                                sGPUOctree *octree_to_fill) {
        // TODO only square textures
        // Compute the total size of the octree
        size_t element_size = 0;
        size_t number_of_levels = log2(volume_texture.width);
        uint32_t levels_start_index[10];
        for(uint32_t i = 0; i <= number_of_levels; i++) {
            levels_start_index[i] = element_size;
            
            size_t size_of_level = (uint32_t) pow(2, i * 3);
            element_size += size_of_level;
            std::cout << i << " starts at " << levels_start_index[i] << " with size of " << size_of_level << std::endl;
        }

        size_t octree_bytesize = sizeof(sOctreeNode) * element_size;

        std::cout << "Total size " << element_size << std::endl;


        // Generate SSBO and the shaders
        uint32_t SSBO;
        glGenBuffers(1, &SSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, octree_bytesize, NULL, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, SSBO);

        sShader compute_first_pass = {}, compute_n_pass = {}, compute_final_pass = {};
        compute_first_pass.load_file_compute_shader("resources/shaders/octree_load.cs");
        compute_n_pass.load_file_compute_shader("resources/shaders/octree_generate.cs");
        compute_final_pass.load_file_compute_shader("resources/shaders/octree_generate_tip.cs");

        // First compute pass: Upload the data from the texture
        glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, volume_texture.texture_id);

        std::cout << "start at " << levels_start_index[number_of_levels] << std::endl;

        uint32_t dispatch_size = volume_texture.width/2;
        compute_first_pass.activate();
        compute_first_pass.set_uniform_texture("u_volume_map", 0);
        compute_first_pass.set_uniform("u_layer_start", levels_start_index[number_of_levels]);
        compute_first_pass.set_uniform("u_img_dimm", (uint32_t)volume_texture.width/2);
        compute_first_pass.dispatch(dispatch_size,dispatch_size,dispatch_size,
                                    true);
        compute_first_pass.deactivate();

        //return;
        for(uint32_t i = number_of_levels-1; i >= 1; i--) {
            compute_n_pass.activate();
            compute_n_pass.set_uniform("u_curr_layer_start", levels_start_index[i]);
            compute_n_pass.set_uniform("u_prev_layer_start", levels_start_index[i+1]);

            uint32_t curr_size = (uint32_t) pow(2, i * 3);
            uint32_t prev_size = (uint32_t) pow(2, (i+1) * 3);
            
            uint32_t local_dispatch_size = (curr_size / 8.0f);
            uint32_t dispatch_size = std::ceil(pow(local_dispatch_size, 1.0f/ 3.0f));

            std::cout << "Dispatch size " << (uint32_t) dispatch_size << " Curr size: " << curr_size << " layer_start: " << levels_start_index[i] << std::endl;
            compute_n_pass.set_uniform_vector("u_curr_layer_size", glm::vec3(curr_size, curr_size, curr_size));
            compute_n_pass.set_uniform_vector("u_prev_layer_size", glm::vec3(prev_size, prev_size, prev_size));

            compute_n_pass.dispatch((uint32_t) local_dispatch_size,1,1,
                                     // + (uint32_t) ceil(glm::mod(local_dispatch_size, (float)dispatch_size)),
                                    true);

            compute_n_pass.deactivate();

        }

        compute_final_pass.activate(); 
        compute_final_pass.dispatch((uint32_t) 1,1,1, true);
        compute_final_pass.deactivate();

        std::cout << "Finished" << std::endl;

        octree_to_fill->SSBO = SSBO;
        //octree_to_fill->byte_size = octree_byte_size;
    }


    inline void create_test_octree_two_layers(sGPUOctree *to_fill) {
        size_t octree_node_size = 1 + 8;
        size_t octree_byte_size = octree_node_size * sizeof(sOctreeNode);

        sOctreeNode* octree = (sOctreeNode*) malloc(octree_byte_size);


        for(uint32_t i = 0; i < 8;i++) {
            octree[1 + i].is_leaf = (i % 2) + 1;
            std::cout << 1 + i << " is " << (i % 2) + 1 << std::endl;
        }
        // Fill the data
        octree[0].is_leaf = NON_LEAF;
        octree[0].child_indexes = 1;
        octree[2].is_leaf = FULL_LEAF;
        

        // Upload to the GPU
        uint32_t SSBO;
        glGenBuffers(1, &SSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, octree_byte_size, octree, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        free(octree);

        to_fill->SSBO = SSBO;
        to_fill->byte_size = octree_byte_size;
    }

};
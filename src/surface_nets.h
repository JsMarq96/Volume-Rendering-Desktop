#pragma once

#include <iostream>
#include <cmath>
#include <stdint.h>

#include "gl3w.h"
#include "glcorearb.h"
#include "shader.h"
#include "texture.h"

// https://github.com/mikolalysenko/mikolalysenko.github.com/blob/master/Isosurface/js/surfacenets.js
namespace SurfaceNets {
    const uint32_t MAX_VERTEX_COUNT = 2000;

    struct sSurfacesPoint {
        glm::vec3 position;
        int is_surface;
        //glm::vec3 normal;
        //float padding2;
    };

    struct sRawMesh {
        glm::vec3 pad;
        int vertices_count = 0;
        sSurfacesPoint vertices[];
    };

    struct sGenerator {
        sRawMesh *vertices;

        sSurfacesPoint* surface_points = NULL;

        uint32_t mesh_vertices_SSBO = 0;

        sShader  mesh_vertex_finder = {};

        void generate_from_volume(const sTexture &volume_texture, 
                                  const uint32_t sampling_rate) {
            mesh_vertex_finder.load_file_compute_shader("..\\resources\\shaders\\surface_find.cs");


            uint32_t max_vertex_count = sampling_rate * sampling_rate * sampling_rate;
            size_t vertices_byte_size = sizeof(sSurfacesPoint) * max_vertex_count;
                        
            glGenBuffers(1, &mesh_vertices_SSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, mesh_vertices_SSBO);
            // Allocate memory
            glBufferData(GL_SHADER_STORAGE_BUFFER, vertices_byte_size, NULL, GL_DYNAMIC_COPY);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh_vertices_SSBO);

            glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, volume_texture.texture_id);

            mesh_vertex_finder.activate();
            mesh_vertex_finder.set_uniform_texture("u_volume_map", 0);
            mesh_vertex_finder.dispatch(sampling_rate, 
                                        sampling_rate,
                                        sampling_rate, 
                                        true);
			mesh_vertex_finder.deactivate();

            // Get the vertices back from the GPU memmory
            surface_points = (sSurfacesPoint*) malloc(vertices_byte_size);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, vertices_byte_size, surface_points);
        }
    };

};
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "assets.hpp"

#include "primitives.hpp"

void createTessellatedGridMesh(Mesh* g, glm::uvec2 tiles) {
    Mesh w;
    g->num_vertices = tiles.x * tiles.y * 6;
    g->attributes = Mesh::Attributes::VERTICES | Mesh::Attributes::UVS;
    g->vertices = reinterpret_cast<decltype(g->vertices)>(malloc(sizeof(*g->vertices) * g->num_vertices));
    g->uvs = reinterpret_cast<decltype(g->uvs)>(malloc(sizeof(*g->uvs) * g->num_vertices));

    glm::vec3 off(tiles.x*0.5, 0.0, tiles.y*0.5);
    auto size = 1.0f / glm::vec2(tiles);
    int i = 0;
    for (int x = 0; x < tiles.x; x++) {
        for (int z = 0; z < tiles.y; z++) {
            auto bl = glm::fvec2(x, z);
            auto tr = glm::fvec2(x, z) + glm::vec2(1);

            // Bottom left triangle
            g->vertices[i] = glm::vec3(bl.x, 0.0, bl.y) - off;
            g->uvs[i] = glm::vec2(bl.x, bl.y) * size;
            i++;

            g->vertices[i] = glm::vec3(bl.x, 0.0, tr.y) - off;
            g->uvs[i] = glm::vec2(bl.x, tr.y) * size;
            i++;

            g->vertices[i] = glm::vec3(tr.x, 0.0, bl.y) - off;
            g->uvs[i] = glm::vec2(tr.x, bl.y) * size;
            i++;

            // Top right triangle
            g->vertices[i] = glm::vec3(bl.x, 0.0, tr.y) - off;
            g->uvs[i] = glm::vec2(bl.x, tr.y) * size;
            i++;

            g->vertices[i] = glm::vec3(tr.x, 0.0, tr.y) - off;
            g->uvs[i] = glm::vec2(tr.x, tr.y) * size;
            i++;

            g->vertices[i] = glm::vec3(tr.x, 0.0, bl.y) - off;
            g->uvs[i] = glm::vec2(tr.x, bl.y) * size;
            i++;
        }
    }

    g->num_submeshes = 1;
    g->draw_mode = GL_PATCHES;
    g->draw_start = reinterpret_cast<decltype(g->draw_start)>(malloc(sizeof(*g->draw_start) * g->num_submeshes));
    g->draw_count = reinterpret_cast<decltype(g->draw_count)>(malloc(sizeof(*g->draw_count) * g->num_submeshes));
    g->draw_start[0] = 0;
    g->draw_count[0] = g->num_vertices;
    createMeshVao(g);
}

void createQuadMesh(Mesh* q) {
    q->num_vertices = 4;
    q->attributes = Mesh::Attributes::VERTICES | Mesh::Attributes::UVS;
    q->vertices = reinterpret_cast<decltype(q->vertices)>(malloc(sizeof(*q->vertices) * q->num_vertices));
    q->uvs      = reinterpret_cast<decltype(q->uvs     )>(malloc(sizeof(*q->uvs     ) * q->num_vertices));

    // @hardcoded
    q->vertices[0] = glm::vec3(-1.0f, 1.0f, 0.0f),
    q->vertices[1] = glm::vec3(-1.0f, -1.0f, 0.0f),
    q->vertices[2] = glm::vec3(1.0f, 1.0f, 0.0f),
    q->vertices[3] = glm::vec3(1.0f, -1.0f, 0.0f),

    // @hardcoded
    q->uvs[0] = glm::vec2(0.0f, 1.0f);
    q->uvs[1] = glm::vec2(0.0f, 0.0f);
    q->uvs[2] = glm::vec2(1.0f, 1.0f);
    q->uvs[3] = glm::vec2(1.0f, 0.0f);

    q->num_submeshes = 1;
    q->draw_mode = GL_TRIANGLE_STRIP;
    q->draw_start = reinterpret_cast<decltype(q->draw_start)>(malloc(sizeof(*q->draw_start) * q->num_submeshes));
    q->draw_count = reinterpret_cast<decltype(q->draw_count)>(malloc(sizeof(*q->draw_count) * q->num_submeshes));
    q->draw_start[0] = 0;
    q->draw_count[0] = q->num_vertices;
    createMeshVao(q);
}

void createCubeMesh(Mesh* c) {
    // @hardcoded
    static const float vertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    c->num_vertices = sizeof(vertices) / (3 * sizeof(*vertices));
    c->attributes = Mesh::Attributes::VERTICES;
    c->vertices = reinterpret_cast<decltype(c->vertices)>(malloc(sizeof(*c->vertices) * c->num_vertices));
    for (uint64_t i = 0; i < c->num_vertices; i++) {
        c->vertices[i] = glm::vec3(vertices[3 * i], vertices[3 * i + 1], vertices[3 * i + 2]);
    }

    c->num_submeshes = 1;
    c->draw_mode = GL_TRIANGLES;
    c->draw_start = reinterpret_cast<decltype(c->draw_start)>(malloc(sizeof(*c->draw_start) * c->num_submeshes));
    c->draw_count = reinterpret_cast<decltype(c->draw_count)>(malloc(sizeof(*c->draw_count) * c->num_submeshes));
    c->draw_start[0] = 0;
    c->draw_count[0] = c->num_vertices;
    createMeshVao(c);
}

void createLineCubeMesh(Mesh* c) {
    // @hardcoded
    static const float vertices[] = {
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
    };

    c->num_vertices = sizeof(vertices) / (3 * sizeof(*vertices));
    c->attributes = Mesh::Attributes::VERTICES;
    c->vertices = reinterpret_cast<decltype(c->vertices)>(malloc(sizeof(*c->vertices) * c->num_vertices));
    for (uint64_t i = 0; i < c->num_vertices; i++) {
        c->vertices[i] = glm::vec3(vertices[3 * i], vertices[3 * i + 1], vertices[3 * i + 2]);
    }

    c->num_submeshes = 1;
    c->draw_mode = GL_LINES;
    c->draw_start = reinterpret_cast<decltype(c->draw_start)>(malloc(sizeof(*c->draw_start) * c->num_submeshes));
    c->draw_count = reinterpret_cast<decltype(c->draw_count)>(malloc(sizeof(*c->draw_count) * c->num_submeshes));
    c->draw_start[0] = 0;
    c->draw_count[0] = c->num_vertices;
    createMeshVao(c);
}
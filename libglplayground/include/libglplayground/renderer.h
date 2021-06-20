#pragma once
#include "ref.h"
#include "shader.h"
#include "vertex_array_object.h"
#include "vertex_buffer_object.h"
#include "element_buffer_object.h"
#include "texture.h"
namespace libplayground {
    namespace gl {
        struct vertex {
            glm::vec3 pos, normal;
            glm::vec2 uv;
        };
        struct texture_descriptor {
            ref<texture> data;
            std::string uniform_name;
        };
        struct mesh {
            glm::mat4 transform;
            std::vector<vertex> vertices;
            std::vector<uint32_t> indices;
            std::vector<texture_descriptor> textures;
        };
        class renderer : public ref_counted {
        public:
            void reset();
            void set_shader(ref<shader> shader);
            ref<shader> get_shader();
            void submit(const mesh& m);
            void render();
        private:
            struct assembled_mesh {
                std::vector<texture_descriptor> textures;
                ref<vertex_array_object> vao;
                ref<vertex_buffer_object> vbo;
                ref<element_buffer_object> ebo;
                glm::mat4 transform;
            };
            ref<shader> m_shader;
            std::vector<assembled_mesh> m_meshes;
        };
    }
}
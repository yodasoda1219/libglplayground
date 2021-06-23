#include "libglppch.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "vertex_array_object.h"
#include "vertex_buffer_object.h"
#include "element_buffer_object.h"
#include "model.h"
namespace libplayground {
    namespace gl {
        static glm::mat4 from_assimp_matrix(const aiMatrix4x4& matrix) {
            glm::mat4 result;
            result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
            result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
            result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
            result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
            return result;
        }
        template<size_t L, typename T> static glm::vec<L, float> from_assimp_vector(const T& vector) {
            glm::vec<L, float> result;
            for (size_t i = 0; i < L; i++) {
                result[i] = vector[i];
            }
            return result;
        }
        static glm::quat from_assimp_quaternion(const aiQuaternion& quat) {
            return glm::quat(quat.w, quat.x, quat.y, quat.z);
        }
        static constexpr uint32_t model_import_flags =
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_LimitBoneWeights;
        struct log_stream : public Assimp::LogStream {
            static void initialize() {
                if (Assimp::DefaultLogger::isNullLogger()) {
                    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
                    Assimp::DefaultLogger::get()->attachStream(new log_stream, Assimp::Logger::Err | Assimp::Logger::Warn);
                }
            }
            virtual void write(const char* message) override {
                spdlog::error("Assimp: " + std::string(message));
            }
        };
        model::model(const std::string& path) {
            this->m_file_path = path;
            log_stream::initialize();
            spdlog::info("Loading model from: " + this->m_file_path);
            this->m_importer = std::make_unique<Assimp::Importer>();
            this->m_scene = this->m_importer->ReadFile(this->m_file_path, model_import_flags);
            if (!this->m_scene || !this->m_scene->HasMeshes()) {
                throw std::runtime_error("Could not load model from: " + this->m_file_path);
            }
            this->m_is_animated = this->m_scene->mAnimations != nullptr;
            // todo: get the model shader from the shader library, when thats implemented
            this->m_inverse_transform = glm::inverse(from_assimp_matrix(this->m_scene->mRootNode->mTransformation));
            uint32_t vertex_count = 0;
            uint32_t index_count = 0;
            this->m_meshes.reserve((size_t)this->m_scene->mNumMeshes);
            for (uint32_t m = 0; m < this->m_scene->mNumMeshes; m++) {
                aiMesh* mesh = this->m_scene->mMeshes[m];
                auto& mesh_ = this->m_meshes.emplace_back();
                mesh_.base_vertex = vertex_count;
                mesh_.base_index = index_count;
                mesh_.mesh_name = std::string(mesh->mName.C_Str());
                mesh_.vertex_count = mesh->mNumVertices;
                mesh_.index_count = mesh->mNumFaces * 3;
                mesh_.material_index = mesh->mMaterialIndex;
                vertex_count += mesh_.vertex_count;
                index_count += mesh_.index_count;
                if (!mesh->HasPositions()) {
                    throw std::runtime_error("This mesh does not have vertex positions!");
                }
                if (!mesh->HasNormals()) {
                    throw std::runtime_error("This mesh does not have normals!");
                }
                if (this->m_is_animated) {
                    for (size_t i = 0; i < (size_t)mesh->mNumVertices; i++) {
                        animated_vertex vertex;
                        vertex.m.pos = from_assimp_vector<3>(mesh->mVertices[i]);
                        vertex.m.normal = from_assimp_vector<3>(mesh->mNormals[i]);
                        if (mesh->HasTextureCoords(0)) {
                            vertex.m.uv = from_assimp_vector<2>(mesh->mTextureCoords[0][i]);
                        }
                        this->m_animated_vertices.push_back(vertex);
                    }
                } else {
                    for (size_t i = 0; i < (size_t)mesh->mNumVertices; i++) {
                        vertex v;
                        v.pos = from_assimp_vector<3>(mesh->mVertices[i]);
                        v.normal = from_assimp_vector<3>(mesh->mNormals[i]);
                        if (mesh->HasTextureCoords(0)) {
                            v.uv = from_assimp_vector<2>(mesh->mTextureCoords[0][i]);
                        }
                    }
                }
                for (size_t i = 0; i < (size_t)mesh->mNumFaces; i++) {
                    if (mesh->mFaces[i].mNumIndices != 3) {
                        throw std::runtime_error("Face does not have exactly 3 indices!");
                    }
                    index _i;
                    for (size_t j = 0; j < 3; j++) {
                        _i[j] = mesh->mFaces[i].mIndices[j];
                    }
                    this->m_indices.push_back(_i);
                    // todo: triangle cache
                }
            }
            this->traverse_nodes(this->m_scene->mRootNode);
            if (this->m_is_animated) {
                for (size_t m = 0; m < (size_t)this->m_scene->mNumMeshes; m++) {
                    aiMesh* mesh = this->m_scene->mMeshes[m];
                    auto& mesh_ = this->m_meshes[m];
                    for (size_t i = 0; i < (size_t)mesh->mNumBones; i++) {
                        aiBone* bone = mesh->mBones[i];
                        std::string bone_name = std::string(bone->mName.C_Str());
                        uint32_t bone_index = 0;
                        if (this->m_bone_map.find(bone_name) == this->m_bone_map.end()) {
                            bone_index = this->m_bone_count;
                            this->m_bone_count++;
                            this->m_bone_info.push_back(bone_info());
                            this->m_bone_info[bone_index].bone_offset = from_assimp_matrix(bone->mOffsetMatrix);
                            this->m_bone_map[bone_name] = bone_index;
                        } else {
                            bone_index = this->m_bone_map[bone_name];
                        }
                        for (size_t j = 0; j < (size_t)bone->mNumWeights; j++) {
                            uint32_t vertex_id = mesh_.base_index + bone->mWeights[j].mVertexId;
                            float weight = bone->mWeights[j].mWeight;
                            this->m_animated_vertices[vertex_id].add_bone_data(bone_index, weight);
                        }
                    }
                }
            }
            // todo: materials
            size_t stride = this->m_is_animated ? sizeof(animated_vertex) : sizeof(vertex);
            std::vector<vertex_attribute> attributes = {
                { GL_FLOAT, 3, stride, offsetof(vertex, pos), false },
                { GL_FLOAT, 3, stride, offsetof(vertex, normal), false },
                { GL_FLOAT, 2, stride, offsetof(vertex, uv), false }
            };
            this->m_vao = ref<vertex_array_object>::create();
            this->m_vao->bind();
            if (this->m_is_animated) {
                attributes.insert(attributes.end(), {
                    { GL_INT, 4, stride, offsetof(animated_vertex, ids), false },
                    { GL_FLOAT, 4, stride, offsetof(animated_vertex, weights), false }
                });
                this->m_vbo = ref<vertex_buffer_object>::create(this->m_animated_vertices);
            } else {
                this->m_vbo = ref<vertex_buffer_object>::create(this->m_static_vertices);
            }
            std::vector<uint32_t> indices;
            for (auto& index : this->m_indices) {
                for (size_t i = 0; i < 3; i++) {
                    indices.push_back(index[i]);
                }
            }
            this->m_ebo = ref<element_buffer_object>::create(indices);
            this->m_vao->unbind();
        }
        std::vector<animated_mesh>& model::get_meshes() {
            return this->m_meshes;
        }
        const std::vector<animated_mesh>& model::get_meshes() const {
            return this->m_meshes;
        }
        const std::vector<vertex>& model::get_static_vertices() const {
            return this->m_static_vertices;
        }
        const std::vector<index>& model::get_indices() const {
            return this->m_indices;
        }
        ref<shader> model::get_mesh_shader() {
            return this->m_shader;
        }
        std::vector<ref<texture>>& model::get_textures() {
            return this->m_textures;
        }
        const std::vector<ref<texture>>& model::get_textures() const {
            return this->m_textures;
        }
        const std::string& model::get_file_path() const {
            return this->m_file_path;
        }
        void model::bone_transform(float time) {
            this->read_node_hierarchy(time, this->m_scene->mRootNode, glm::mat4(1.f));
            this->m_bone_transforms.resize(this->m_bone_count);
            for (size_t i = 0; i < (size_t)this->m_bone_count; i++) {
                this->m_bone_transforms[i] = this->m_bone_info[i].final_transform;
            }
        }
        void model::read_node_hierarchy(float animation_time, const aiNode* node, const glm::mat4& parent_transform) {
            std::string name = std::string(node->mName.C_Str());
            const aiAnimation* animation = this->m_scene->mAnimations[0]; // first animation for now; todo: add animation id parameter
            glm::mat4 node_transform = from_assimp_matrix(node->mTransformation);
            const aiNodeAnim* node_animation = this->find_node_animation(animation, name);
            if (node_animation) {
                glm::vec3 translation = this->interpolate_translation(animation_time, node_animation);
                glm::mat4 translation_matrix = glm::translate(glm::mat4(1.f), translation);
                glm::quat rotation = this->interpolate_rotation(animation_time, node_animation);
                glm::mat4 rotation_matrix = glm::toMat4(rotation);
                glm::vec3 scale = this->interpolate_scale(animation_time, node_animation);
                glm::mat4 scale_matrix = glm::scale(glm::mat4(1.f), scale);
                node_transform = translation_matrix * rotation_matrix * scale_matrix;
            }
            glm::mat4 transform = parent_transform * node_transform;
            if (this->m_bone_map.find(name) != this->m_bone_map.end()) {
                uint32_t bone_index = this->m_bone_map[name];
                bone_info& bi = this->m_bone_info[(size_t)bone_index];
                bi.final_transform = this->m_inverse_transform * transform * bi.bone_offset;
            }
            for (uint32_t i = 0; i < node->mNumChildren; i++) {
                this->read_node_hierarchy(animation_time, node->mChildren[i], transform);
            }
        }
        void model::traverse_nodes(aiNode* node, const glm::mat4& parent_transform, uint32_t level) {
            glm::mat4 transform = parent_transform * from_assimp_matrix(node->mTransformation);
            this->m_node_map[node].resize(node->mNumMeshes);
            for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                uint32_t mesh = node->mMeshes[i];
                auto& mesh_ = this->m_meshes[mesh];
                mesh_.node_name = std::string(node->mName.C_Str());
                mesh_.transform = transform;
                this->m_node_map[node][i] = mesh;
            }
            for (uint32_t i = 0; i < node->mNumChildren; i++) {
                this->traverse_nodes(node->mChildren[i], transform, level + 1);
            }
        }
        const aiNodeAnim* model::find_node_animation(const aiAnimation* animation, const std::string& node_name) {
            for (uint32_t i = 0; i < animation->mNumChannels; i++) {
                const aiNodeAnim* node_animation = animation->mChannels[i];
                if (std::string(node_animation->mNodeName.C_Str()) == node_name) {
                    return node_animation;
                }
            }
            return nullptr;
        }
        uint32_t model::find_position(float animation_time, const aiNodeAnim* node_animation) {
            for (uint32_t i = 0; i < node_animation->mNumPositionKeys - 1; i++) {
                if (animation_time < (float)node_animation->mPositionKeys[i + 1].mTime) {
                    return i;
                }
            }
            return 0;
        }
        uint32_t model::find_rotation(float animation_time, const aiNodeAnim* node_animation) {
            for (uint32_t i = 0; i < node_animation->mNumRotationKeys - 1; i++) {
                if (animation_time < (float)node_animation->mRotationKeys[i + 1].mTime) {
                    return i;
                }
            }
            return 0;
        }
        uint32_t model::find_scale(float animation_time, const aiNodeAnim* node_animation) {
            for (uint32_t i = 0; i < node_animation->mNumScalingKeys - 1; i++) {
                if (animation_time < (float)node_animation->mScalingKeys[i + 1].mTime) {
                    return i;
                }
            }
            return 0;
        }
        glm::vec3 model::interpolate_translation(float animation_time, const aiNodeAnim* node_animation) {
            if (node_animation->mNumPositionKeys <= 1) {
                return from_assimp_vector<3>(node_animation->mPositionKeys[0].mValue);
            }
            uint32_t position_index = this->find_position(animation_time, node_animation);
            uint32_t next_position_index = position_index + 1;
            float delta_time = (float)(node_animation->mPositionKeys[next_position_index].mTime - node_animation->mPositionKeys[position_index].mTime);
            float factor = (delta_time - (float)node_animation->mPositionKeys[position_index].mTime) / delta_time;
            if (factor > 1.f) {
                throw std::runtime_error("Factor must be below 1!");
            }
            factor = glm::clamp(factor, 0.f, 1.f);
            const auto& start = node_animation->mPositionKeys[position_index].mValue;
            const auto& end = node_animation->mPositionKeys[next_position_index].mValue;
            auto delta = end - start;
            return from_assimp_vector<3>(start + factor * delta);
        }
        glm::quat model::interpolate_rotation(float animation_time, const aiNodeAnim* node_animation) {
            if (node_animation->mNumRotationKeys <= 1) {
                return from_assimp_quaternion(node_animation->mRotationKeys[0].mValue);
            }
            uint32_t rotation_index = this->find_rotation(animation_time, node_animation);
            uint32_t next_rotation_index = rotation_index + 1;
            float delta_time = (float)(node_animation->mRotationKeys[next_rotation_index].mTime - node_animation->mRotationKeys[rotation_index].mTime);
            float factor = (delta_time - (float)node_animation->mRotationKeys[rotation_index].mTime) / delta_time;
            if (factor > 1.f) {
                throw std::runtime_error("Factor must be below 1!");
            }
            factor = glm::clamp(factor, 0.f, 1.f);
            const auto& start = node_animation->mRotationKeys[rotation_index].mValue;
            const auto& end = node_animation->mRotationKeys[next_rotation_index].mValue;
            aiQuaternion q;
            aiQuaternion::Interpolate(q, start, end, factor);
            return from_assimp_quaternion(q.Normalize());
        }
        glm::vec3 model::interpolate_scale(float animation_time, const aiNodeAnim* node_animation) {
            if (node_animation->mNumScalingKeys <= 1) {
                return from_assimp_vector<3>(node_animation->mScalingKeys[0].mValue);
            }
            uint32_t scale_index = this->find_scale(animation_time, node_animation);
            uint32_t next_scale_index = scale_index + 1;
            float delta_time = (float)(node_animation->mScalingKeys[next_scale_index].mTime - node_animation->mScalingKeys[scale_index].mTime);
            float factor = (delta_time - (float)node_animation->mScalingKeys[scale_index].mTime) / delta_time;
            if (factor > 1.f) {
                throw std::runtime_error("Factor must be below 1!");
            }
            factor = glm::clamp(factor, 0.f, 1.f);
            const auto& start = node_animation->mScalingKeys[scale_index].mValue;
            const auto& end = node_animation->mScalingKeys[next_scale_index].mValue;
            auto delta = end - start;
            return from_assimp_vector<3>(start + factor * delta);
        }
    }
}
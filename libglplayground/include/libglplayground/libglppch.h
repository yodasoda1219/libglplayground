#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <entt/entt.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#ifdef BUILT_IMGUI
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#endif
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <utility>
#include <type_traits>
#include <stdexcept>
#include <typeinfo>
#include <cstdint>
#include <stddef.h> // for ::size_t
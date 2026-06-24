#include "Tucano/Asset/ModelLoader.h"
#include "Tucano/Core/Logger.h"
#include "Tucano/ECS/Components.h"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "ufbx/ufbx.h"

namespace Tucano {

bool ModelLoader::LoadGLTF(const std::string& filepath, World& world, VulkanContext* context) {
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

    if (result != cgltf_result_success) {
        TUCANO_CORE_ERROR("Failed to load glTF file: {0}", filepath);
        return false;
    }

    result = cgltf_load_buffers(&options, data, filepath.c_str());
    if (result != cgltf_result_success) {
        TUCANO_CORE_ERROR("Failed to load glTF buffers for: {0}", filepath);
        cgltf_free(data);
        return false;
    }

    TUCANO_CORE_INFO("Successfully loaded glTF: {0} (Meshes: {1}, Nodes: {2})", 
        filepath, data->meshes_count, data->nodes_count);

    // Simple implementation: extract all meshes as flat entities
    for (size_t i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* mesh = &data->meshes[i];

        for (size_t p = 0; p < mesh->primitives_count; ++p) {
            cgltf_primitive* primitive = &mesh->primitives[p];

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;
            cgltf_accessor* texAccessor = nullptr;
            size_t vertexCount = 0;

            for (size_t a = 0; a < primitive->attributes_count; ++a) {
                cgltf_attribute* attribute = &primitive->attributes[a];
                if (attribute->type == cgltf_attribute_type_position) {
                    posAccessor = attribute->data;
                    vertexCount = posAccessor->count;
                } else if (attribute->type == cgltf_attribute_type_normal) {
                    normAccessor = attribute->data;
                } else if (attribute->type == cgltf_attribute_type_texcoord) {
                    texAccessor = attribute->data;
                }
            }

            if (posAccessor) {
                for (size_t v = 0; v < vertexCount; ++v) {
                    Vertex vertex{};
                    
                    float pos[3] = {0,0,0};
                    cgltf_accessor_read_float(posAccessor, v, pos, 3);
                    vertex.Position = { pos[0], pos[1], pos[2] };
                    
                    if (normAccessor) {
                        float norm[3] = {0,0,0};
                        cgltf_accessor_read_float(normAccessor, v, norm, 3);
                        vertex.Normal = { norm[0], norm[1], norm[2] };
                    } else {
                        vertex.Normal = {0.0f, 1.0f, 0.0f};
                    }
                    
                    if (texAccessor) {
                        float tex[2] = {0,0};
                        cgltf_accessor_read_float(texAccessor, v, tex, 2);
                        vertex.TexCoord = { tex[0], tex[1] };
                    } else {
                        vertex.TexCoord = {0.0f, 0.0f};
                    }
                    
                    vertex.Color = {1.0f, 1.0f, 1.0f}; // Default white
                    vertices.push_back(vertex);
                }
            }

            if (primitive->indices) {
                size_t indexCount = primitive->indices->count;
                for (size_t idx = 0; idx < indexCount; ++idx) {
                    indices.push_back((uint32_t)cgltf_accessor_read_index(primitive->indices, idx));
                }
            } else {
                for (size_t idx = 0; idx < vertexCount; ++idx) {
                    indices.push_back((uint32_t)idx);
                }
            }

            auto vulkanMesh = std::make_shared<Mesh>(context, vertices, indices);
            auto material = std::make_shared<Material>();

            if (primitive->material) {
                cgltf_material* gltfMat = primitive->material;
                if (gltfMat->has_pbr_metallic_roughness) {
                    material->GetProperties().AlbedoFactor = {
                        gltfMat->pbr_metallic_roughness.base_color_factor[0],
                        gltfMat->pbr_metallic_roughness.base_color_factor[1],
                        gltfMat->pbr_metallic_roughness.base_color_factor[2],
                        gltfMat->pbr_metallic_roughness.base_color_factor[3]
                    };
                    material->GetProperties().MetallicFactor = gltfMat->pbr_metallic_roughness.metallic_factor;
                    material->GetProperties().RoughnessFactor = gltfMat->pbr_metallic_roughness.roughness_factor;
                    
                    if (gltfMat->pbr_metallic_roughness.base_color_texture.texture && gltfMat->pbr_metallic_roughness.base_color_texture.texture->image) {
                        cgltf_image* image = gltfMat->pbr_metallic_roughness.base_color_texture.texture->image;
                        if (image->uri) {
                            std::string dir = filepath.substr(0, filepath.find_last_of("/\\") + 1);
                            material->SetAlbedoMap(std::make_shared<Texture2D>(context, dir + image->uri, true));
                        }
                    }

                    if (gltfMat->pbr_metallic_roughness.metallic_roughness_texture.texture && gltfMat->pbr_metallic_roughness.metallic_roughness_texture.texture->image) {
                        cgltf_image* image = gltfMat->pbr_metallic_roughness.metallic_roughness_texture.texture->image;
                        if (image->uri) {
                            std::string dir = filepath.substr(0, filepath.find_last_of("/\\") + 1);
                            material->SetMetallicRoughnessMap(std::make_shared<Texture2D>(context, dir + image->uri, false));
                        }
                    }
                }

                if (gltfMat->normal_texture.texture && gltfMat->normal_texture.texture->image) {
                    cgltf_image* image = gltfMat->normal_texture.texture->image;
                    if (image->uri) {
                        std::string dir = filepath.substr(0, filepath.find_last_of("/\\") + 1);
                        material->SetNormalMap(std::make_shared<Texture2D>(context, dir + image->uri, false));
                    }
                }
            }

            auto entity = world.CreateEntity(mesh->name ? mesh->name : "glTF_Mesh");
            world.GetRegistry().emplace<MeshComponent>(entity, vulkanMesh);
            world.GetRegistry().emplace<MaterialComponent>(entity, material);

            auto& transform = world.GetRegistry().get<TransformComponent>(entity).Transform;
            
            cgltf_node* targetNode = nullptr;
            for (size_t n = 0; n < data->nodes_count; ++n) {
                if (data->nodes[n].mesh == mesh) {
                    targetNode = &data->nodes[n];
                    break;
                }
            }

            if (targetNode) {
                float matrix[16];
                cgltf_node_transform_world(targetNode, matrix);
                glm::mat4 m = glm::make_mat4(matrix);
                
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(m, transform.Scale, transform.Rotation, transform.Position, skew, perspective);
                transform.Rotation = glm::conjugate(transform.Rotation); // decompose might yield conjugated rot

                TUCANO_CORE_INFO("Decomposed Node '{0}' transform: Scale({1}, {2}, {3}), Position({4}, {5}, {6})",
                    targetNode->name ? targetNode->name : "unnamed",
                    transform.Scale.x, transform.Scale.y, transform.Scale.z,
                    transform.Position.x, transform.Position.y, transform.Position.z);
            }
        }
    }

    cgltf_free(data);
    return true;
}

bool ModelLoader::LoadFBX(const std::string& filepath, World& world, VulkanContext* context) {
    ufbx_load_opts opts = {0};
    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(filepath.c_str(), &opts, &error);
    if (!scene) {
        TUCANO_CORE_ERROR("Failed to load FBX file: {0} (Error: {1})", filepath, error.description.data);
        return false;
    }

    TUCANO_CORE_INFO("Successfully loaded FBX: {0} (Meshes: {1}, Nodes: {2})", 
        filepath, scene->meshes.count, scene->nodes.count);

    for (size_t i = 0; i < scene->meshes.count; ++i) {
        ufbx_mesh* mesh = scene->meshes.data[i];
        
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Triangulate indices
        size_t num_tri_indices = mesh->max_face_triangles * 3;
        std::vector<uint32_t> tri_indices(num_tri_indices);

        for (size_t f = 0; f < mesh->num_faces; ++f) {
            ufbx_face face = mesh->faces.data[f];
            uint32_t num_tris = ufbx_triangulate_face(tri_indices.data(), num_tri_indices, mesh, face);
            
            for (uint32_t t = 0; t < num_tris * 3; ++t) {
                uint32_t index = tri_indices[t];
                
                Vertex vertex{};
                
                ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
                vertex.Position = {pos.x, pos.y, pos.z};
                
                if (mesh->vertex_normal.exists) {
                    ufbx_vec3 norm = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                    vertex.Normal = {norm.x, norm.y, norm.z};
                } else {
                    vertex.Normal = {0.0f, 1.0f, 0.0f};
                }

                if (mesh->vertex_uv.exists) {
                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                    vertex.TexCoord = {uv.x, uv.y};
                } else {
                    vertex.TexCoord = {0.0f, 0.0f};
                }
                
                vertex.Color = {1.0f, 1.0f, 1.0f};
                
                // For simplicity we duplicate vertices instead of full indexing deduplication in this step
                vertices.push_back(vertex);
                indices.push_back((uint32_t)(vertices.size() - 1));
            }
        }

        if (!vertices.empty()) {
            auto vulkanMesh = std::make_shared<Mesh>(context, vertices, indices);
            auto material = std::make_shared<Material>();

            if (mesh->materials.count > 0) {
                ufbx_material* fbxMat = mesh->materials.data[0];
                material->GetProperties().AlbedoFactor = {
                    fbxMat->pbr.base_color.value_vec4.x,
                    fbxMat->pbr.base_color.value_vec4.y,
                    fbxMat->pbr.base_color.value_vec4.z,
                    1.0f
                };
                material->GetProperties().MetallicFactor = fbxMat->pbr.metalness.value_real;
                material->GetProperties().RoughnessFactor = fbxMat->pbr.roughness.value_real;

                std::string dir = filepath.substr(0, filepath.find_last_of("/\\") + 1);

                if (fbxMat->pbr.base_color.texture) {
                    material->SetAlbedoMap(std::make_shared<Texture2D>(context, dir + fbxMat->pbr.base_color.texture->relative_filename.data, true));
                }
                if (fbxMat->pbr.roughness.texture) {
                    material->SetMetallicRoughnessMap(std::make_shared<Texture2D>(context, dir + fbxMat->pbr.roughness.texture->relative_filename.data, false));
                }
                if (fbxMat->fbx.normal_map.texture) {
                    material->SetNormalMap(std::make_shared<Texture2D>(context, dir + fbxMat->fbx.normal_map.texture->relative_filename.data, false));
                }
            }

            auto entity = world.CreateEntity(mesh->name.data ? mesh->name.data : "FBX_Mesh");
            world.GetRegistry().emplace<MeshComponent>(entity, vulkanMesh);
            world.GetRegistry().emplace<MaterialComponent>(entity, material);
            
            // Find a node that references this mesh to get transform
            for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
                if (scene->nodes.data[ni]->mesh == mesh) {
                    auto& transform = world.GetRegistry().get<TransformComponent>(entity).Transform;
                    
                    ufbx_matrix u_mat = scene->nodes.data[ni]->geometry_to_world;
                    glm::mat4 m(1.0f);
                    m[0] = glm::vec4(u_mat.cols[0].x, u_mat.cols[0].y, u_mat.cols[0].z, 0.0f);
                    m[1] = glm::vec4(u_mat.cols[1].x, u_mat.cols[1].y, u_mat.cols[1].z, 0.0f);
                    m[2] = glm::vec4(u_mat.cols[2].x, u_mat.cols[2].y, u_mat.cols[2].z, 0.0f);
                    m[3] = glm::vec4(u_mat.cols[3].x, u_mat.cols[3].y, u_mat.cols[3].z, 1.0f);

                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(m, transform.Scale, transform.Rotation, transform.Position, skew, perspective);
                    transform.Rotation = glm::conjugate(transform.Rotation);

                    break;
                }
            }
        }
    }

    ufbx_retain_scene(scene);
    ufbx_free_scene(scene);
    return true;
}

} // namespace Tucano

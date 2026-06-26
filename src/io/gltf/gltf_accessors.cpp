#include "io/gltf/gltf_accessors.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace io::gltf {
namespace {

bool getAccessorLayout(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    const unsigned char*& base,
    int& stride,
    int& component_count,
    int& component_size
) {
    if(accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) return false;
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if(view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    if(buffer.data.empty()) return false;

    component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
    component_count = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
    if(component_size <= 0 || component_count <= 0) return false;

    stride = accessor.ByteStride(view);
    if(stride <= 0) return false;

    std::size_t offset = static_cast<std::size_t>(view.byteOffset + accessor.byteOffset);
    if(offset >= buffer.data.size()) return false;
    base = buffer.data.data() + offset;
    return true;
}

bool getBufferViewPointer(
    const tinygltf::Model& model,
    int buffer_view_index,
    std::size_t byte_offset,
    const unsigned char*& base
) {
    if(buffer_view_index < 0 || buffer_view_index >= static_cast<int>(model.bufferViews.size())) return false;
    const tinygltf::BufferView& view = model.bufferViews[buffer_view_index];
    if(view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    std::size_t offset = static_cast<std::size_t>(view.byteOffset) + byte_offset;
    if(offset >= buffer.data.size()) return false;
    base = buffer.data.data() + offset;
    return true;
}

double readComponentValue(const unsigned char* ptr, int component_type, bool normalized) {
    switch(component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        int8_t v = *reinterpret_cast<const int8_t*>(ptr);
        if(normalized) return std::max(-1.0, static_cast<double>(v) / 127.0);
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        uint8_t v = *reinterpret_cast<const uint8_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 255.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        int16_t v = *reinterpret_cast<const int16_t*>(ptr);
        if(normalized) return std::max(-1.0, static_cast<double>(v) / 32767.0);
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v = *reinterpret_cast<const uint16_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 65535.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t v = *reinterpret_cast<const uint32_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 4294967295.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return static_cast<double>(*reinterpret_cast<const float*>(ptr));
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return *reinterpret_cast<const double*>(ptr);
    default:
        return 0.0;
    }
}

bool readSparseIndex(
    const tinygltf::Model& model,
    const tinygltf::Accessor::Sparse& sparse,
    std::size_t sparse_index,
    std::size_t& out_index
) {
    const unsigned char* base = nullptr;
    if(!getBufferViewPointer(
        model,
        sparse.indices.bufferView,
        static_cast<std::size_t>(sparse.indices.byteOffset),
        base
    )) {
        return false;
    }

    int component_size = tinygltf::GetComponentSizeInBytes(
        static_cast<uint32_t>(sparse.indices.componentType)
    );
    if(component_size <= 0) return false;
    const unsigned char* ptr = base + sparse_index * static_cast<std::size_t>(component_size);
    double value = readComponentValue(ptr, sparse.indices.componentType, false);
    if(value < 0.0) return false;
    out_index = static_cast<std::size_t>(value);
    return true;
}

bool readAccessorElement(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::size_t element_index,
    std::array<double, 4>& out
) {
    if(element_index >= accessor.count) return false;

    out = {0.0, 0.0, 0.0, 1.0};
    const unsigned char* base = nullptr;
    int stride = 0, component_count = 0, component_size = 0;
    if(accessor.bufferView >= 0) {
        if(!getAccessorLayout(model, accessor, base, stride, component_count, component_size)) return false;
        const unsigned char* element_ptr = base + element_index * static_cast<std::size_t>(stride);
        for(int c = 0; c < component_count && c < 4; ++c) {
            out[c] = readComponentValue(
                element_ptr + c * component_size,
                accessor.componentType,
                accessor.normalized
            );
        }
    } else {
        component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
        component_count = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
        if(component_size <= 0 || component_count <= 0) return false;
    }

    if(accessor.sparse.isSparse && accessor.sparse.count > 0) {
        for(std::size_t sparse_i = 0; sparse_i < static_cast<std::size_t>(accessor.sparse.count); ++sparse_i) {
            std::size_t target_index = 0;
            if(!readSparseIndex(model, accessor.sparse, sparse_i, target_index)) return false;
            if(target_index != element_index) continue;

            const unsigned char* sparse_values = nullptr;
            if(!getBufferViewPointer(
                model,
                accessor.sparse.values.bufferView,
                static_cast<std::size_t>(accessor.sparse.values.byteOffset),
                sparse_values
            )) {
                return false;
            }
            std::size_t sparse_stride = static_cast<std::size_t>(component_count * component_size);
            const unsigned char* sparse_ptr = sparse_values + sparse_i * sparse_stride;
            for(int c = 0; c < component_count && c < 4; ++c) {
                out[c] = readComponentValue(
                    sparse_ptr + c * component_size,
                    accessor.componentType,
                    accessor.normalized
                );
            }
            break;
        }
    }
    return true;
}

} // namespace

bool readIndexAccessor(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& out_indices
) {
    int component_count = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
    if(component_count != 1) return false;

    out_indices.resize(accessor.count);
    for(std::size_t i = 0; i < accessor.count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        double value = v[0];
        out_indices[i] = value < 0.0 ? 0u : static_cast<uint32_t>(value);
    }
    return true;
}

bool readAttributeVec3(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec3>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count != expected_count) return false;

    out.resize(expected_count);
    for(std::size_t i = 0; i < expected_count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec3(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
    }
    return true;
}

bool readAttributeVec2(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec2>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count != expected_count) return false;

    out.resize(expected_count);
    for(std::size_t i = 0; i < expected_count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec2(static_cast<float>(v[0]), static_cast<float>(v[1]));
    }
    return true;
}

bool readAttributeVec4(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec4>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count != expected_count) return false;

    out.resize(expected_count);
    for(std::size_t i = 0; i < expected_count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec4(
            static_cast<float>(v[0]),
            static_cast<float>(v[1]),
            static_cast<float>(v[2]),
            static_cast<float>(v[3])
        );
    }
    return true;
}

} // namespace io::gltf

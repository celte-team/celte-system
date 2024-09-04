#include "Grape.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <glm/glm.hpp>
#include <ranges>

namespace celte {
    namespace chunks {
        Grape::Grape(const GrapeOptions& options)
            : _options(options)
        {
           if (_options.subdivision <= 0) {
                throw std::invalid_argument("Subdivision must be a positive integer.");
           }
           __subdivide();
        }

        Grape::Grape(Grape& grape, std::vector<std::string> chunksIds)
            : _options(grape._options)
        {
            for (auto chunkId : chunksIds) {
                _chunks[chunkId] = grape._chunks[chunkId];
                grape._chunks.erase(chunkId);
            }
        }

        Grape::~Grape() { }

        void Grape::__subdivide() {
            // subdivide each axis into _options.subdivision parts to create a list of points equally spaced
            // along each axis, to map the space in the grape
            glm::vec3 forward = glm::normalize(_options.forward);
            glm::vec3 up = glm::normalize(_options.up);
            glm::vec3 right = glm::normalize(glm::cross(forward, up));

            float chunkSizeForward = _options.sizeForward / _options.subdivision;
            float chunkSizeRight = _options.sizeRight / _options.subdivision;
            float chunkSizeUp = _options.sizeUp / _options.subdivision;

            glm::vec3 start = _options.position - (_options.sizeForward / 2.0f * forward) - (_options.sizeRight / 2.0f * right) - (_options.sizeUp / 2.0f * up);

            auto range = std::views::iota(0, _options.subdivision);

            for (int i : range) {
                for (int j : range) {
                    for (int k : range) {
                        glm::vec3 point = start + i * (_options.sizeForward / _options.subdivision) * forward
                                                + j * (_options.sizeRight / _options.subdivision) * right
                                                + k * (_options.sizeUp / _options.subdivision) * up;
                        ChunkConfig cconfig = {
                            .chunkId = std::to_string(i) + "-" + std::to_string(j) + "-" + std::to_string(k),
                            .grapeId = _options.grapeId,
                            .position = point,
                            .sizeForward = chunkSizeForward,
                            .sizeRight = chunkSizeRight,
                            .sizeUp = chunkSizeUp,
                            .forward = forward
                        };
                        _chunks[cconfig.chunkId] = std::make_shared<Chunk>(cconfig);
                    }
                }
            }
        }
    } // namespace chunks
} // namespace celte
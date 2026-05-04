#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace render::cpu {

class AliasTable {
public:
    AliasTable() = default;
    explicit AliasTable(const std::vector<float>& weights) { build(weights); }

    void build(const std::vector<float>& weights) {
        const std::size_t count = weights.size();
        probability.assign(count, 1.0f);
        alias.resize(count);
        pdf_values.assign(count, 0.0f);
        total_weight = 0.0f;

        if(count == 0) return;
        for(std::size_t i = 0; i < count; ++i) {
            alias[i] = static_cast<uint32_t>(i);
            total_weight += std::max(0.0f, weights[i]);
        }

        if(total_weight <= 0.0f) {
            float uniform_pdf = 1.0f / static_cast<float>(count);
            std::fill(pdf_values.begin(), pdf_values.end(), uniform_pdf);
            return;
        }

        std::vector<float> scaled(count, 0.0f);
        std::vector<uint32_t> small;
        std::vector<uint32_t> large;
        small.reserve(count);
        large.reserve(count);

        for(std::size_t i = 0; i < count; ++i) {
            pdf_values[i] = std::max(0.0f, weights[i]) / total_weight;
            scaled[i] = pdf_values[i] * static_cast<float>(count);
            if(scaled[i] < 1.0f) {
                small.push_back(static_cast<uint32_t>(i));
            } else {
                large.push_back(static_cast<uint32_t>(i));
            }
        }

        while(!small.empty() && !large.empty()) {
            uint32_t s = small.back();
            small.pop_back();
            uint32_t l = large.back();

            probability[s] = scaled[s];
            alias[s] = l;

            scaled[l] = scaled[l] + scaled[s] - 1.0f;
            if(scaled[l] < 1.0f) {
                large.pop_back();
                small.push_back(l);
            }
        }

        for(uint32_t index : large) probability[index] = 1.0f;
        for(uint32_t index : small) probability[index] = 1.0f;
    }

    std::size_t size() const { return probability.size(); }
    bool empty() const { return probability.empty(); }

    std::size_t sample(float u_column, float u_choice, float& pdf) const {
        if(probability.empty()) {
            pdf = 0.0f;
            return 0;
        }

        std::size_t column = std::min(
            static_cast<std::size_t>(std::max(0.0f, u_column) * static_cast<float>(probability.size())),
            probability.size() - 1
        );
        std::size_t index = u_choice < probability[column] ? column : alias[column];
        pdf = pdf_values[index];
        return index;
    }

    float pdf(std::size_t index) const {
        if(index >= pdf_values.size()) return 0.0f;
        return pdf_values[index];
    }

private:
    std::vector<float> probability;
    std::vector<uint32_t> alias;
    std::vector<float> pdf_values;
    float total_weight = 0.0f;
};

} // namespace render::cpu

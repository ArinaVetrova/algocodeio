#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

template <typename T>
class DiscreteDistributionSampler {
   public:
    explicit DiscreteDistributionSampler(
        const std::vector<std::pair<T, float>>& objectsWithWeights,
        std::function<float()> randomGenerator = [] { return std::rand(); })
        : randomGenerator_(randomGenerator) {
        for (auto [object, weight] : objectsWithWeights) {
            objects_.push_back(object);
            assert(weight > 0.0);
            totalWeight_ += weight;
        }

        float currentWeight = 0.0;
        for (auto [_, weight] : objectsWithWeights) {
            currentWeight += weight;
            cumulativeWeights_.push_back(currentWeight);
        }
    }

    void Add(const std::pair<T, float>& elem) {
        float weight = elem.second;
        assert(weight > 0.0);
        objects_.push_back(elem.first);
        totalWeight_ += weight;
        cumulativeWeights_.push_back(cumulativeWeights_.back() + weight);
    }

    T Sample() const {
        auto r = randomGenerator_();
        auto target = static_cast<float>(r) * totalWeight_;

        auto it = std::lower_bound(cumulativeWeights_.begin(), cumulativeWeights_.end(), target);
        assert(it != cumulativeWeights_.end());

        return objects_[it - cumulativeWeights_.begin()];
    }

   private:
    std::vector<T> objects_;
    std::vector<float> cumulativeWeights_;
    float totalWeight_;
    std::function<float()> randomGenerator_;
};

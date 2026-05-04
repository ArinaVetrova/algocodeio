#include <cassert>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <vector>

template <typename T>
class DiscreteDistributionSampler {
   public:
    explicit DiscreteDistributionSampler(const std::vector<std::pair<T, float>>& objectsWithWeights,
                                         std::function<float()> randomGenerator = nullptr)
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
        std::unique_lock<std::shared_mutex> lock(mtx);
        float weight = elem.second;
        assert(weight > 0.0);
        objects_.push_back(elem.first);
        totalWeight_ += weight;
        float prev = cumulativeWeights_.empty() ? 0.0f : cumulativeWeights_.back();
        cumulativeWeights_.push_back(prev + weight);
    }

    T Sample() {
        std::shared_lock lock(mtx);
        float target;
        if (randomGenerator_) {
            auto r = randomGenerator_();
            target = static_cast<float>(r) * totalWeight_;
        } else {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, totalWeight_);
            target = dist(rng);
        }

        auto it = std::lower_bound(cumulativeWeights_.begin(), cumulativeWeights_.end(), target);
        assert(it != cumulativeWeights_.end());

        return objects_[it - cumulativeWeights_.begin()];
    }

   private:
    std::vector<T> objects_;
    std::vector<float> cumulativeWeights_;
    float totalWeight_ = 0.0;
    std::function<float()> randomGenerator_;

    std::shared_mutex mtx;
};

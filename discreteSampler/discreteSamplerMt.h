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
        : randomGenerator(randomGenerator) {
        for (auto [object, weight] : objectsWithWeights) {
            objects.push_back(object);
            assert(weight > 0.0);
            totalWeight += weight;
        }

        float currentWeight = 0.0;
        for (auto [_, weight] : objectsWithWeights) {
            currentWeight += weight;
            cumulativeWeights.push_back(currentWeight);
        }
    }

    void Add(const std::pair<T, float>& elem) {
        // unique lock for adding
        std::unique_lock<std::shared_mutex> lock(mtx);
        float weight = elem.second;
        assert(weight > 0.0);
        objects.push_back(elem.first);
        totalWeight += weight;
        float prev = cumulativeWeights.empty() ? 0.0f : cumulativeWeights.back();
        cumulativeWeights.push_back(prev + weight);
    }

    T Sample() {
        // shared_lock for reading
        std::shared_lock lock(mtx);
        float target;
        if (randomGenerator) {
            auto r = randomGenerator();
            target = static_cast<float>(r) * totalWeight;
        } else {
            // thread local generator - so other threads don't affect other threads generators
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, totalWeight);
            target = dist(rng);
        }

        auto it = std::lower_bound(cumulativeWeights.begin(), cumulativeWeights.end(), target);
        assert(it != cumulativeWeights.end());

        return objects[it - cumulativeWeights.begin()];
    }

   private:
    std::vector<T> objects;
    std::vector<float> cumulativeWeights;
    float totalWeight = 0.0;
    std::function<float()> randomGenerator;

    std::shared_mutex mtx;
};

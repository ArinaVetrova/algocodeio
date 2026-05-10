#include <gtest/gtest.h>

#include <thread>

#include "discreteSamplerMt.h"

TEST(DiscreteDistributionSampler, OneElement) {
    auto gen = []() { return 0.1f; };
    auto sampler = DiscreteDistributionSampler<int>({{10, 1.0}}, gen);
    ASSERT_EQ(sampler.Sample(), 10);
}

TEST(DiscreteDistributionSampler, TwoElements) {
    auto gen = []() -> float {
        static int callCount = 0;
        static const float values[] = {0.1f, 0.2f, 0.4f};
        return values[callCount++ % 3];
    };
    auto sampler = DiscreteDistributionSampler<int>({{10, 1.0}, {20, 2.0}}, gen);

    // target: 0.1 * 3.0 = 0.3 → 10 (0.0 ≤ 0.3 < 1.0)
    // target: 0.2 * 3.0 = 0.6 → 10 (0.0 ≤ 0.6 < 1.0)
    // target: 0.4 * 3.0 = 1.2 → 20 (1.0 ≤ 1.2 < 3.0)
    EXPECT_EQ(sampler.Sample(), 10);
    EXPECT_EQ(sampler.Sample(), 10);
    EXPECT_EQ(sampler.Sample(), 20);
}

TEST(DiscreteDistributionSampler, AddToEmptySampler) {
    auto gen = []() { return 0.5f; };
    DiscreteDistributionSampler<int> sampler({}, gen);

    sampler.Add({42, 1.0f});

    EXPECT_EQ(sampler.Sample(), 42);
}

TEST(DiscreteDistributionSampler, AddElementToExisting) {
    auto gen = []() -> float {
        static int callCount = 0;
        static const float values[] = {0.1f, 0.6f, 0.9f};
        return values[callCount++ % 3];
    };

    DiscreteDistributionSampler<int> sampler({{10, 1.0}}, gen);
    sampler.Add({20, 2.0f});

    EXPECT_EQ(sampler.Sample(), 10);
    EXPECT_EQ(sampler.Sample(), 20);
    EXPECT_EQ(sampler.Sample(), 20);
}

TEST(DiscreteDistributionSampler, MultipleAdds) {
    auto gen = []() -> float {
        static int callCount = 0;
        static const float values[] = {0.05f, 0.2f, 0.4f, 0.7f, 0.95f};
        return values[callCount++ % 5];
    };

    DiscreteDistributionSampler<int> sampler({{10, 1.0}}, gen);

    sampler.Add({20, 2.0f});
    sampler.Add({30, 3.0f});
    sampler.Add({40, 4.0f});

    EXPECT_EQ(sampler.Sample(), 10);
    EXPECT_EQ(sampler.Sample(), 20);
    EXPECT_EQ(sampler.Sample(), 30);
    EXPECT_EQ(sampler.Sample(), 40);
    EXPECT_EQ(sampler.Sample(), 40);
}

TEST(DiscreteDistributionSamplerMt, ThreadSafetySample) {
    DiscreteDistributionSampler<int> sampler({{10, 1.0}, {20, 2.0}, {30, 3.0}});

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&sampler, &errors]() {
            for (int j = 0; j < 1000; j++) {
                auto result = sampler.Sample();
                if (result != 10 && result != 20 && result != 30) {
                    errors++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(errors, 0);
}

TEST(DiscreteDistributionSamplerMt, ThreadSafetySampleAndAdd) {
    DiscreteDistributionSampler<int> sampler({{10, 1.0}, {20, 2.0}});

    std::vector<std::thread> threads;
    std::atomic<bool> crashed{false};

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&sampler, &crashed]() {
            for (int j = 0; j < 1000; j++) {
                try {
                    sampler.Sample();
                } catch (...) {
                    crashed = true;
                }
            }
        });
    }

    // rare writer
    threads.emplace_back([&sampler]() {
        for (int i = 30; i <= 50; i += 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sampler.Add({i, 1.0f});
        }
    });

    for (auto& t : threads) t.join();
    EXPECT_FALSE(crashed);
}

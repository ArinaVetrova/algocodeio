#include <gtest/gtest.h>

#include "discreteSampler.h"

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

#include "xmlEncryptorMt.h"

#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

std::string encryptNode(
    const std::vector<std::string_view>& path, const std::string& text,
    const std::vector<std::pair<std::string_view, std::string>>& encryptedChild) {

    std::string result;
    for (auto p : path) {
        if (!result.empty()) result += ".";
        result += p;
    }

    if (!encryptedChild.empty()) {
        result += "(";
        for (size_t i = 0; i < encryptedChild.size(); ++i) {
            if (i > 0) result += ",";
            result += std::string(encryptedChild[i].first) + "=" + encryptedChild[i].second;
        }
        result += ")";
    }

    return result;
}
struct NodeEncyptionCollector : std::enable_shared_from_this<NodeEncyptionCollector> {
    const XmlNode* node;
    std::vector<std::string_view> path;
    EncryptedChildren encryptedChilds;
    size_t remainChilds = 0;
    Pool* pool = nullptr;
    std::function<void(std::string_view, std::string)> onDone;
    std::mutex mtx;

    NodeEncyptionCollector(
        const XmlNode* node,
        std::vector<std::string_view> path,
        std::function<void(std::string_view, std::string)> onDone,
        Pool* pool)
        : node(node)
        , path(std::move(path))
        , remainChilds(node->children.size())
        , pool(pool)
        , onDone(std::move(onDone)) {
        encryptedChilds.reserve(node->children.size());
    }

    void OnChildEncrypted(std::string_view tagName, std::string encrypted) {
        std::shared_ptr<NodeEncyptionCollector> self = shared_from_this();
        bool allDone = false;
        {
            std::lock_guard lock(mtx);
            encryptedChilds.emplace_back(tagName, std::move(encrypted));
            --remainChilds;
        }
        if (remainChilds == 0) {
            pool->Submit([self]() mutable {
                auto tagName = self->path.back();
                auto encryptedParent = encryptNode(
                    self->path, self->node->text, self->encryptedChilds);
                self->onDone(tagName, std::move(encryptedParent));
            });
        }
    }
};

static void SubmitLeaf(
    Pool& pool,
    const std::shared_ptr<NodeEncyptionCollector>& collector)
{
    pool.Submit([collector]() {
        auto encrypted = encryptNode(collector->path, collector->node->text, {});
        collector->onDone(collector->node->tagName, std::move(encrypted));
    });
}

static void EnqueueChildren(
    Pool& pool,
    const std::shared_ptr<NodeEncyptionCollector>& collector,
    std::queue<std::shared_ptr<NodeEncyptionCollector>>& queue)
{
    for (const auto& child : collector->node->children) {
        auto childPath = collector->path;
        childPath.push_back(child.tagName);

        auto childCollector = std::make_shared<NodeEncyptionCollector>(
            &child,
            std::move(childPath),
            [parentCollector = collector](std::string_view tag, std::string encrypted) {
                parentCollector->OnChildEncrypted(tag, std::move(encrypted));
            },
            &pool
        );

        queue.push(childCollector);
    }
}

std::string encryptXmlTree(Pool& pool, const XmlNode& root) {
    std::string res;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;

    auto rootCollector = std::make_shared<NodeEncyptionCollector>(
        &root,
        std::vector<std::string_view>{root.tagName},
        [&](std::string_view, std::string encrypted) {
            {
                std::lock_guard lock(mutex);
                res = std::move(encrypted);
                done = true;
            }
            cv.notify_one();
        },
        &pool
    );

    std::queue<std::shared_ptr<NodeEncyptionCollector>> queue;
    queue.push(rootCollector);

    while (!queue.empty()) {
        auto collector = queue.front();
        queue.pop();

        if (collector->node->children.empty()) {
            SubmitLeaf(pool, collector);
        } else {
            EnqueueChildren(pool, collector, queue);
        }
    }

    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return done; });

    return res;
}

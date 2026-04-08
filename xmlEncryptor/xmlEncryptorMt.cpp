#include "xmlEncryptorMt.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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
struct NodeEncryptionCollector : std::enable_shared_from_this<NodeEncryptionCollector> {
    const XmlNode* node;
    std::vector<std::string_view> path;
    EncryptedChildren encryptedChilds;
    std::atomic<size_t> remainChilds = 0;
    Pool* pool = nullptr;
    std::function<void(std::string_view, std::string)> onDone;

    NodeEncryptionCollector(const XmlNode* node, std::vector<std::string_view> path,
                            std::function<void(std::string_view, std::string)> onDone, Pool* pool)
        : node(node),
          path(std::move(path)),
          remainChilds(node->children.size()),
          pool(pool),
          onDone(std::move(onDone)) {
        encryptedChilds.resize(node->children.size());
    }

    void OnChildEncrypted(size_t childIdx, std::string_view tagName, std::string encrypted) {
        std::shared_ptr<NodeEncryptionCollector> self = shared_from_this();

        encryptedChilds[childIdx] = {tagName, std::move(encrypted)};
        // fectch_sub(1) returns value befor decrement and if it was 1 --> it become 0, all childs
        // processed
        if (remainChilds.fetch_sub(1) == 1) {
            pool->Submit([self]() mutable {
                auto tagName = self->path.back();
                auto encryptedParent =
                    encryptNode(self->path, self->node->text, self->encryptedChilds);
                self->onDone(tagName, std::move(encryptedParent));
            });
        }
    }
};

static void SubmitLeaf(Pool& pool, const std::shared_ptr<NodeEncryptionCollector>& collector) {
    pool.Submit([collector]() {
        auto encrypted = encryptNode(collector->path, collector->node->text, {});
        collector->onDone(collector->node->tagName, std::move(encrypted));
    });
}

static void EnqueueChildren(Pool& pool, const std::shared_ptr<NodeEncryptionCollector>& collector,
                            std::queue<std::shared_ptr<NodeEncryptionCollector>>& queue) {
    for (size_t i = 0; i < collector->node->children.size(); ++i) {
        const auto& child = collector->node->children[i];
        auto childPath = collector->path;
        childPath.push_back(child.tagName);

        auto childCollector = std::make_shared<NodeEncryptionCollector>(
            &child, std::move(childPath),
            [parentCollector = collector, i](std::string_view tag, std::string encrypted) {
                parentCollector->OnChildEncrypted(i, tag, std::move(encrypted));
            },
            &pool);

        queue.push(childCollector);
    }
}

std::string encryptXmlTree(Pool& pool, const XmlNode& root) {
    std::string res;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;

    auto rootCollector = std::make_shared<NodeEncryptionCollector>(
        &root, std::vector<std::string_view>{root.tagName},
        [&](std::string_view, std::string encrypted) {
            {
                std::lock_guard lock(mutex);
                res = std::move(encrypted);
                done = true;
            }
            cv.notify_one();
        },
        &pool);

    std::queue<std::shared_ptr<NodeEncryptionCollector>> queue;
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

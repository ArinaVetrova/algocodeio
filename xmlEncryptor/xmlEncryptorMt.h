#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using EncryptedChildren =
    std::vector<std::pair<std::string_view /*tagName*/, std::string /*encryptedChild*/>>;

struct XmlNode {
    std::string tagName;
    std::string text;
    std::vector<XmlNode> children;
};

class Pool {
   public:
    virtual void Submit(std::function<void()> task) {}
};

// added simple pool to be able to run the tests
class SimplePool : public Pool {
   public:
    void Submit(std::function<void()> task) override { std::thread(std::move(task)).detach(); }
};

std::string encryptNode(
    const std::vector<std::string_view>& path, const std::string& text,
    const std::vector<std::pair<std::string_view /*tagName*/, std::string /*encryptedChild*/>>&
        encryptedChild);

// taken root of the tree, returns encryptedString for whole xml tree using thread Pool
std::string encryptXmlTree(Pool& pool, const XmlNode& root);

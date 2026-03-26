#include "xmlEncryptor.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/*
example of xml tree:
<root>
    <child1>
        <subChild1>text</subChild1>
    </child1>
    <child2>
        <subChild2>text</subChild2>
    </child2>
</root>
*/

using EncryptedChildren =
    std::vector<std::pair<std::string_view /*tagName*/, std::string /*encryptedChild*/>>;

// encryptNode func is already given outside, here is func to be able to build project
// it supposed that the func is threadsafe
// encrypt Node with a path like {"root", "child", "subChild"}
// encrypt just as the order of encryption to check in the tests
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

struct NodeEncryptionCollector {
    const XmlNode* node;
    std::vector<std::string_view> path;
    size_t processedChilds = 0;
    EncryptedChildren encryptedChilds;
    // calling this calback field when the node is encrypted with encryptNode
    std::function<void(std::string_view, std::string)> onDone;

    NodeEncryptionCollector(const XmlNode* node, std::vector<std::string_view> path,
                           size_t processedChilds,
                           std::function<void(std::string_view, std::string)> onDone)
        : node(node),
          path(std::move(path)),
          processedChilds(processedChilds),
          onDone(std::move(onDone)) {}
};

std::string encryptXmlTree(const XmlNode& root) {
    std::string res;

    // Note! as a base container for std::stack - std::deque should be used here
    // to avoid dangling references in callbacks after poping from the stack
    std::stack<NodeEncryptionCollector, std::deque<NodeEncryptionCollector>> treeTraverseStack;

    // root using callback which writes to res
    treeTraverseStack.emplace(
        &root, std::vector<std::string_view>{root.tagName}, 0,
        [&res](std::string_view, std::string encrypted) { res = std::move(encrypted); });

    while (!treeTraverseStack.empty()) {
        NodeEncryptionCollector& nodeWrp = treeTraverseStack.top();

        if (nodeWrp.processedChilds < nodeWrp.node->children.size()) {
            const XmlNode& childNode = nodeWrp.node->children[nodeWrp.processedChilds];
            ++nodeWrp.processedChilds;

            std::vector<std::string_view> childPath = nodeWrp.path;
            childPath.push_back(childNode.tagName);

            // the child is created with callback whis puts encryption result to related parent node
            // encryptedChilds
            treeTraverseStack.emplace(&childNode, childPath, 0,
                                      [&nodeWrp](std::string_view tagName, std::string encrypted) {
                                          nodeWrp.encryptedChilds.emplace_back(
                                              tagName, std::move(encrypted));
                                      });
        } else {  // tree leaf reached, start encrypt child nodes from the bottom and pushing
                  // results to related parents by calling onDone
            auto tagName = nodeWrp.path.back();
            auto encrypted = encryptNode(nodeWrp.path, nodeWrp.node->text, nodeWrp.encryptedChilds);
            nodeWrp.onDone(tagName, std::move(encrypted));
            treeTraverseStack.pop();
        }
    }

    std::cout << "res: " << res << std::endl;
    return res;
}

#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
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

struct XmlNode {
    std::string tagName;
    std::string text;
    std::vector<XmlNode> children;
};

// encryptNode func is already given outside, here is func to be able to build project
// it supposed that the func is threadsafe
// encrypt Node with a path like {"root", "child", "subChild"}
// encrypt just as the order of encryption to check in the tests
std::string encryptNode(
    const std::vector<std::string_view>& path, const std::string& text,
    const std::vector<std::pair<std::string_view /*tagName*/, std::string /*encryptedChild*/>>&
        encryptedChild) {
    std::string result;
    for (auto p : path) {
        if (!result.empty()) result += ".";
        result += p;
    }

    if (!encryptedChild.empty()) {
        result += "(";
        for (size_t i = 0; i < encryptedChild.size(); ++i) {
            if (i > 0) result += ",";
            std::cout << "encryptedChild[" << i << "]: " << encryptedChild[i].first << ", "
                      << encryptedChild[i].second << "\n";
            result += std::string(encryptedChild[i].first) + "=" + encryptedChild[i].second;
        }
        result += ")";
    }

    return result;
}

struct NodeEncyptionCollector {
    const XmlNode* node;
    std::vector<std::string_view> path;
    size_t processedChilds = 0;
    EncryptedChildren encryptedChilds;
};

// taken root of the tree, returns encryptedString for whole xml tree
std::string encryptXmlTree(const XmlNode& root) {
    std::string res{""};

    std::vector<std::string_view> curPath{root.tagName};

    std::stack<NodeEncyptionCollector> treeTraverseStack;
    treeTraverseStack.emplace(&root, curPath, 0);

    EncryptedChildren encryptedChilds;

    while (!treeTraverseStack.empty()) {
        NodeEncyptionCollector& nodeWrp = treeTraverseStack.top();

        // if there is nested childs - keep put them to the stack and process them later
        if (nodeWrp.processedChilds < nodeWrp.node->children.size()) {
            const XmlNode& childNode = nodeWrp.node->children[nodeWrp.processedChilds];
            ++nodeWrp.processedChilds;

            std::vector<std::string_view> childPath = nodeWrp.path;
            childPath.push_back(childNode.tagName);
            treeTraverseStack.emplace(&childNode, childPath, 0);
        } else {  // tree leaf reached, start encrypt child nodes from the botton
            auto tagName = nodeWrp.path.back();  // contains last child tag
            res = encryptNode(nodeWrp.path, nodeWrp.node->text, nodeWrp.encryptedChilds);

            treeTraverseStack.pop();  // node encrypted
            if (!treeTraverseStack.empty()) {
                // if stack isn't empty, add encrypted child to the parent node path
                treeTraverseStack.top().encryptedChilds.emplace_back(tagName, res);
            } else {
                break;  // if stack is empty - it was the root and res contains encrypted tree
            }
        }
    }

    std::cout << "res: " << res << std::endl;
    return res;
}

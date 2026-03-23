#pragma once

#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
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

std::string encryptNode(
    const std::vector<std::string_view>& path, const std::string& text,
    const std::vector<std::pair<std::string_view /*tagName*/, std::string /*encryptedChild*/>>&
        encryptedChild);

// taken root of the tree, returns encryptedString for whole xml tree
std::string encryptXmlTree(const XmlNode& root);

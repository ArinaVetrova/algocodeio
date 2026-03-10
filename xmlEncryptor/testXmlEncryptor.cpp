#include <gtest/gtest.h>

#include "xmlEncryptor.h"

XmlNode createNode(std::string name, std::string text = "") {
    XmlNode node;
    node.tagName = name;
    node.text = text;
    return node;
}

void addChild(XmlNode& parent, XmlNode child) { parent.children.push_back(std::move(child)); }

TEST(XmlEncryptorTest, SingleNode) {
    XmlNode root;
    root.tagName = "root";
    root.text = "hello";

    std::string result = encryptXmlTree(root);

    EXPECT_EQ(result, "root");
}

TEST(XmlEncryptorTest, OneChild) {
    XmlNode root;
    root.tagName = "root";
    root.text = "root_text";

    XmlNode child;
    child.tagName = "child";
    child.text = "child_text";
    root.children.push_back(child);

    std::string result = encryptXmlTree(root);

    // child encrypted as "child" and root as "root(child=child)"
    EXPECT_EQ(result, "root(child=root.child)");
}

TEST(XmlEncryptorTest, TwoChildren) {
    XmlNode root;
    root.tagName = "root";

    XmlNode child1;
    child1.tagName = "child1";
    root.children.push_back(child1);

    XmlNode child2;
    child2.tagName = "child2";
    root.children.push_back(child2);

    std::string result = encryptXmlTree(root);

    // root: "root(child1=root.child1,child2=root.child2)"
    EXPECT_EQ(result, "root(child1=root.child1,child2=root.child2)");
}

TEST(XmlEncryptorTest, NestedChildren) {
    XmlNode root;
    root.tagName = "root";

    XmlNode child;
    child.tagName = "child";

    XmlNode grandchild;
    grandchild.tagName = "grandchild";
    child.children.push_back(grandchild);

    root.children.push_back(child);

    std::string result = encryptXmlTree(root);

    // grandchild: "root.child.grandchild"
    // child: "root.child(grandchild=root.child.grandchild)"
    // root: "root(child=root.child(grandchild=root.child.grandchild))"

    EXPECT_EQ(result, "root(child=root.child(grandchild=root.child.grandchild))");
}

TEST(XmlEncryptorTest, ComplexTree) {
    // root
    //   child1
    //     grandchild1
    //   child2
    //     grandchild2
    //     grandchild3

    XmlNode root;
    root.tagName = "root";

    XmlNode child1;
    child1.tagName = "child1";

    XmlNode grandchild1;
    grandchild1.tagName = "grandchild1";
    child1.children.push_back(grandchild1);

    XmlNode child2;
    child2.tagName = "child2";

    XmlNode grandchild2;
    grandchild2.tagName = "grandchild2";
    child2.children.push_back(grandchild2);

    XmlNode grandchild3;
    grandchild3.tagName = "grandchild3";
    child2.children.push_back(grandchild3);

    root.children.push_back(child1);
    root.children.push_back(child2);

    std::string result = encryptXmlTree(root);

    std::string expected =
        "root("
        "child1=root.child1(grandchild1=root.child1.grandchild1),"
        "child2=root.child2(grandchild2=root.child2.grandchild2,grandchild3=root.child2."
        "grandchild3)"
        ")";

    EXPECT_EQ(result, expected);
}

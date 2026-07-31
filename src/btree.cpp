#include "btree.h"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace mydb {

// ── Node serialization ────────────────────────────────────────────
// Simple text format stored in page data area

void BTreeNode::serialize(Page& page) const {
    std::ostringstream ss;
    ss << (isLeaf ? "L" : "I") << "\n" << keyCount << "\n";
    for (int i = 0; i < keyCount; ++i)
        ss << keys[i] << "\n";
    if (!isLeaf) {
        for (int i = 0; i <= keyCount; ++i)
            ss << children[i] << "\n";
    } else {
        for (int i = 0; i < keyCount; ++i)
            ss << values[i] << "\n";
        ss << nextLeaf << "\n";
    }
    std::string data = ss.str();
    char* dest = page.rawData() + sizeof(PageHeader);
    std::memcpy(dest, data.c_str(), std::min(data.size(),
                PAGE_SIZE - sizeof(PageHeader)));
}

void BTreeNode::deserialize(const Page& page) {
    const char* src = page.rawData() + sizeof(PageHeader);
    std::istringstream ss(src);
    std::string type;
    ss >> type;
    isLeaf = (type == "L");
    ss >> keyCount;
    for (int i = 0; i < keyCount; ++i) ss >> keys[i];
    if (!isLeaf) {
        for (int i = 0; i <= keyCount; ++i) ss >> children[i];
    } else {
        for (int i = 0; i < keyCount; ++i) ss >> values[i];
        ss >> nextLeaf;
    }
}

// ── Buffer pool helpers ───────────────────────────────────────────

BTreeNode BTree::loadNode(PageId id) {
    Page* page = pool_.fetchPage(id);
    BTreeNode node;
    node.deserialize(*page);
    pool_.unpin(id, false);
    return node;
}

void BTree::saveNode(PageId id, const BTreeNode& node) {
    Page* page = pool_.fetchPage(id);
    node.serialize(*page);
    pool_.unpin(id, true); // mark dirty — needs to be written to disk
}

PageId BTree::allocNode() {
    Page* page = pool_.newPage();
    PageId id  = page->id();
    pool_.unpin(id, true);
    return id;
}

// ── Constructor ───────────────────────────────────────────────────

BTree::BTree(BufferPool& pool) : pool_(pool) {
    // Create root leaf node
    rootId_ = allocNode();
    BTreeNode root;
    root.isLeaf   = true;
    root.keyCount = 0;
    saveNode(rootId_, root);
}

// ── Search ────────────────────────────────────────────────────────

PageId BTree::findLeaf(const std::string& key) {
    PageId cur = rootId_;

    while (true) {
        BTreeNode node = loadNode(cur);
        if (node.isLeaf) return cur;

        // Find child to follow
        int i = 0;
        while (i < node.keyCount && key >= node.keys[i]) ++i;
        cur = node.children[i];
    }
}

std::optional<std::string> BTree::search(const std::string& key) {
    PageId leafId = findLeaf(key);
    BTreeNode leaf = loadNode(leafId);

    for (int i = 0; i < leaf.keyCount; ++i) {
        if (leaf.keys[i] == key)
            return leaf.values[i];
    }
    return std::nullopt;
}

// ── Range scan ────────────────────────────────────────────────────

std::vector<std::pair<std::string, std::string>>
BTree::rangeScan(const std::string& from, const std::string& to) {
    std::vector<std::pair<std::string, std::string>> result;

    PageId leafId = findLeaf(from);

    // Walk leaf linked list
    while (leafId != INVALID_PAGE_ID) {
        BTreeNode leaf = loadNode(leafId);

        for (int i = 0; i < leaf.keyCount; ++i) {
            if (leaf.keys[i] > to)   return result; // past range
            if (leaf.keys[i] >= from)
                result.emplace_back(leaf.keys[i], leaf.values[i]);
        }

        leafId = leaf.nextLeaf;
    }
    return result;
}

// ── Insert ────────────────────────────────────────────────────────

void BTree::insert(const std::string& key, const std::string& value) {
    auto split = insertRec(rootId_, key, value);

    if (split.has_value()) {
        // Root was split — create new root
        PageId newRootId = allocNode();
        BTreeNode newRoot;
        newRoot.isLeaf      = false;
        newRoot.keyCount    = 1;
        newRoot.keys[0]     = split->key;
        newRoot.children[0] = rootId_;
        newRoot.children[1] = split->newPage;
        saveNode(newRootId, newRoot);
        rootId_ = newRootId;
    }
}

std::optional<BTree::SplitResult>
BTree::insertRec(PageId nodeId,
                 const std::string& key,
                 const std::string& value) {
    BTreeNode node = loadNode(nodeId);

    if (node.isLeaf) {
        // Insert into sorted position
        int pos = node.keyCount;
        while (pos > 0 && node.keys[pos - 1] > key) {
            node.keys[pos]   = node.keys[pos - 1];
            node.values[pos] = node.values[pos - 1];
            --pos;
        }
        node.keys[pos]   = key;
        node.values[pos] = value;
        ++node.keyCount;

        if (node.keyCount <= BTreeNode::ORDER) {
            saveNode(nodeId, node);
            return std::nullopt; // no split needed
        }

        // Node is full — split it
        return splitNode(nodeId, node);
    }

    // Internal node — find child to recurse into
    int i = 0;
    while (i < node.keyCount && key >= node.keys[i]) ++i;

    auto split = insertRec(node.children[i], key, value);
    if (!split.has_value()) return std::nullopt;

    // Child split — insert new key into this node
    int pos = node.keyCount;
    while (pos > i) {
        node.keys[pos]       = node.keys[pos - 1];
        node.children[pos+1] = node.children[pos];
        --pos;
    }
    node.keys[i]       = split->key;
    node.children[i+1] = split->newPage;
    ++node.keyCount;

    if (node.keyCount <= BTreeNode::ORDER) {
        saveNode(nodeId, node);
        return std::nullopt;
    }

    return splitNode(nodeId, node);
}

BTree::SplitResult BTree::splitNode(PageId nodeId, BTreeNode& node) {
    int mid = node.keyCount / 2;

    BTreeNode right;
    right.isLeaf = node.isLeaf;

    if (node.isLeaf) {
        // Leaf split: right node gets mid..end
        right.keyCount = node.keyCount - mid;
        for (int i = 0; i < right.keyCount; ++i) {
            right.keys[i]   = node.keys[mid + i];
            right.values[i] = node.values[mid + i];
        }
        node.keyCount = mid;

        // Maintain leaf linked list
        right.nextLeaf = node.nextLeaf;
        PageId rightId = allocNode();
        node.nextLeaf  = rightId;

        saveNode(nodeId, node);
        saveNode(rightId, right);

        // Push up the first key of right node
        return {right.keys[0], rightId};
    }

    // Internal node split
    std::string pushUpKey = node.keys[mid];
    right.keyCount        = node.keyCount - mid - 1;
    right.children[0]     = node.children[mid + 1];

    for (int i = 0; i < right.keyCount; ++i) {
        right.keys[i]       = node.keys[mid + 1 + i];
        right.children[i+1] = node.children[mid + 2 + i];
    }
    node.keyCount = mid;

    PageId rightId = allocNode();
    saveNode(nodeId, node);
    saveNode(rightId, right);

    return {pushUpKey, rightId};
}

} // namespace mydb

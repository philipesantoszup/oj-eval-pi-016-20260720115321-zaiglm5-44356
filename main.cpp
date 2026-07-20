#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>

using namespace std;

const int ORDER = 64; // B+ Tree order
const int MAX_KEYS = ORDER - 1;
const int MIN_KEYS = ORDER / 2;

struct IndexKey {
    char data[68]; // index string (max 64) + padding
    
    IndexKey() { memset(data, 0, sizeof(data)); }
    IndexKey(const string& s) {
        memset(data, 0, sizeof(data));
        strncpy(data, s.c_str(), 64);
    }
    
    bool operator<(const IndexKey& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(const IndexKey& other) const {
        return strcmp(data, other.data) == 0;
    }
    bool operator>(const IndexKey& other) const {
        return strcmp(data, other.data) > 0;
    }
    bool operator<=(const IndexKey& other) const {
        return strcmp(data, other.data) <= 0;
    }
    
    string str() const { return string(data); }
};

// Use memory-based B+ Tree with multimap behavior
// For each key, store sorted list of values
// Persist to file on exit

template<typename Key, typename Value>
class BPTree {
private:
    struct Node {
        bool is_leaf;
        int num_keys;
        Key keys[ORDER];
        Node* children[ORDER + 1];
        vector<Value>* values[ORDER]; // Only for leaf nodes
        Node* next; // For leaf nodes (linked list)
        
        Node(bool leaf = true) : is_leaf(leaf), num_keys(0), next(nullptr) {
            for (int i = 0; i < ORDER; i++) values[i] = nullptr;
            for (int i = 0; i <= ORDER; i++) children[i] = nullptr;
        }
    };
    
    Node* root;
    
    // Find the leaf node where key should be
    Node* findLeaf(const Key& key) {
        if (!root) return nullptr;
        Node* node = root;
        while (!node->is_leaf) {
            int i = 0;
            while (i < node->num_keys && key > node->keys[i]) i++;
            node = node->children[i];
        }
        return node;
    }
    
    // Split a leaf node
    void splitLeaf(Node* leaf) {
        int mid = leaf->num_keys / 2;
        Node* new_leaf = new Node(true);
        
        // Move second half to new leaf
        for (int i = mid; i < leaf->num_keys; i++) {
            new_leaf->keys[i - mid] = leaf->keys[i];
            new_leaf->values[i - mid] = leaf->values[i];
        }
        new_leaf->num_keys = leaf->num_keys - mid;
        leaf->num_keys = mid;
        
        new_leaf->next = leaf->next;
        leaf->next = new_leaf;
        
        insertIntoParent(leaf, new_leaf->keys[0], new_leaf);
    }
    
    // Split an internal node
    void splitInternal(Node* node) {
        int mid = node->num_keys / 2;
        Node* new_internal = new Node(false);
        
        Key mid_key = node->keys[mid];
        
        // Move second half to new internal
        for (int i = mid + 1; i < node->num_keys; i++) {
            new_internal->keys[i - mid - 1] = node->keys[i];
            new_internal->children[i - mid - 1] = node->children[i];
        }
        new_internal->children[node->num_keys - mid - 1] = node->children[node->num_keys];
        new_internal->num_keys = node->num_keys - mid - 1;
        node->num_keys = mid;
        
        insertIntoParent(node, mid_key, new_internal);
    }
    
    // Insert a key and child into parent
    void insertIntoParent(Node* left, const Key& key, Node* right) {
        if (left == root) {
            Node* new_root = new Node(false);
            new_root->keys[0] = key;
            new_root->children[0] = left;
            new_root->children[1] = right;
            new_root->num_keys = 1;
            root = new_root;
            return;
        }
        
        // Find parent
        Node* parent = findParent(root, left);
        
        // Find position to insert
        int i = 0;
        while (i < parent->num_keys && parent->children[i] != left) i++;
        
        // Shift keys and children
        for (int j = parent->num_keys; j > i; j--) {
            parent->keys[j] = parent->keys[j - 1];
            parent->children[j + 1] = parent->children[j];
        }
        
        parent->keys[i] = key;
        parent->children[i + 1] = right;
        parent->num_keys++;
        
        if (parent->num_keys >= ORDER) {
            splitInternal(parent);
        }
    }
    
    // Find parent of a node
    Node* findParent(Node* current, Node* target) {
        if (current->is_leaf) return nullptr;
        
        for (int i = 0; i <= current->num_keys; i++) {
            if (current->children[i] == target) return current;
            if (!current->children[i]->is_leaf) {
                Node* result = findParent(current->children[i], target);
                if (result) return result;
            }
        }
        return nullptr;
    }
    
public:
    BPTree() : root(nullptr) {}
    
    void insert(const Key& key, const Value& value) {
        if (!root) {
            root = new Node(true);
            root->keys[0] = key;
            root->values[0] = new vector<Value>();
            root->values[0]->push_back(value);
            root->num_keys = 1;
            return;
        }
        
        Node* leaf = findLeaf(key);
        
        // Find position in leaf
        int i = 0;
        while (i < leaf->num_keys && leaf->keys[i] < key) i++;
        
        if (i < leaf->num_keys && leaf->keys[i] == key) {
            // Key exists, add value if not duplicate
            auto& vals = *(leaf->values[i]);
            if (std::find(vals.begin(), vals.end(), value) == vals.end()) {
                vals.insert(std::lower_bound(vals.begin(), vals.end(), value), value);
            }
        } else {
            // New key
            // Shift keys and values right
            for (int j = leaf->num_keys; j > i; j--) {
                leaf->keys[j] = leaf->keys[j - 1];
                leaf->values[j] = leaf->values[j - 1];
            }
            leaf->keys[i] = key;
            leaf->values[i] = new vector<Value>();
            leaf->values[i]->push_back(value);
            leaf->num_keys++;
            
            if (leaf->num_keys >= ORDER) {
                splitLeaf(leaf);
            }
        }
    }
    
    bool erase(const Key& key, const Value& value) {
        if (!root) return false;
        
        Node* leaf = findLeaf(key);
        if (!leaf) return false;
        
        for (int i = 0; i < leaf->num_keys; i++) {
            if (leaf->keys[i] == key) {
                auto& vals = *(leaf->values[i]);
                auto it = std::find(vals.begin(), vals.end(), value);
                if (it == vals.end()) return false;
                vals.erase(it);
                
                // If no values left, remove the key
                if (vals.empty()) {
                    delete leaf->values[i];
                    for (int j = i; j < leaf->num_keys - 1; j++) {
                        leaf->keys[j] = leaf->keys[j + 1];
                        leaf->values[j] = leaf->values[j + 1];
                    }
                    leaf->num_keys--;
                }
                return true;
            }
        }
        return false;
    }
    
    vector<Value>* find(const Key& key) {
        Node* leaf = findLeaf(key);
        if (!leaf) return nullptr;
        
        for (int i = 0; i < leaf->num_keys; i++) {
            if (leaf->keys[i] == key) {
                return leaf->values[i];
            }
        }
        return nullptr;
    }
    
    void saveToFile(const string& filename) {
        ofstream out(filename, ios::binary);
        if (!out) return;
        
        // Traverse all leaf nodes
        Node* leaf = root;
        if (!leaf) return;
        
        while (!leaf->is_leaf) leaf = leaf->children[0];
        
        while (leaf) {
            for (int i = 0; i < leaf->num_keys; i++) {
                string key_str = leaf->keys[i].str();
                int key_len = key_str.length();
                out.write((char*)&key_len, sizeof(key_len));
                out.write(key_str.c_str(), key_len);
                
                int num_vals = leaf->values[i]->size();
                out.write((char*)&num_vals, sizeof(num_vals));
                for (int v : *(leaf->values[i])) {
                    out.write((char*)&v, sizeof(v));
                }
            }
            leaf = leaf->next;
        }
        out.close();
    }
    
    void loadFromFile(const string& filename) {
        ifstream in(filename, ios::binary);
        if (!in) return;
        
        while (in.peek() != EOF) {
            int key_len;
            in.read((char*)&key_len, sizeof(key_len));
            if (in.eof()) break;
            
            string key_str(key_len, '\0');
            in.read(&key_str[0], key_len);
            
            int num_vals;
            in.read((char*)&num_vals, sizeof(num_vals));
            
            for (int i = 0; i < num_vals; i++) {
                int val;
                in.read((char*)&val, sizeof(val));
                insert(IndexKey(key_str), val);
            }
        }
        in.close();
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    BPTree<IndexKey, int> tree;
    
    // Load existing data
    tree.loadFromFile("database.dat");
    
    int n;
    cin >> n;
    
    string cmd;
    for (int i = 0; i < n; i++) {
        cin >> cmd;
        
        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            tree.insert(IndexKey(index), value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            tree.erase(IndexKey(index), value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            
            vector<int>* result = tree.find(IndexKey(index));
            if (result == nullptr || result->empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < result->size(); j++) {
                    if (j > 0) cout << " ";
                    cout << (*result)[j];
                }
                cout << "\n";
            }
        }
    }
    
    // Save data
    tree.saveToFile("database.dat");
    
    return 0;
}

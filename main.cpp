#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cstring>

using namespace std;

// Simple approach using std::map with file persistence
struct IndexKey {
    char data[68];
    
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
    
    string str() const { return string(data); }
};

class FileDB {
private:
    map<IndexKey, vector<int>> data;
    
public:
    void loadFromFile(const string& filename) {
        ifstream in(filename, ios::binary);
        if (!in) return;
        
        while (in.peek() != EOF) {
            int key_len;
            in.read((char*)&key_len, sizeof(key_len));
            if (in.eof() || in.gcount() == 0) break;
            
            string key_str(key_len, '\0');
            in.read(&key_str[0], key_len);
            
            int num_vals;
            in.read((char*)&num_vals, sizeof(num_vals));
            
            vector<int> vals;
            for (int i = 0; i < num_vals; i++) {
                int val;
                in.read((char*)&val, sizeof(val));
                vals.push_back(val);
            }
            
            IndexKey key(key_str);
            data[key] = vals;
        }
        in.close();
    }
    
    void saveToFile(const string& filename) {
        ofstream out(filename, ios::binary);
        if (!out) return;
        
        for (auto& [key, vals] : data) {
            if (vals.empty()) continue;
            
            string key_str = key.str();
            int key_len = key_str.length();
            out.write((char*)&key_len, sizeof(key_len));
            out.write(key_str.c_str(), key_len);
            
            int num_vals = vals.size();
            out.write((char*)&num_vals, sizeof(num_vals));
            for (int v : vals) {
                out.write((char*)&v, sizeof(v));
            }
        }
        out.close();
    }
    
    void insert(const IndexKey& key, int value) {
        auto it = data.find(key);
        if (it == data.end()) {
            data[key] = vector<int>{value};
        } else {
            auto& vals = it->second;
            auto vit = std::lower_bound(vals.begin(), vals.end(), value);
            if (vit == vals.end() || *vit != value) {
                vals.insert(vit, value);
            }
        }
    }
    
    void erase(const IndexKey& key, int value) {
        auto it = data.find(key);
        if (it == data.end()) return;
        
        auto& vals = it->second;
        auto vit = std::lower_bound(vals.begin(), vals.end(), value);
        if (vit != vals.end() && *vit == value) {
            vals.erase(vit);
        }
        
        if (vals.empty()) {
            data.erase(it);
        }
    }
    
    vector<int>* find(const IndexKey& key) {
        auto it = data.find(key);
        if (it == data.end()) return nullptr;
        return &it->second;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    FileDB db;
    
    // Load existing data
    db.loadFromFile("database.dat");
    
    int n;
    cin >> n;
    
    string cmd;
    for (int i = 0; i < n; i++) {
        cin >> cmd;
        
        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            db.insert(IndexKey(index), value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            db.erase(IndexKey(index), value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            
            vector<int>* result = db.find(IndexKey(index));
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
    db.saveToFile("database.dat");
    
    return 0;
}

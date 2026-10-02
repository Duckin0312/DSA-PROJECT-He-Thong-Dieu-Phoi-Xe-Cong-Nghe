#pragma once
#include "VietnameseFixed.hpp"
#include <algorithm>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <functional>

struct Location{
    std::string name;
    double posX;
    double posY;
    int population;
};

struct TrieNode{
    std::unordered_map<char, TrieNode*> children;
    bool endOfWord = false;
    Location location;
    std::vector<Location> topSuggestLocation;
    std::vector<Location> fixedTopSuggestLocation;
};

extern double userPosX, userPosY;

class AutoComplete{
    private:
        TrieNode* root;
        const int MAXSUGGESTLOCATION = 5;

        void FreeNode(TrieNode* node);
        void Traverse(const TrieNode* node, std::string& prefix,
                      const std::function<void(const std::string&, const TrieNode&)>& visit) const;

    public:
        AutoComplete(){ root = new TrieNode; }
        ~AutoComplete(){ FreeNode(root); }
        AutoComplete(const AutoComplete&) = delete;
        AutoComplete& operator=(const AutoComplete&) = delete;

        Location ConvertFixed(Location location);
        void UpdateSuggestLocation(TrieNode* node, const Location& location);
        void InsertLocation(const Location& location);
        std::vector<Location> SearchPrefix(std::string prefix);

        // Mới
        void Clear();
        void ForEachNode(const std::function<void(const std::string&, const TrieNode&)>& visit) const;
        int GetMaxSuggest() const { return MAXSUGGESTLOCATION; }
};
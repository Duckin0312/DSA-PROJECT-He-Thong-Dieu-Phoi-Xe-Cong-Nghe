#pragma once
#include "VietnameseFixed.hpp"
#include <algorithm>
#include <vector>
#include <cmath>
#include <unordered_map>

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
        
    public:
        //Constructor
        AutoComplete(){
            root = new TrieNode;
        }
        
        //Convert from vietnamese to english alphabet
        Location ConvertFixed(Location location);

        //Update suggest locations from each node in Trie
        void UpdateSuggestLocation(TrieNode* node, const Location& location);

        void InsertLocation(const Location& location);

        std::vector<Location> SearchPrefix(std::string prefix);
};
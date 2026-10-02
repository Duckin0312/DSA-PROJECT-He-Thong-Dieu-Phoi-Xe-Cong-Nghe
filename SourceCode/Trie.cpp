#include "Trie.hpp"

using namespace std;

double userPosX = 0.0, userPosY = 0.0;

Location AutoComplete::ConvertFixed(Location location){
    Location fixedLocation = location;
    fixedLocation.name = toLower(location.name);
    return fixedLocation;
}

void AutoComplete::UpdateSuggestLocation(TrieNode* node, const Location& location){
    node->topSuggestLocation.push_back(location);
    
    Location fixedLocation = ConvertFixed(location);

    node->fixedTopSuggestLocation.push_back(fixedLocation);

    auto calculateScore = [](const Location& location){
        double distance = std::hypot(userPosX - location.posX, userPosY - location.posY);
        return 0.2*location.population - 0.8*distance;
    };

    //Sort suggest locations by population and dictionary 
    std::sort(node->topSuggestLocation.begin(), node->topSuggestLocation.end(), [&](Location& first, Location& second){
        double scoreFirst = calculateScore(first);
        double scoreSecond = calculateScore(second);
        if(scoreFirst == scoreSecond){
            return first.name < second.name;
        }
        return scoreFirst > scoreSecond;
    });

    std::sort(node->fixedTopSuggestLocation.begin(), node->fixedTopSuggestLocation.end(), [&](Location& first, Location& second){
        double scoreFirst = calculateScore(first);
        double scoreSecond = calculateScore(second);
        if(scoreFirst == scoreSecond){
            return first.name < second.name;
        }
        return scoreFirst > scoreSecond;
    });

    //Limit top suggest locations to 5, pop the least population one
    if(node->topSuggestLocation.size() > MAXSUGGESTLOCATION){
        node->topSuggestLocation.pop_back();
        node->fixedTopSuggestLocation.pop_back();
    }
}

void AutoComplete::InsertLocation(const Location& location){
    TrieNode* current = root;
    
    UpdateSuggestLocation(root, location);

    std::string fixedLocationName = toLower(location.name);

    for(char ch : fixedLocationName){
        if(current->children.find(ch) == current->children.end()){
            current->children[ch] = new TrieNode;
        }
        current = current->children[ch];

        UpdateSuggestLocation(current, location);
    }

    current->endOfWord = true;
    current->location = location;
}

vector<Location> AutoComplete::SearchPrefix(std::string prefix){
    TrieNode* current = root;

    std::string fixedPrefix = toLower(prefix);

    for(char ch : fixedPrefix){
        if(current->children.find(ch) == current->children.end()){
            return {};
        }
        current = current->children[ch];
    }

    return current->topSuggestLocation;
}

void AutoComplete::FreeNode(TrieNode* node){
    if(!node) return;
    for(auto& item : node->children) FreeNode(item.second);
    delete node;
}

void AutoComplete::Clear(){
    FreeNode(root);
    root = new TrieNode;
}

void AutoComplete::Traverse(const TrieNode* node, std::string& prefix,
                            const std::function<void(const std::string&, const TrieNode&)>& visit) const{
    visit(prefix, *node);
    for(const auto& item : node->children){
        prefix.push_back(item.first);
        Traverse(item.second, prefix, visit);
        prefix.pop_back();
    }
}

void AutoComplete::ForEachNode(const std::function<void(const std::string&, const TrieNode&)>& visit) const{
    std::string prefix;
    Traverse(root, prefix, visit);
}
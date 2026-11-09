#include "../include/Commit.h"
#include <map>
#include <string>
#include <iostream>
#include <fstream>

// 建议类 Commit 的实现文件，可按需使用或删除。

Commit Commit::deserialize(const std::string& commitHash) {
    Commit c;
    std::fstream file(".gitlite/objects/" + commitHash);
    std::string line;
    bool isReadingMessage = false;
    while(std::getline(file, line)) {
        if(isReadingMessage) {
            c.message += "\n" + line;
            continue;
        }
        if(line.rfind("tree ", 0) == 0) {
            c.treeID = line.substr(5);
        } else if(line.rfind("parent ", 0) == 0) {
            c.parents.push_back(line.substr(7));
        } else if(line.rfind("time ", 0) == 0) {
            c.timestamp = std::stoll(line.substr(5));
        } else if(line.rfind("message ", 0) == 0) {
            c.message = line.substr(8); 
            isReadingMessage = true;
        }
    }
    return c;
}

std::string Commit::serialize() const {
    std::string out = "tree " + treeID + "\n";
    for(const auto& p : parents) {
        out += "parent" + p + "\n";
    }
    out += "time " + std::to_string(timestamp) + "\n";
    out += "message " + message + "\n";
    return out;
} 
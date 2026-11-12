#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include <map>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <queue>

// main.cpp 的 checkCWD 依赖此方法，请保留。
std::string Repository::getGitliteDir() {
    return ".gitlite";
}

//我写完才发现有 getGitliteDir，以后再改吧

std::string Repository::getCurrentBranch() {
    std::ifstream headFile(".gitlite/HEAD");
    std::string headContent;
    if(!std::getline(headFile, headContent)) return "";
    return Utils::trimTrailingNewlines(headContent.substr(16));
}

std::map<std::string, std::string> Repository::getHeadTree() {
    //把当前的提交快照拿出来
    std::map<std::string, std::string> headFiles;
    std::ifstream headFile(".gitlite/HEAD");
    std::string headContent;
    if(!std::getline(headFile, headContent)) return headFiles;

    std::string refPath = ".gitlite/" + headContent.substr(5);
    std::ifstream refFile(refPath);
    std::string commitHash;
    if(!std::getline(refFile, commitHash)) return headFiles;

    std::ifstream commitFile(".gitlite/objects/" + commitHash);
    std::string line;
    std::string treeHash;
    while(std::getline(commitFile, line)) {
        if(line.rfind("tree ", 0) == 0) {
            treeHash = line.substr(5);
            break ;
        }
    }

    std::ifstream treeFile(".gitlite/objects/" + treeHash);
    std::string hash, filename;
    while(treeFile >> hash >> filename) {
        headFiles[filename] = hash;
    }
    return  headFiles;
}

std::map<std::string, std::pair<char, std::string>> Repository::readIndex() {
    //把当前的清单拿出来
    std::map<std::string, std::pair<char, std::string>> idx;
    std::ifstream f(".gitlite/index");
    char state;
    std::string hash, filename;
    while(f >> state >> hash >> filename) {
        idx[filename] = {state, hash};
    }
    return idx;
}

void Repository::writeIndex(const std::map<std::string, std::pair<char, std::string>>& idx) {
    //写清单
    std::ofstream f(".gitlite/index");
    for(auto const& [filename, info] : idx) {
        f << info.first << " " << info.second << " " << filename << "\n";
    }
}

std::string Repository::getCurrentCommitHash() {
    //把提交快照的指针拿出来
    std::ifstream headFile(".gitlite/HEAD");
    std::string headContent;
    if(!std::getline(headFile, headContent)) return "";

    std::string refPath = ".gitlite/" + headContent.substr(5);
    std::ifstream refFile(refPath);
    std::string commitHash;
    std::getline(refFile, commitHash);
    return commitHash;
}

std::map<std::string, std::string> Repository::getTreeFromCommit(const std::string& commitHash) {
    //从提交指针把提交清单拿出来
    std::map<std::string, std::string> treeFiles;
    if(commitHash.empty()) return treeFiles;

    std::ifstream commitFile(".gitlite/objects/" + commitHash);
    std::string line, treeHash;
    while(std::getline(commitFile, line)) {
        if(line.rfind("tree ", 0) == 0) {
            treeHash = line.substr(5);
            break ;
        }
    }

    if(!treeHash.empty()) {
        std::ifstream treeFile(".gitlite/objects/" + treeHash);
        std::string blobHash, filename;
        while(treeFile >> blobHash >> filename) {
            treeFiles[filename] = blobHash;
        }
    }
    return treeFiles;
}

bool Repository::matchPattern(const std::string& pattern, const std::string& filename) {
    int p = 0, f = 0;
    int starIdx = -1, matchIdx = 0;
    while(f < filename.size()) {
        if(p < pattern.size() && ( pattern[p] == '?' || pattern[p] == filename[f])) {
            p++; f++;
        } else if(p + 1 < pattern.size() && pattern[p] == '\\' && pattern[p + 1] == filename[f]) {
            p += 2; f++;
        } else if(p < pattern.size() && pattern[p] == '*') {
            starIdx = p;
            matchIdx = f;
            p++;
        } else if(starIdx != -1) {
            p = starIdx + 1;
            matchIdx++;
            f = matchIdx;
        } else {
            return false;
        }
    }

    while(p < pattern.size() && pattern[p] == '*') p++;
    return p == pattern.size();
}

bool Repository::IsFileIgnored(const std::string& filename) {
    if(!Utils::exists(".gitliteignore")) return false;

    std::ifstream file(".gitliteignore");
    std::string line;
    bool ignored = false;
    while(std::getline(file, line)) {
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) continue;
        if(line[0] == '#' && (line.size() < 2 || line[1] != '#')) continue;
        
        bool is_negation = false;
        if(line[0] == '!' && (line.size() < 2 || line[1] != '!')) {
            is_negation = true;
            line = line.substr(1);
        }

        if(line[0] == '/') {
            line = line.substr(1);
        }
        if(Repository::matchPattern(line, filename)) {
            ignored = !is_negation;
        }
    } 
    return ignored;
}
std::vector<std::string> Repository::getAllRefs() {
    std::vector<std::string> refs;
    std::string headsPath = ".gitlite/refs/heads";
    // heads
    if(Utils::exists(headsPath)) {
        auto allfile = Utils::plainFilenamesIn(headsPath);
        for(const auto& name : allfile) {
            std::ifstream f(headsPath + "/" + name);
            std::string hash;
            if(std::getline(f, hash) && !hash.empty()) {
                refs.push_back(Utils::trimTrailingNewlines(hash));
            }
        }
    }
    // tags
    std::string tagsPath = ".gitlite/refs/tags";
    if (Utils::exists(tagsPath)) {
        auto allfile = Utils::plainFilenamesIn(tagsPath);
        for (const auto& name : allfile) {
            std::ifstream f(tagsPath + "/" + name);
            std::string hash;
            if (std::getline(f, hash) && !hash.empty()) {
                refs.push_back(Utils::trimTrailingNewlines(hash));
            }
        }
    }
    // 此处有远程分支

    return refs;
}

bool Repository::isValidName(const std::string& name) {
    return std::regex_match(name, std::regex("[A-Za-z0-9][A-Za-z0-9._-]*"));
}

std::string Repository::resolveRevision(const std::string& revision) {
    std::string tagPath = ".gitlite/refs/tags/" + revision;
    if(Utils::exists(tagPath)) {
        std::string hash = Utils::readContentsAsString(tagPath);
        return Utils::trimTrailingNewlines(hash);
    }

    std::vector<std::string> matches;
    auto allfile = Utils::plainFilenamesIn(".gitlite/objects");
    for(const auto& filename : allfile) {
        if(filename.size() == 40 && filename.rfind(revision, 0) == 0) { 
            std::string content = Utils::readContentsAsString(".gitlite/objects/" + filename);
            if(content.rfind("tree ", 0) == 0) {
                matches.push_back(filename);
            }
        }
    }

    if(!matches.empty()) {
        std::sort(matches.begin(), matches.end());
        return matches[0];
    }
    return "";
}

std::set<std::string> Repository::findAncestors(const std::string& commitHash) {
    std::set<std::string> Ancestors;
    std::queue<std::string> q;
    q.push(commitHash);
    Ancestors.insert(commitHash);
    while(!q.empty()) {
        std::string Hash = q.front();
        q.pop();
        Commit c = Commit::deserialize(Hash);
        for(const std::string& parent : c.parents) {
            if(Ancestors.find(parent) == Ancestors.end()) {
                Ancestors.insert(parent);
                q.push(parent);
            }
        }
    }
    return Ancestors;
}

std::string Repository::getSplitPoint(const std::string& currentHash, const std::string& targetHash) {
    auto currentAncestors = Repository::findAncestors(currentHash);
    if(currentAncestors.count(targetHash)) return targetHash;
    std::queue<std::string> q;
    std::set<std::string> visited;
    q.push(targetHash); visited.insert(targetHash);
    while(!q.empty()) {
        std::string Hash = q.front();
        q.pop();
        Commit c = Commit::deserialize(Hash);
        for(const std::string& parent : c.parents) {
            if(currentAncestors.find(parent) == currentAncestors.end()) {
                if(!visited.count(parent)) {
                    q.push(parent);
                    visited.insert(parent);
                }
            } else {
                return parent;
            }
        }
    }
    return "";
}

std::string Repository::getFileContent(const std::map<std::string, std::string>& tree, const std::string filename) {
    auto it = tree.find(filename);
    if(it == tree.end()) return "";
    auto Hash = it -> second;
    if(Hash.empty()) return "";
    std::string Content = Utils::readContentsAsString(".gitlite/objects/" + Hash);
    return Content;
}

std::map<std::string, std::string> Repository::getCommitSnapshot(const std::string& commitHash) {
    std::map<std::string, std::string> snapshot; //[filename] [content]
    auto tree = Repository::getTreeFromCommit(commitHash);
    for(const auto& [filename, blobhash] : tree) {
        std::string content = Utils::readContentsAsString(".gitlite/objects/" + blobhash);
        snapshot[filename] = content;
    }
    return snapshot;
}

std::map<std::string, std::string> Repository::getWorkDirSnapshot() {
    std::map<std::string, std::string> snapshot;
    auto files = Utils::plainFilenamesIn(".");
    for(const auto& f : files) {
        if(f == ".gitlite") continue ;
        if(!Utils::isFile(f)) continue ;
        snapshot[f] = Utils::readContentsAsString(f);
    }
    return snapshot;
}

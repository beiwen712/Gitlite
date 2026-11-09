#include "../include/SomeObj.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include "../include/Repository.h"
#include <set>
#include <queue>
#include <filesystem>
#include <map>
#include <string>

SomeObj::SomeObj() {}

// ---------------- Subtask 1：init / add / commit / rm ----------------

void SomeObj::init() {
    if(Utils::isDirectory(".gitlite")) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }

    Utils::createDirectories(".gitlite/objects");
    Utils::createDirectories(".gitlite/refs/heads");

    std::ofstream indexFile(".gitlite/index");
    indexFile.close();
    
    std::string emptyTreeHash = Utils::sha1("");
    std::string commitContent = "tree " + emptyTreeHash + "\n";
    commitContent += "time 0\n";
    commitContent += "message initial commit\n";

    std::string initialCommitHash = Utils::sha1(commitContent);
    std::ofstream commitFile(".gitlite/objects/" + initialCommitHash);
    
    commitFile << commitContent;
    commitFile.close();

    std::ofstream masterFile(".gitlite/refs/heads/master");
    masterFile << initialCommitHash << "\n";
    masterFile.close();

    std::ofstream headFile(".gitlite/HEAD");
    headFile << "ref: refs/heads/master\n";
    headFile.close();
    // TODO: 创建 .gitlite 目录、initial commit 与 master 分支
}

void SomeObj::add(const std::string& f) {
    // TODO: 将文件的当前内容加入暂存区
    if(!Utils::exists(f)) {
        Utils::exitWithMessage("File does not exist.");
    }
    auto headFiles = Repository::getHeadTree();
    auto idx = Repository::readIndex();

    bool isTrackedOrStaged = headFiles.count(f) || idx.count(f);
    if(!isTrackedOrStaged && Repository::IsFileIgnored(f)) {
        return;
    }

    std::string content = Utils::readContentsAsString(f);
    std::string hash = Utils::sha1(content);
    Utils::writeContents(".gitlite/objects/" + hash, content);

    if(headFiles.count(f) && headFiles[f] == hash) {
        idx.erase(f);
    } else {
        idx[f] = {'A', hash};
    }
    Repository::writeIndex(idx);
}

void SomeObj::commit(const std::string& m) {
    // TODO: 将暂存区保存为新提交，并推进当前分支
    if(m.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }
    auto idx = Repository::readIndex();
    if(idx.empty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }
    
    std::string parentHash = Repository::getCurrentCommitHash();
    std::map<std::string, std::string> newTree = Repository::getTreeFromCommit(parentHash);

    for(auto const& [filename, info] : idx) {
        char status = info.first;
        std::string hash = info.second;

        if(status == 'A') {
            newTree[filename] = hash;
        } else if(status == 'R') {
            newTree.erase(filename);
        }
    }

    std::string treeContent = "";
    for(auto const& [filename, hash] : newTree) {
        treeContent += hash + " " + filename + "\n";
    }

    std::string newTreeHash = Utils::sha1(treeContent);
    Utils::writeContents(".gitlite/objects/" + newTreeHash, treeContent);
    //写tree文件

    std::string commitContent = "tree " + newTreeHash + "\n";
    if(!parentHash.empty()) {
        commitContent += "parent " + parentHash + "\n";
    }
    
    std::time_t now = std::time(nullptr);
    commitContent += "time " + std::to_string(now) + "\n";
    commitContent += "message " + m + "\n";

    std::string newCommitHash = Utils::sha1(commitContent);
    Utils::writeContents(".gitlite/objects/" + newCommitHash, commitContent);
    //写commit文件

    std::ifstream headFile(".gitlite/HEAD");
    std::string headContent;
    if(std::getline(headFile, headContent)) {
        if(headContent.rfind("ref: ", 0) == 0) {
            std::string refPath = headContent.substr(5);
            std::ofstream branchFile(".gitlite/" + refPath);
            branchFile << newCommitHash << std::endl;
            branchFile.close();
        }
    }

    std::map<std::string, std::pair<char, std::string>> emptyIdx;
    Repository::writeIndex(emptyIdx);
}

void SomeObj::rm(const std::string& f) {
    // TODO: 将文件取消暂存，或标记为待删除
    auto headFiles = Repository::getHeadTree();
    auto idx = Repository::readIndex();

    bool inIndex = (idx.find(f) != idx.end());
    bool inHead = (headFiles.find(f) != headFiles.end());

    if(!inIndex && !inHead) {
        Utils::exitWithMessage("No reason to remove the file.");
    }
    
    if(inHead) {
        idx[f] = {'R', "-"};
        if(Utils::exists(f)) {
            Utils::restrictedDelete(f);
        }
    } else if(inIndex) {
        idx.erase(f);
    }

    Repository::writeIndex(idx);
}

// ---------------- Subtask 2：log / global-log / tag ----------------

void SomeObj::log() {
    // TODO: 从当前提交沿第一个父提交打印日志
    auto currentHash = Repository::getCurrentCommitHash();
    while(!currentHash.empty()) {
        Commit c = Commit::deserialize(currentHash);
        Utils::message("===");
        Utils::message("commit " + currentHash);
        if(c.parents.size() >= 2) {
            Utils::message("Merge: " + c.parents[0].substr(0, 7) + " " + c.parents[1].substr(0, 7));
        }
        Utils::message("Date: " + SomeObj::formatDate(c.timestamp));
        Utils::message(c.message + "\n");
        if(!c.parents.empty()) currentHash = c.parents[0];
        else break;
    }

}
namespace {
    struct LogNode{
        std::string hash;
        Commit commit;
    };
}
void SomeObj::globalLog() {
    // TODO: 从所有本地分支、远程跟踪分支和标签出发，输出全部可达提交并按规范排序
    std::vector<std::string> starts = Repository::getAllRefs();
    // Utils::message(std::to_string(starts.size()));
    std::vector<LogNode> allCommits;
    std::set<std::string> visited;
    std::queue<std::string> q;

    for(const auto& hash : starts) {
        if(!hash.empty() && visited.find(hash) == visited.end()) {
            visited.insert(hash);
            q.push(hash);
        }
    }

    while(!q.empty()) {
        std::string currentHash = q.front();
        q.pop();

        Commit c = Commit::deserialize(currentHash);
        allCommits.push_back({currentHash, c});

        for(const std::string& parent : c.parents) {
            if(visited.find(parent) == visited.end()) {
                visited.insert(parent);
                q.push(parent);
            }
        }
    }

    std::sort(allCommits.begin(), allCommits.end(), [](const LogNode& a, const LogNode& b) {
        if(a.commit.timestamp != b.commit.timestamp) {
            return a.commit.timestamp > b.commit.timestamp;
        }
        return a.hash < b.hash;   
    });
    for(const auto& node : allCommits) {
        Utils::message("===");
        Utils::message("commit " + node.hash);
        if(node.commit.parents.size() >= 2) {
            Utils::message("Merge: " + node.commit.parents[0].substr(0, 7)
                                     + node.commit.parents[1].substr(0, 7));
        }
        Utils::message("Date: " + SomeObj::formatDate(node.commit.timestamp));
        Utils::message(node.commit.message + "\n");
    }
}

void SomeObj::listTags() {
    // TODO: 按字典序打印所有标签及其目标提交 id
    std::string tagsPath = ".gitlite/refs/tags";
    if(!Utils::isDirectory(tagsPath)) return ;

    std::map<std::string, std::string> tags;
    auto allfile = Utils::plainFilenamesIn(tagsPath);
    for(const auto& name : allfile) {
        std::string hash = Utils::readContentsAsString(tagsPath + "/" + name);
        tags[name] = Utils::trimTrailingNewlines(hash);
    }
    for(const auto& [name, hash] : tags) {
        Utils::message(name + " " + hash);
    }
}

void SomeObj::tag(const std::string& tagName) {
    // TODO: 为当前提交创建轻量标签
    std::string headHash = Repository::getCurrentCommitHash();
    SomeObj::createTag(tagName, headHash);
}

void SomeObj::tag(const std::string& tagName, const std::string& revision) {
    // TODO: 为指定提交或标签所指提交创建轻量标签
    std::string targetHash = Repository::resolveRevision(revision);
    if(targetHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    SomeObj::createTag(tagName, targetHash);
}

void SomeObj::createTag(const std::string& tagName, const std::string& tagHash) {
    if(!Repository::isValidName(tagName)) {
        Utils::exitWithMessage("Incorrect operands.");
    }
    std::string tagPath = ".gitlite/refs/tags/" + tagName;
    if(Utils::exists(tagPath)) {
        Utils::exitWithMessage("A tag with that name already exists.");
    }
    Utils::createDirectories(".gitlite/refs/tags");
    Utils::writeContents(tagPath, tagHash + "\n");
}

// ---------------- Subtask 3：status / branch / rm-branch ----------------

void SomeObj::status() {
    // TODO: 显示分支、暂存区、未暂存修改与未跟踪文件
    auto headFiles = Repository::getHeadTree();
    auto idx = Repository::readIndex();
    std::string currentBranch = Repository::getCurrentBranch();
    // 只考虑了 refs/heads/ 的情况
    
    std::set<std::string> branches;
    std::string headsPath = ".gitlite/refs/heads";
    if(Utils::isDirectory(headsPath)) {
        auto allBranchFiles = Utils::plainFilenamesIn(headsPath);
        for(const auto& b : allBranchFiles) branches.insert(b);
    }

    std::set<std::string> workFiles;
    auto allWorkFiles = Utils::plainFilenamesIn(".");
    for(const auto& filename : allWorkFiles) {
        if(filename == ".gitlite") continue;
        workFiles.insert(filename);
    }
    Utils::message("=== Branches ===");
    for(const auto& b : branches) {
        if(b == currentBranch) 
            Utils::message("*" + b);
        else 
            Utils::message(b);
    }
    Utils::message("");

    Utils::message("=== Staged Files ===");
    for(const auto& [filename, info] : idx) {
        if(info.first == 'A') {
            Utils::message(filename);
        }
    }
    Utils::message("");

    Utils::message("=== Removed Files ===");
    for(const auto& [filename, info] : idx) {
        if(info.first == 'R') {
            Utils::message(filename);
        }
    }
    Utils::message("");

    std::map<std::string, std::string> mods;
    std::set<std::string> candidates;

    for(const auto& [f, h] : headFiles) candidates.insert(f);
    for(const auto& [f, info] : idx) candidates.insert(f);
    for(const auto& f : candidates) {
        bool inHead = headFiles.count(f);
        bool inIdx = idx.count(f);
        bool inWork = workFiles.count(f);
        std::string headHash = inHead ? headFiles[f] : "";
        char idxState = inIdx ? idx[f].first : ' ';
        std::string idxHash = inIdx ? idx[f].second : "";
        if(inWork) {
            std::string workHash = Utils::sha1(Utils::readContentsAsString(f));
            if(inHead && !inIdx && workHash != headHash) {
                mods[f] = "(modified)";
            } else if(inIdx && idxState == 'A' && workHash != idxHash) {
                mods[f] = "(modified)";
            }
        } else {
            if(inHead && !inIdx) {
                mods[f] = "(deleted)";
            } else if(inIdx && idxState == 'A') {
                mods[f] = "(deleted)";
            }
        }
    } 
    Utils::message("=== Modifications Not Staged For Commit ===");
    for(const auto& [f, suffix] : mods) {
        Utils::message(f + " " + suffix);
    }
    Utils::message("");

    Utils::message("=== Untracked Files ===");
    for(const auto& f : workFiles) {
        bool inHead = headFiles.count(f);
        bool inIdx = idx.count(f);
        if(!inHead && !inIdx) {
            if(Repository::IsFileIgnored(f)) continue ;
            Utils::message(f);
        }
        if(inHead && inIdx && idx[f].first == 'R') {
            Utils::message(f);
        }
    }
    Utils::message("");

}

void SomeObj::branch(const std::string& branchName) {
    // TODO: 新建指向当前提交的分支，不切换
    if(!Repository::isValidName(branchName)) {
        Utils::exitWithMessage("Incorrect operands.");
    }
    std::string branchPath = ".gitlite/refs/heads/" + branchName;
    if(Utils::exists(branchPath)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }
    std::string currentHash = Repository::getCurrentCommitHash();
    Utils::createDirectories(".gitlite/refs/heads");
    Utils::writeContents(branchPath, currentHash + "\n");
}

void SomeObj::rmBranch(const std::string& branchName) {
    // TODO: 删除分支指针，不删除其提交
    std::string branchPath = ".gitlite/refs/heads/" + branchName;
    if(!Utils::exists(branchPath)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }
    std::string currentBranch = Repository::getCurrentBranch();
    if(currentBranch == branchName) {
        Utils::exitWithMessage("Cannot remove the current branch.");
    }
    // Utils::restrictedDelete(branchPath);
    // 为什么不能这么删
    std::filesystem::remove(branchPath);
}

// ---------------- Subtask 4：checkout / reset ----------------

bool SomeObj::isUntrackedInTheWay(const std::map<std::string, std::string>& currentTree, const std::map<std::string, std::string>& targetTree) {
    auto workFiles = Utils::plainFilenamesIn(".");
    auto idx = Repository::readIndex();
    for(const auto& [filename, hash] : targetTree) {
        bool inWork = (std::find(workFiles.begin(), workFiles.end(), filename) != workFiles.end());
        bool inCurr = currentTree.count(filename) > 0;
        bool inIdx = idx.count(filename) > 0;
        if(inWork && !inCurr && !inIdx) {
            if(!Repository::IsFileIgnored(filename)) {
                return true;    
            }
        }
    }
    return false;
}

void SomeObj::deleteFilesNotInTarget(const std::map<std::string, std::string>& currentTree, const std::map<std::string, std::string>& targetTree) {
    for(const auto& [filename, hash] : currentTree) {
        if(targetTree.find(filename) == targetTree.end()) {
            Utils::restrictedDelete(filename);
        }
    }
}

void SomeObj::writeFilesWithTarget(const std::map<std::string, std::string>& targetTree) {
    for(const auto& [filename, blobHash] : targetTree) {
        std::string content = Utils::readContentsAsString(".gitlite/objects/" + blobHash);
        Utils::writeContents(filename, content);
    }
}

void SomeObj::clearIndex() {
    std::map<std::string, std::pair<char, std::string>> emptyIdx;
    Repository::writeIndex(emptyIdx);
}

void SomeObj::checkoutBranch(const std::string& branchName) {
    // TODO: 切换到指定分支
    std::string targetBranchPath = ".gitlite/refs/heads/" + branchName;
    if(!Utils::exists(targetBranchPath)) {
        Utils::exitWithMessage("No such branch exists.");
    }

    std::string currentBranch = Repository::getCurrentBranch();
    if(currentBranch == branchName) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }

    std::string targetCommitHash = Utils::trimTrailingNewlines(Utils::readContentsAsString(targetBranchPath));
    auto targetTree = Repository::getTreeFromCommit(targetCommitHash);
    auto currentTree = Repository::getHeadTree();
    // 检察为追踪文件
    if(SomeObj::isUntrackedInTheWay(currentTree, targetTree)) {
        Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
    }
    // 删除当前 Tree 里有，但目标 Tree 没有的
    SomeObj::deleteFilesNotInTarget(currentTree, targetTree);
    // 覆盖原文件
    SomeObj::writeFilesWithTarget(targetTree);
    // 清空 index
    SomeObj::clearIndex();
    // 更新 HEAD 指针
    Utils::writeContents(".gitlite/HEAD", "ref: refs/heads/" + branchName + "\n");
}

void SomeObj::checkoutFile(const std::string& filename) {
    // TODO: 用当前提交中的版本覆盖工作目录中的文件
    auto headTree = Repository::getHeadTree();
    if(!SomeObj::RestoreFileFromTree(filename, headTree)) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }
}

void SomeObj::checkoutFileInCommit(const std::string& revision, const std::string& filename) {
    // TODO: 用指定 revision 的版本覆盖工作文件，支持标签与 id 缩写
    std::string commitHash = Repository::resolveRevision(revision);
    if(commitHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    auto targetTree = Repository::getTreeFromCommit(commitHash);
    if(!SomeObj::RestoreFileFromTree(filename, targetTree)) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }
}

void SomeObj::reset(const std::string& revision) {
    // TODO: 解析 revision，检出指定提交并移动当前分支头
    auto targetHash = Repository::resolveRevision(revision);
    if(targetHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    auto targetTree = Repository::getTreeFromCommit(targetHash);
    auto currentTree = Repository::getHeadTree();
    if(SomeObj::isUntrackedInTheWay(currentTree, targetTree)) {
        Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
    }
    SomeObj::deleteFilesNotInTarget(currentTree, targetTree);
    SomeObj::writeFilesWithTarget(targetTree);
    SomeObj::clearIndex();
    auto branchName = Repository::getCurrentBranch();
    auto branchPath = ".gitlite/refs/heads/" + branchName;
    Utils::writeContents(branchPath, targetHash + "\n");
}

// ---------------- Subtask 5：merge ----------------

void SomeObj::merge(const std::string& targetBranchName) {
    // TODO: 将指定分支合并到当前分支
    // check
    auto idx = Repository::readIndex();
    if(!idx.empty()) {
        Utils::exitWithMessage("You have uncommitted changes.");
    }
    auto targetBranchPath = ".gitlite/refs/heads/" + targetBranchName;
    if(!Utils::exists(targetBranchPath)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }
    auto currentBranchName = Repository::getCurrentBranch();
    if(currentBranchName == targetBranchName) {
        Utils::exitWithMessage("Cannot merge a branch with itself.");
    }
    auto currentCommitHash = Repository::getCurrentCommitHash();
    auto currentTree = Repository::getHeadTree();
    auto targetCommitHash = Utils::trimTrailingNewlines(Utils::readContentsAsString(targetBranchPath));
    auto targetTree = Repository::getTreeFromCommit(targetCommitHash);

    // find ancester
    
    auto currentAncestors = Repository::findAncestors(currentCommitHash);
    auto SplitPoint = Repository::getSplitPoint(currentCommitHash, targetCommitHash);
    // 处理特殊关系
    if(SplitPoint == targetCommitHash) {
        Utils::exitWithMessage("Given branch is an ancestor of the current branch.");
    }
    
    if(SplitPoint == currentCommitHash) {
        if(SomeObj::isUntrackedInTheWay(currentTree, targetTree)) {
            Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
        }
        //Fast-Forward
        SomeObj::reset(targetCommitHash);
        Utils::message("Current branch fast-forwarded.");
        return;
    }
    // 普通合并
    auto baseTree = Repository::getTreeFromCommit(SplitPoint);
    std::map<std::string, std::string> mergedTree = currentTree;
    bool hasConflict = false; 

    std::set<std::string> allFiles;
    for(const auto& [f, h] : baseTree) allFiles.insert(f);
    for(const auto& [f, h] : currentTree) allFiles.insert(f);
    for(const auto& [f, h] : targetTree) allFiles.insert(f);

    std::set<std::string> filesToModify;
    for(const auto& filename : allFiles) {
        std::string currentHash = currentTree.count(filename) ? currentTree[filename] : "";
        std::string targetHash = targetTree.count(filename) ? targetTree[filename] : "";
        std::string baseHash = baseTree.count(filename) ? baseTree[filename] : "";
        if(currentHash == baseHash && targetHash != baseHash) {
            filesToModify.insert(filename);
        }
        else if(targetHash == baseHash) {
            // 当前改了，目标没改
        }
        else if(currentHash == targetHash) {
            // 两边都改了，或者都删了
        } else {
            filesToModify.insert(filename);
        }
    }

    auto workFiles = Utils::plainFilenamesIn(".");
    for(const auto& f : filesToModify) {
        bool inWork = (std::find(workFiles.begin(), workFiles.end(), f) != workFiles.end());
        bool inCur = currentTree.count(f);
        bool inIdx = idx.count(f);
        if(inWork && !inCur && !inIdx) {
            if(!Repository::IsFileIgnored(f)) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }
    }
    for(const auto& filename : allFiles) {
        std::string currentHash = currentTree.count(filename) ? currentTree[filename] : "";
        std::string targetHash = targetTree.count(filename) ? targetTree[filename] : "";
        std::string baseHash = baseTree.count(filename) ? baseTree[filename] : "";

        if(currentHash == baseHash && targetHash != baseHash) {
            filesToModify.insert(filename);
            if(targetHash == "") {
                // 此处貌似不能用
                std::filesystem::remove(filename);
                mergedTree.erase(filename);
            }
                
            else {
                std::string content = Repository::getFileContent(targetTree, filename);
                Utils::writeContents(filename, content);
                mergedTree[filename] = targetHash;
            }
        }
        else if(targetHash == baseHash) {
            // 当前改了，目标没改
        }
        else if(currentHash == targetHash) {
            // 两边都改了，或者都删了
        } else {
            filesToModify.insert(filename);
            hasConflict = true;
            auto currentContent = Repository::getFileContent(currentTree, filename);
            auto targetContent = Repository::getFileContent(targetTree, filename);

            if(!currentContent.empty() && currentContent.back() != '\n') currentContent += '\n';
            if(!targetContent.empty() && targetContent.back() != '\n') targetContent += '\n';
            
            std::string conflict = "<<<<<<< HEAD\n" + currentContent  + "=======\n" + targetContent + ">>>>>>>\n";
            auto newBlobHash = Utils::sha1(conflict);
            Utils::writeContents(".gitlite/objects/" + newBlobHash, conflict);
            Utils::writeContents(filename, conflict);
            mergedTree[filename] = newBlobHash;
        }
    }
    if(hasConflict) {
        Utils::message("Encountered a merge conflict.");
    }

    //生成提交
    std::string treeContent = "";
    for(const auto& [f, h] : mergedTree) {
        treeContent += h + " " + f + "\n";
    }
    std::string newTreeHash = Utils::sha1(treeContent);
    Utils::writeContents(".gitlite/objects/" + newTreeHash, treeContent);

    std::string commitContent = "tree " + newTreeHash + "\n";
    commitContent += "parent " + currentCommitHash + "\n";
    commitContent += "parent " + targetCommitHash + "\n";
    commitContent += "time " +std::to_string(std::time(nullptr)) + "\n";
    commitContent += "message Merged " + targetBranchName + " into " + currentBranchName + ".\n";

    std::string newCommitHash = Utils::sha1(commitContent);
    Utils::writeContents(".gitlite/objects/" + newCommitHash, commitContent);
    //更新当前分支指针
    Utils::writeContents(".gitlite/refs/heads/" + currentBranchName, newCommitHash + "\n");

    SomeObj::clearIndex();
}

// ---------------- Bonus-1：add-remote / rm-remote / push / fetch / pull ----------------

void SomeObj::addRemote(const std::string&, const std::string&) {
    // TODO: 保存远程仓库地址
}

void SomeObj::rmRemote(const std::string&) {
    // TODO: 删除远程仓库信息
}

void SomeObj::push(const std::string&, const std::string&) {
    // TODO: 将当前分支的提交推送到远程分支
}

void SomeObj::fetch(const std::string&, const std::string&) {
    // TODO: 将远程分支的提交与 blob 复制到本地
}

void SomeObj::pull(const std::string&, const std::string&) {
    // TODO: fetch 后将取回的分支合并到当前分支
}

// ---------------- Bonus-2：diff / show ----------------

void SomeObj::diff() {
    // TODO: 比较当前提交与工作目录并输出差异，无差异时不输出
}

void SomeObj::diffWithCommit(const std::string&) {
    // TODO: 解析 revision，比较指定提交与工作目录，无差异时不输出
}

void SomeObj::diffBetween(const std::string&, const std::string&) {
    // TODO: 解析两个 revision 并比较对应提交，无差异时不输出
}

void SomeObj::show() {
    // TODO: 显示当前提交及其相对第一父提交的差异
}

void SomeObj::show(const std::string&) {
    // TODO: 显示指定提交或标签及其相对第一父提交的差异
}

std::string SomeObj::formatDate(long long timestamp) {
    std::time_t t = timestamp;
    std::tm* tm = std::localtime(&t);

    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y %z", tm);
    return std::string(buffer);
}

bool SomeObj::RestoreFileFromTree(const std::string& filename, const std::map<std::string, std::string>& tree) {
    auto it = tree.find(filename);
    if(it == tree.end()) return false;

    std::string blobHash = it->second;
    std::string content = Utils::readContentsAsString(".gitlite/objects/" + blobHash);
    Utils::writeContents(filename, content);
    return true;
}
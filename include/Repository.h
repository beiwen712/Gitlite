#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
#include <map>
#include <vector>
#include <regex>
#include <set>

// 建议类：抽象仓库的持久化状态，包括 HEAD、分支、标签、暂存区与对象库；
// 可自由扩充或重新设计。
// 注意：main.cpp 的 checkCWD 依赖 getGitliteDir()，请保留该方法。
class Repository {
public:
    // 元数据目录名，即 ".gitlite"
    static std::string getGitliteDir();

    static std::string getCurrentBranch();
    static std::map<std::string, std::string> getHeadTree();
    static std::map<std::string, std::pair<char, std::string>> readIndex();
    static void writeIndex(const std::map<std::string, std::pair<char, std::string>>& idx);
    static std::string getCurrentCommitHash();
    static std::map<std::string, std::string> getTreeFromCommit(const std::string& commitHash);

    static bool matchPattern(const std::string& pattern, const std::string& filename);
    static bool IsFileIgnored(const std::string& filename);

    static std::vector<std::string> getAllRefs();
    static bool isValidName(const std::string& name);
    static std::string resolveRevision(const std::string& revision);

    static std::set<std::string> findAncestors(const std::string& commitHash);
    static std::string getSplitPoint(const std::string& currentHash, const std::string& targetHash);
    static std::string getFileContent(const std::map<std::string, std::string>& tree, const std::string filename);
};

#endif // REPOSITORY_H

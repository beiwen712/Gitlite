#ifndef LCS_H
#define LCS_H

#include <string>
#include <vector>

class Lcs {
public:  
    enum EditType{EQUAL, DELETE, INSERT};
    struct Edit{
        EditType type;
        std::string line;
    };
    static std::vector<std::string> splitLines(const std::string& content);
    static std::vector<Edit> computeLCS(const std::string& oldContent, const std::string& newContent);
    static void printFileDiff(const std::string& filename, const std::vector<Edit> edits, const bool& isOldNull, const bool& isNewNull);
};


#endif // COMMIT_H

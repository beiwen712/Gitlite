#include "../include/Lcs.h"
#include "../include/Utils.h"
#include <vector>
#include <string>
#include <algorithm>

std::vector<std::string> Lcs::splitLines(const std::string& content) {
    std::vector<std::string> Lines;
    std::string currentLine = "";
    for(const char c : content) {
        currentLine += c;
        if(c == '\n') {
            Lines.push_back(currentLine);
            currentLine = "";
        }
    }
    if(!currentLine.empty()) {
        Lines.push_back(currentLine);
    }
    return Lines;
}

std::vector<Lcs::Edit> Lcs::computeLCS(const std::string& oldContent, const std::string& newContent) {
    auto oldLines = Lcs::splitLines(oldContent);
    auto newLines = Lcs::splitLines(newContent);
    int M = oldLines.size();
    int N = newLines.size();

    // dp[i][i] := old 前 i 行和 new 前 j 行的 lcs
    std::vector<std::vector<int>> dp(M + 5, std::vector<int>(N + 1, 0));
    for(int i = 1; i <= M; i++) {
        for(int j = 1; j <= N; j++) {
            if(oldLines[i - 1] == newLines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }
    //偷懒，只写 n 方算法哈哈哈哈
    std::vector<Lcs::Edit> edits;
    int i = M, j = N;
    while(i > 0 || j > 0) {
        if(i > 0 && j > 0 && oldLines[i - 1] == newLines[j - 1]) {
            edits.push_back({EQUAL, oldLines[i - 1]});
            i--; j--;
        } else if(j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            edits.push_back({INSERT, newLines[j - 1]});
            j--;
        } else if(i > 0 && (j == 0 || dp[i][j - 1] < dp[i - 1][j])) {
            edits.push_back({DELETE, oldLines[i - 1]});
            i--;
        } else {
            break;
        }
    }
    std::reverse(edits.begin(), edits.end());
    return edits;
}


void Lcs::printFileDiff(const std::string& filename, const std::vector<Lcs::Edit> edits, const bool& isOldNull, const bool& isNewNull) {
    Utils::message("diff --gitlite a/" + filename + " b/" + filename);
    Utils::message("--- " + (isOldNull ? "/dev/null" : "a/" + filename));
    Utils::message("+++ " + (isNewNull ? "/dev/null" : "b/" + filename));

    int n = edits.size(), i = 0;

    int oldLine = 1;
    int newLine = 1;
    // 找到第一个变更的位置
    while(i < n) {
        // 跳过相等的
        if(edits[i].type == EQUAL) {
            oldLine++;
            newLine++;
            i++;
            continue;
        }
        // 回溯 2 行 
        int back = 0;
        while(back < 3 && i - back - 1 >= 0 && edits[i - back - 1].type == EQUAL) {
            back++;
        }
        int hunkStartIdx = i - back;
        int hunkOldStartLine = oldLine - back;
        int hunkNewStartLine = newLine - back;

        int lastChangeIdx = i;
        int j = i;
        //向后扫描，判断是否超过 6 行
        while(j < n) {
            if(edits[j].type != EQUAL) {
                lastChangeIdx = j;
            } else if(j - lastChangeIdx > 6) {
                break;
            }
            j++;
        }
        // 向后 3 行
        int forward = 0;
        while(forward < 3 && lastChangeIdx + 1 + forward < n && edits[lastChangeIdx + 1 + forward].type == EQUAL) {
            forward++;
        } 
        int hunkEndIdx = lastChangeIdx + forward;
        
        int oldCount = 0, newCount = 0;
        for(int k = hunkStartIdx; k <= hunkEndIdx; k++) {
            if(edits[k].type == EQUAL) {oldCount++; newCount++;}
            else if(edits[k].type == DELETE) {oldCount++;}
            else if(edits[k].type == INSERT) {newCount++;} 
        }
        int outOldStart = oldCount == 0 ? 0 : hunkOldStartLine;
        int outNewStart = newCount == 0 ? 0 : hunkNewStartLine;
        Utils::message("@@ -" + std::to_string(outOldStart) + "," + std::to_string(oldCount) + 
                         " +" + std::to_string(outNewStart) + "," + std::to_string(newCount) + " @@");
        
        int curOld = hunkOldStartLine;
        int curNew = hunkNewStartLine;

        for(int k = hunkStartIdx; k <= hunkEndIdx; k++) {
            const auto& e = edits[k];
            if(e.type == EQUAL) {
                Utils::message(" " + Utils::trimTrailingNewlines(e.line));
                curOld++;
                curNew++;
            } else if(e.type == DELETE) {
                Utils::message("-" + Utils::trimTrailingNewlines(e.line));
                curOld++;
            } else if(e.type == INSERT) {
                Utils::message("+" + Utils::trimTrailingNewlines(e.line));
                curNew++;
            }

            if(!e.line.empty() && e.line.back() != '\n') {
                Utils::message("\\ No newline at end of file");
            }
        }
        oldLine = curOld;
        newLine = curNew;
        i = hunkEndIdx + 1;
    }
}


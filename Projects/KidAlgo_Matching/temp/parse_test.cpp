#include <string>
#include <iostream>
#include <vector>
#include <unordered_set>

using namespace std;


static void doSplit(const string& input, const string& delimiter, vector<string>& res) {
        if (input.size() == 0) {
                return;
        }
        const auto pos = input.find(delimiter);
        if (pos == string::npos) {
                res.push_back(input);
                return;
        }
        const int len = delimiter.size();
        //if (pos == 0) {
        //      doSplit(input.substr(len), delimiter, res);
        //}
        res.push_back(input.substr(0, pos));
        doSplit(input.substr(pos+len), delimiter, res);
}

static vector<string> split(const string& input, const string& delimiter) {
        vector<string> result;
        doSplit(input, delimiter, result);
        return result;
}

static string parse(const string& input) {
        const auto splitted = split(input, "\;");
        cout << "Input: " << input << "\n";
        unordered_set<char> leftSet;
        unordered_set<char> rightSet;
        for(const auto& colonSplit : splitted) {
                // std::cout << "~~" << colonSplit << "~~\n";
                const auto commaSplit = split(colonSplit, ",");
                for(const auto& part : commaSplit) {
                        // std::cout << "--" << part << "--\n";
                        const auto pos = part.find("/");
                        const auto left = part.substr(0, pos);
                        const auto right = part.substr(pos+1);
                        for(const auto& leftChar : left) {
                                leftSet.insert(leftChar);
                        }
                        for(const auto& rightChar : right) {
                                rightSet.insert(rightChar);
                        }
                }
        }
        string result = "";
        for(const auto& left : leftSet) {
                result += left;
        }
        result += "/";
        for(const auto& right : rightSet) {
                result += right;
        }
        return result;
}

static string orderedDistinctLetter(const string& inputStr, const unsigned int index) {
        string result = "";
        unordered_set<char> visited;
        for(const auto& c : inputStr) {
                if (visited.count(c) == 0 && isalpha(c)) {
                        result += c;
                        visited.insert(c);
                }
        }
        
        if (index == -1) {
                return result;
        } else {
                return string(1, result[min(index, (unsigned int)(result.size()))]);
        }
}

static void test1() {
        const string input = "GWB/R";
        const string result = parse(input);

        std::cout << "Parsed: " << result << "\n";
}

static void test2() {
        const string input = "G/,/RR\;G/,/W,G/,/RR\;G/,/RR,G/,/RR,G/,/RR";
        // const unsigned int index = 2;
        // const string result = orderedDistinctLetter(input, index);
        // const string input = "GRW";
        const string result = orderedDistinctLetter(input, -1);
        cout << "Distinct: " << orderedDistinctLetter(result, 1) << "\n";
}

int main() {
        // G/\;/RR\;G/,/RR\;G/,/RRR,GGG/,G/RR
        // string input = "G/,/RR\;G/,/W,G/,/RR\;G/,/RR,G/,/RR,G/,/RR";
        // test1(); // test parse() method
        test2(); // test orderedDistinctLetter() method


}
#pragma once


#include <string.h>

template<typename T>
std::string str(T x){
	// Just a handy, pythonesque string function
	return std::to_string(x);
}


std::deque<std::string> split(const std::string& s, const char delimiter){
	// split is a little bit of a pair -- we want to split the string at delimiter
	// but handle nulls correctly too. 
	// special cases:
	// split("a:", ':') -> ["a", ""]
	// split(":", ':')  -> [""]
	// split(":a", ':') -> ["", "a"]
	
	std::deque<std::string> tokens;
	
	if(s.length() == 0) {
		return tokens;
	}
	
	size_t i = 0;
	while(i < s.size()) {
		size_t k = s.find(delimiter, i);
		
		if(k == std::string::npos) {
			tokens.push_back(s.substr(i,std::string::npos));
			return tokens;
		}
		else {		
			tokens.push_back(s.substr(i,k-i));
			i=k+1;
		}
	}	

	// if we get here, that means that the last foudn k was the last
	// character, which means we need to append ""
	tokens.push_back("");
	return tokens;

//	std::deque<std::string> tokens;
//	std::string token;
//	std::istringstream ts(s);
//	while (std::getline(ts, token, delimiter)) {
//		std::cerr << "TOKEN =" << token << std::endl;
//		tokens.push_back(token);
//	}
//	
//	if(s.at(s.length()-1) == delimiter) {
//		tokens.push_back("");
//	}
}



unsigned int levenshtein_distance(const std::string& s1, const std::string& s2) {
	// From https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C++

	const std::size_t len1 = s1.size(), len2 = s2.size();
	std::vector<std::vector<unsigned int>> d(len1 + 1, std::vector<unsigned int>(len2 + 1));

	d[0][0] = 0;
	for(unsigned int i = 1; i <= len1; ++i) d[i][0] = i;
	for(unsigned int i = 1; i <= len2; ++i) d[0][i] = i;

	for(unsigned int i = 1; i <= len1; ++i)
		for(unsigned int j = 1; j <= len2; ++j)
			  d[i][j] = std::min(d[i - 1][j] + 1, std::min(d[i][j - 1] + 1, d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) ));
			  
	return d[len1][len2];
}


size_t count(const std::string& str, const std::string& sub) {
	// how many times does sub occur in str?
	// https://www.rosettacode.org/wiki/Count_occurrences_of_a_substring#C.2B.2B
	
    if (sub.length() == 0) return 0;
    
	size_t count = 0;
    for (size_t offset = str.find(sub); 
		 offset != std::string::npos;
		 offset = str.find(sub, offset + sub.length())) {
        ++count;
    }
    return count;
}

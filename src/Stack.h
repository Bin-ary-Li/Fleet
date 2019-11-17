#pragma once

#include <vector> 

/* Many things in Fleet are stacks and this is designed to allow for
 * rapid changse to the stack type in order to improve speed. std::stack appears to be
 * slow. Using vector with some wrappers is faster
 * 
 * NOTE: We have tried reserving the intiial vector size and it doesn't seem to speed things up
 * 
 * 
 * */

template<typename T>
class Stack : public std::vector<T> {
		
public:

	Stack() {
//		this->reserve(32); /// chosen with a little experimetnation for FormalLanguageTheory-Complex
	}

	void push(const T& val) {
		this->push_back(val);
	}
	
	void pop() {
		this->pop_back();
	}
	
	/* There is a little sublety here -- for integral types, it's a pain to return a reference
	 * so we want to just return T. But some optimizations of the virtual machine will do better
	 * with references, leaving the top value on the stack. So by default we don't return a reference
	 * with top() but we do with topref
	 */
	
	T top() {
		return this->back();
	}

	T& topref() {
		return std::forward<T&>(this->back());
	}
};

#ifndef TOP_H
#define TOP_H

#include "stdlib.h"
#include "stdio.h"

#include <pthread.h>
#include <cmath>
#include <set>
#include <queue>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <cassert>

template<class T>
class TopN {
	
protected:
	std::map<T,unsigned long> cnt; // also let's count how many times we've seen each for easy debugging
	pthread_mutex_t lock; 
	size_t N;
	std::multiset<T> s; // important that it stores in sorted order by posterior!
	
public:

	TopN(size_t n=1000) : N(n) {
		pthread_mutex_init(&lock, nullptr); 
	}

	void set_size(size_t n) {
		N = n;
	}

	size_t size() const {
		return s.size();
	}
	
	std::multiset<T>& values(){
		return s;
	}

	void add(const T x) { // NOTE: Do not use reference here, since we often pass in derefernce of pointer! And insert takes a reference
		//std::cerr << x.posterior TAB x.string() TAB (s.empty()?-infinity:s.begin()->posterior) ENDL;
	
		// toss out infs
		if(std::isnan(x.posterior) || x.posterior == -infinity || CTRL_C) return;
		
		pthread_mutex_lock(&lock);
		
		// if we aren't in there and our posterior is better than the worst
		if(s.find(x) == s.end()) { 
			if(s.size() < N || s.empty() || x.posterior > s.begin()->posterior) { // skip adding if its the worst
				s.insert(x); // add this one
				assert(cnt.find(x) == cnt.end());
				cnt[x] = 1;
				
				// and remove until we are the right size
				while(s.size() > N) {
					size_t n = cnt.erase(*s.begin());
					assert(n==1);
					s.erase(s.begin()); 
				}
			}
		}
		else { // if its stored somewhere already
			cnt[x]++;
		}
		pthread_mutex_unlock(&lock);
	}
	void operator<<(const T x) {
		add(x);
	}
	
	void add(TopN<T> x) { // add from a whole other topN
		for(auto h: x.s){
			add(h);
		}
	}
	void operator<<(const TopN<T> x) {
		add(x);
	}
	
    T max() { 
		assert(!s.empty());
		return *s.rbegin();  
	}
    T min() { 
		assert(!s.empty());
		return *s.begin(); 
	}
	
    bool empty() { return s.empty(); }
    
	double Z() { // compute the normalizer
		double z = -infinity;
		pthread_mutex_lock(&lock);
		for(auto x : s) z = logplusexp(z, x.posterior);
		pthread_mutex_unlock(&lock);
		return z;       
	}
	
    void print(void printer(T&)) {
	   pthread_mutex_lock(&lock);
       for(auto h : s) {
          printer(h);
       }
	   pthread_mutex_unlock(&lock);
    }
	
	void clear() {
		pthread_mutex_lock(&lock);
		s.erase(s.begin(), s.end());
		cnt.clear();
		pthread_mutex_unlock(&lock);
	}
	
	unsigned long count(const T x) const {
		// This migth get called by something not in here, so we can't assume x is in 
		if(cnt.count(x)) return cnt.at(x);
		else             return 0;
	}
	
};



#endif
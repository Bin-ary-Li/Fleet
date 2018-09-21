#ifndef MCTS_H
#define MCTS_H

//#define DEBUG_MCTS 1

#include <atomic>
#include <mutex>
#include <set>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <pthread.h>

#include "StreamingStatistics.h"

/// Wrapper for parlallel

template<typename t_value> class MCTSNode;
template<typename t_value> void parallel_MCTS(MCTSNode<t_value>* root, unsigned long steps, unsigned long cores);
template<typename t_value> void parallel_MCTS_helper( void* _args);

template<typename t_value>
struct parallel_MCTSargs { 
	MCTSNode<t_value>* root;
	unsigned long steps; 
};

template<typename t_value>
void* parallel_MCTS_helper( void* args) {
	auto a = (parallel_MCTSargs<t_value>*)args;
	a->root->search(a->steps);
	pthread_exit(nullptr);
}

template<typename t_value>
void parallel_MCTS(MCTSNode<t_value>* root, unsigned long steps, unsigned long cores) {
	
	
	if(cores == 1)  { // don't use parallel, for easier debugging
		root->search(steps);
	}
	else {
	
		pthread_t threads[cores]; 
		parallel_MCTSargs<t_value> args[cores];
		
		for(unsigned long t=0;t<cores;t++) {
			
			args[t].steps=steps;
			args[t].root=root;
			
			int rc = pthread_create(&threads[t], nullptr, parallel_MCTS_helper<t_value>, &args[t]);
			if(rc) assert(0);
		}
		
		for(unsigned long t=0;t<cores;t++)  
			pthread_join(threads[t], nullptr);     // wait for all to complete
	}
}





/// MCTS Implementation


template<typename t_value>
class MCTSNode {
public:

    std::vector<MCTSNode*> children;

    std::atomic<unsigned long> nvisits;  // how many times have I visited each node?
    std::atomic<bool> open; // am I still an available node?
	 
	const bool expand_all_children = false; // when we expand a new leaf, do we expand all children or just sample one (from their priors?) 
	
	double (*compute_playouts)(const t_value*); // a function point to how we compute playouts
	double explore; 
	
    pthread_mutex_t child_modification_lock; // for exploring below
    
	StreamingStatistics statistics;
	
    MCTSNode* parent; // who is my parent?
    t_value* value;
      
    MCTSNode(MCTSNode* par, t_value* v) : parent(par), value(v) {
		// here we don't expand the children because this is the constructor called when enlarging the tree
		explore=par->explore;
		compute_playouts=par->compute_playouts;
		
        initialize();
    }
    
    MCTSNode(double ex, t_value* h0, double cp(const t_value*) ) : compute_playouts(cp), explore(ex), parent(nullptr), value(h0) {
        initialize();        
    }
    
    ~MCTSNode() {
        for(auto c: children)
            delete c;
        delete value;
    }
    
    size_t size() const {
        int n = 1;
        for(auto c : children)
            n += c->size();
        return n;
    }
    
    void initialize() {
		pthread_mutex_init(&child_modification_lock, nullptr); 
        
		nvisits=0;
        open=true;
    }
    
	
    void print(std::ostream& o, const int depth, const bool sort) const {
        std::string idnt = std::string(depth, '\t');
        
		std::string opn = (open?" ":"*");
		o << idnt TAB opn TAB this->sample_score() TAB statistics.median() TAB statistics.max TAB "S=" << nvisits TAB value->string() ENDL;
		
		// optional sort
		if(sort) {
			// we have to make a copy of our pointer array because otherwise its not const			
			std::vector<MCTSNode*> c2 = children;			
			std::sort(c2.begin(), c2.end(), [](const auto a, const auto b) {return a->statistics.N > b->statistics.N;}); // sort by how many samples

			for(auto c : c2) c->print(o, depth+1, sort);
		}
		else {		
			for(auto c : children) c->print(o, depth+1, sort);
		}
    }
 
	// wrappers for file io
    void print(const bool sort=true) const { 
		print(std::cout, 0, sort);		
	}
	void printerr(const bool sort=true) const {
		print(std::cerr, 0);
	}    
	void print(const char* filename, const bool sort=true) const {
		std::ofstream out(filename);
		print(out, 0, sort);
		out.close();		
	}

	double sample_score() const {
		
		// I will add probability "explore" pseudocounts to my parent's distribution in order to form a prior 
		// please change this parameter name later. 
		if(uniform(rng) <= explore / (explore + statistics.N)) {
			// here, if we are the base root node, we treat all "parent" samples as 0.0
			return (this->parent == nullptr ? 0.0 : parent->sample_score()); // handle the base case			
		} 
		else {
			return statistics.sample();			
		}		
	}

	int sample_child() const {
		
		double best_score = -infinity;
		int best_i = -1;
		int i=0;
		for(auto const c : children) {
			if(c->open) {
				double s = c->sample_score();
				//std::cerr << "Sampling " TAB s TAB c->value->string() ENDL;
				if(s >= best_score) {
					best_score = s;
					best_i = i;
				}
			}
			++i;
		}
		
		return best_i;
		
	}

    void add_sample(const double v, const size_t num){ // found something with v
        
		nvisits += num; // we want to update this even if inf/nan since we did try searching here
		
		statistics << v;
		
		// and add this sampel going up too
		if(parent != nullptr) {
			parent->add_sample(v, 0); // we don't add num going up because we incremented it on the way down (while sampling)
		}
    }
	
	
	void add_child_nodes() {

		pthread_mutex_lock(&child_modification_lock); 
		if(children.size() == 0) { // check again in case someone else has edited in the meantime
			
			size_t N = value->neighbors();
			//std::cerr << N TAB value->string() ENDL;
						
			if(N==0) { // remove myself from the tree since these routes have been explored
				open = false;
			} 
			else {
				open = true;
				for(size_t ei = 0; ei<N; ei++) {
					size_t eitmp = ei; // since this is passed by refernece
					
					auto v = value->make_neighbor(eitmp);
					
#ifdef DEBUG_MCTS
        if(value != nullptr) { // the root
            std::cout TAB "Adding child " <<  pthread_self() << "\t" <<  this << "\t[" << v->string() << "] " ENDL;
        }
#endif
					MCTSNode<t_value>* c = new MCTSNode<t_value>(this, v);
					
					children.push_back(c);
				}
			}
		}
		pthread_mutex_unlock(&child_modification_lock); 
				
	}
   
	// search for some number of steps
	void search(unsigned long steps) {
		for(unsigned long i=0;i<steps && !CTRL_C;i++){
		   this->search_one();
		}
	}
	
   
    void search_one() {
        // sample a path down the tree and when we get to the end
        // use random playouts to see how well we do
        assert(value != nullptr || parent==nullptr); // only the root gets a null value

#ifdef DEBUG_MCTS
        if(value != nullptr) { // the root
            std::cout << "MCTS SEARCH " <<  pthread_self() << "\t" <<  this << "\t[" << value->string() << "] " << nvisits << std::endl;
        }
#endif
		
		if(nvisits == 0) { // I am a leaf of the search who has not been expanded yet
			add_sample(compute_playouts(value), 1); // update my internal counts
			open = value->neighbors() > 0; // only open if the value is partial
			return;
		}
		else if(children.size() == 0) { // if I have no children
			this->add_child_nodes(); // add child nodes if they don't exist
				
			if(expand_all_children){
				// Include this if when we add child nodes, we need to go through and
				// compute playouts on all of them. If we don't do this, we'd only choose one
				// and then the one we choose the first time determines the probability of coming
				// back to all the others, which may mislead us (e.g. we may never return to a good playout)
				for(auto c: children) {
					nvisits++; // I am going to get a visit for each of these
					
					c->add_sample(c->compute_playouts(value), 1); // update my internal counts
					c->open = c->value->neighbors() > 0; // only open if the value is partial
					
					open = open || c->open; // I'm open if any child is
				}
				return; // this counts as a search move
			}
		}
		
		// choose an existing child and expand (even if we just added them above!)
			
		// Now choose a child from their weights, but note that our weights will
		// preferentially choose nodes with nvisits=0 since these are the unexpanded leaves
		int which_child = this->sample_child(); 
		
		nvisits++; // increment our visit count on our way down so other threads don't follow
		
		if(which_child==-1) { // this is a sign this node is done
			open = false;
		}
		else {
			children[which_child]->search_one(); // and recurse         
		}
		
    } // end search


};


#endif
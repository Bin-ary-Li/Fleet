
#pragma once 

//#define DEBUG_MCTS 1

#include <atomic>
#include <mutex>
#include <set>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <functional>

#include "StreamingStatistics.h"

template<typename HYP, typename callback_t>
class MCTSNode {

	/// MCTS Implementation
	
public:

	// use a strict enum of how children are scored in sampling them
	// UCBMAX -- a method inspired by UCB sampling, but using the max of all hypotheses found by that route
	// SAMPLE -- sample from (a subsample) of the posteriors we find below a node
	// MEDIAN -- choose if my kid tends to beat the median of the parent
	enum class ScoringType { SAMPLE, UCBMAX, MEDIAN};	

    std::vector<MCTSNode> children;

	// NOTE: These used to be atomics?
    unsigned long nvisits;  // how many times have I visited each node?
    bool open; // am I still an available node?

	callback_t* callback; // a function point to how we compute playouts
	double explore; 
	
	mutable std::mutex child_mutex; // for access in parallelTempering
    mutable std::mutex stats_mutex;
	
	Fleet::Statistics::StreamingStatistics statistics;
	
	typename HYP::t_data* data;
    MCTSNode* parent; // who is my parent?
    HYP value;
    ScoringType scoring_type;// how do I score playouts?
  
    MCTSNode(MCTSNode* par, HYP& v) 
		: callback(par->callback), 
		  explore(par->explore), data(par->data), parent(par), value(v), scoring_type(par->scoring_type) {
		// here we don't expand the children because this is the constructor called when enlarging the tree
		
        initialize();	
    }
    
    MCTSNode(double ex, HYP& h0, callback_t* cb, typename HYP::t_data* d, ScoringType st=ScoringType::SAMPLE ) : 
		callback(cb), explore(ex), data(d), parent(nullptr), value(h0), scoring_type(st) {
        
		initialize();        
		
		if(not h0.value.is_null()) { CERR "# Warning, initializing MCTS root with a non-null node." ENDL; }
    }
	
	// should not copy or move because then the parent pointers get all messed up 
	MCTSNode(const MCTSNode& m) = delete;
	MCTSNode(MCTSNode&& m) {
		std::lock_guard guard1(m.child_mutex); // get m's lock before moving
		std::lock_guard guard2(m.stats_mutex);
		std::lock_guard guard3(  child_mutex); // and mine to be sure
		std::lock_guard guard4(  stats_mutex);
		
		assert(m.children.size() ==0); // else parent pointers get messed up
		nvisits = m.nvisits;
		open = m.open;
		callback = m.callback;
		explore = m.explore;
		statistics = std::move(statistics);
		parent = m.parent;
		value = std::move(m.value);
		scoring_type = m.scoring_type;
		children = std::move(children);
		data = m.data;
		
	}
	
    size_t size() const {
        int n = 1;
        for(const auto& c : children)
            n += c.size();
        return n;
    }
    
    void initialize() {
		nvisits=0;
        open=true;
    }
    
	void print(std::ostream& o, const int depth, const bool sort) const { 
        std::string idnt = std::string(depth, '\t'); // how far should we indent?
        
		std::string opn = (open?" ":"*");
		
		double s = score(); // must call before getting stats mutex, since this requests it too 
		
		{
			std::lock_guard guard(stats_mutex);
			o << idnt TAB opn TAB s TAB statistics.median() TAB statistics.max TAB "S=" << nvisits TAB value.string() ENDL;
		}
		// optional sort
		if(sort) {
			// we have to make a copy of our pointer array because otherwise its not const			
			std::vector<const MCTSNode*> c2;
			for(const auto& c : children) c2.push_back(&c);
			std::sort(c2.begin(), c2.end(), [](const auto a, const auto b) {return a->statistics.N > b->statistics.N;}); // sort by how many samples

			for(const auto& c : c2) 
				c->print(o, depth+1, sort);
		}
		else {		
			for(auto& c : children) 
				c.print(o, depth+1, sort);
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

	double score() const {
		// Compute the score of this node, which is used in sampling down the tree
		// NOTE: this can't be const because the StreamingStatistics need a mutex
		
		if(scoring_type == ScoringType::SAMPLE) {
			// I will add probability "explore" pseudocounts to my parent's distribution in order to form a prior 
			// please change this parameter name later. 
			if(this->parent != nullptr and uniform() <= explore / (explore + statistics.N)) {
				return parent->score(); 
			} 
			else {
				std::lock_guard guard(stats_mutex); // lock our stats here
				return statistics.sample();			
			}		
		}
		else if(scoring_type == ScoringType::UCBMAX) {
			// score based on a UCB-inspired score, using the max found so far below this
			if(statistics.N == 0 || parent == nullptr) return infinity; // preference for unexplored nodes at this level, thus otherwise my parent stats must be >0
			return statistics.max + explore * sqrt(log(parent->statistics.N+1) / log(1+statistics.N));
			
		}
		else if(scoring_type == ScoringType::MEDIAN) {
			if(statistics.N == 0 || parent == nullptr) return infinity; // preference for unexplored nodes at this level, thus otherwise my parent stats must be >0
			return statistics.p_exceeds_median(parent->statistics) + explore * sqrt(log(parent->statistics.N+1) / log(1+statistics.N));
		}
		else {
			assert(false && "Invalide ScoringType specified in MCTS::score");
		}
	}

	size_t open_children() const {
		size_t n = 0;
		for(auto const& c : children) {
			n += c.open;
		}
		return n;
	}


	size_t best_child_index() const {
		// returns a pointer to the best child according to the scoring function
		// NOTE: This might return a nullptr in highly parallel situations
		std::lock_guard guard(child_mutex);
		
		size_t best_index = 0;
		double best_score = -infinity;
		size_t idx = 0;
		for(auto& c : children) {
			if(c.open) {
				//if(c->statistics.N==0) return c; // just take any unopened
				double s = c.score();
				if(s >= best_score) {
					best_score = s;
					best_index = idx;
				}
			}
			idx++;
		}
		return best_index;
	}


    void add_sample(const double v, const size_t num=1){ // found something with v
        
		nvisits += num; // we want to update this even if inf/nan since we did try searching here
		
		std::lock_guard guard(stats_mutex);
		
		// If we do the first of these, we sampling probabilities proportional to the probabilities
		// otherwise we sample them uniformly
		statistics.add(v,v); // it has itself as a log probability!
		//statistics << v;
		
		// and add this sampel going up too
		if(parent != nullptr) {
			parent->add_sample(v, num);
		}
    }
	void operator<<(double v) { add_sample(v,1); }
	
	
	
	virtual void playout(Control inner_ctl) {
		// this is how we compute playouts here -- defaultly mcmc 

#ifdef DEBUG_MCTS
	COUT "\tPLAYOUT " <<  value.string() TAB std::this_thread::get_id() ENDL;
#endif

		HYP h0 = value; // need a copy to change resampling on 
		
		for(auto& n : h0.value ){
			n.can_resample = false;
		}
		
		// make a wrapper to add samples
		std::function<void(HYP& h)> wrapped_callback = [this](HYP& h) { 
			(*callback)(h);
			this->add_sample(h.posterior);
		};
		
		auto h = h0.copy_and_complete(); // fill in any structural gaps
		
		MCMCChain chain(h, data, wrapped_callback);
		chain.run(inner_ctl); // run mcmc with restarts; we sure shouldn't run more than runtime
	}
	
	
	void add_child_nodes() {

		std::lock_guard guard(child_mutex);
		
		if(children.size() == 0) { // check again in case someone else has edited in the meantime
			
			size_t N = value.neighbors();
			//std::cerr << N TAB value->string() ENDL;
						
			if(N==0) { // remove myself from the tree since these routes have been explored
				open = false;
			} 
			else {
				open = true;
				for(size_t ei = 0; ei<N; ei++) {
					size_t eitmp = ei; // since this is passed by refernece
					
					auto v = value.make_neighbor(eitmp);
					
#ifdef DEBUG_MCTS
        COUT "\tAdding child " <<  this << "\t[" << v.string() << "] " ENDL;
#endif
					//children.push_back(MCTSNode<HYP>(this,v));
					children.emplace_back(this,v);
				}
			}
		}
	}
   
	// search for some number of steps
	void search(Control ctl, Control inner_ctl) {
		// ctl controls the mcts path through nodes, inner_ctl controls each playout expansion
		assert(ctl.threads == 1);
		ctl.start();
		
		while(ctl.running()) {
			#ifdef DEBUG_MCTS
			COUT "\tMCTS SEARCH LOOP" TAB std::this_thread::get_id() ENDL;
			#endif
			
			this->search_one(inner_ctl);
		}
	}

	
	void parallel_search(Control ctl, Control inner_ctl) { 
		
		auto __helper = [](MCTSNode<HYP,callback_t>* h, Control ctl, Control inner_ctl) {
			h->search(ctl, inner_ctl);
		};
		
		std::thread threads[ctl.threads]; 
		
		// start everyone running 
		for(unsigned long i=0;i<ctl.threads;i++) {
			Control ctl2 = ctl; ctl2.threads=1;
			threads[i] = std::thread(__helper, this, ctl2, inner_ctl);
		}
		
		for(unsigned long i=0;i<ctl.threads;i++) {
			threads[i].join();
		}	
	}

	
	
   
    void search_one(Control inner_ctl) {
        // sample a path down the tree and when we get to the end
        // use random playouts to see how well we do
       
#ifdef DEBUG_MCTS
		COUT "MCTS SEARCH ONE " <<  this << "\t[" << value.string() << "] " << nvisits TAB std::this_thread::get_id() ENDL;
#endif
		
		if(nvisits == 0) { // I am a leaf of the search who has not been expanded yet
			this->playout(inner_ctl); // update my internal counts
			open = value.neighbors() > 0; // only open if the value is partial
			return;
		}
		
		
		if(children.size() == 0) { // if I have no children
			this->add_child_nodes(); // add child nodes if they don't exist
		}
		
		// even after we try to add, we might not have any. If so, return
		if(open_children() == 0) {
			open = false;
			return;
		}
		else {
			// now if we get here, we must have at least one open child
			// so we can pick the best and recurse 
			
			nvisits++; // increment our visit count on our way down so other threads don't follow
			
			// choose the best and recurse
			size_t bi = best_child_index();
			children[bi].search_one(inner_ctl);
		}
    } // end search


};
#pragma once 

#include "MCMCChain.h"
#include "FiniteHistory.h"

#include <mutex>

template<typename HYP>
class MCMCChain {
	// An MCMC chain object. 
	// NOTE: This defaultly loads its steps etc from the Fleet command line args
	// so we don't have to pass those in 
	// should be thread safe for modifications to temperature and current 
	
public:
	
	HYP* current;
	std::mutex current_mutex; // for access in parallelTempering
	typename HYP::t_data* data;
	
	HYP* themax;
	void (*callback)(HYP*);
	unsigned long restart;
	bool          returnmax; 
	
	unsigned long samples; 
	unsigned long proposals;
	unsigned long acceptances;
	unsigned long steps_since_improvement; 
	
	std::atomic<double> temperature; // make atomic b/c ParallelTemperingn may try to change 
	
	FiniteHistory<bool> history;
	
	MCMCChain(HYP* h0, typename HYP::t_data* d, void(*cb)(HYP*) ) : 
			current(h0), data(d), themax(nullptr), callback(cb), restart(mcmc_restart), 
			returnmax(true), samples(0), proposals(0), acceptances(0), steps_since_improvement(0),
			temperature(1.0), history(100) {
	}
	
	~MCMCChain() {
		delete current;
		delete themax;
	}
	
	HYP* getCurrent() {	return current; }
	
	void run(unsigned long steps, unsigned long time) {
		
		using clock = std::chrono::steady_clock;
		
		// compute the info for the curent
		current->compute_posterior(*data);
		callback(current);
		themax = current->copy();
		
		// we'll start at 1 since we did 1 callback on current to begin
		auto start_time = clock::now();
		for(unsigned long i=1; (i<steps || steps==0) && !CTRL_C; i++){
			
			// check the elapsed time 
			if(time > 0) {
				double elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start_time).count();
				if(elapsed_time > time) break;
			}
			
			#ifdef DEBUG_MCMC
				std::cerr << "\n# Current\t" << current->posterior TAB current->prior TAB current->likelihood TAB "\t" TAB current->string() ENDL;
			#endif
			
			// generate the proposal -- defaulty "restarting" if we're currently at -inf
			HYP* proposal;
			double fb = 0.0;
			
			current_mutex.lock(); // lock here when we are updating current -- otherwise we can modify it
			if(current->posterior > -infinity) {
				std::tie(proposal,fb) = current->propose();
			}
			else {
				proposal = current->restart();
			}
			current_mutex.unlock();
				
			++proposals;
			
			// actually compute the posterior on the dat 
			proposal->compute_posterior(*data);

			#ifdef DEBUG_MCMC
				std::cerr << "# Proposed \t" << proposal->posterior TAB proposal->prior TAB proposal->likelihood TAB fb TAB proposal->string() ENDL;
			#endif
			
			// keep track of the max if we are supposed to
			
			if(proposal->posterior > themax->posterior) {
				delete themax; 			
				themax = proposal->copy(); 
				steps_since_improvement = 0;
			}
						
			// use MH acceptance rule, with some fanciness for NaNs
			current_mutex.lock(); 
			double ratio = proposal->at_temperature(temperature) - current->at_temperature(temperature) - fb; // Remember: don't just check if proposal->posterior>current->posterior or all hell breaks loose		
			if(   (std::isnan(current->posterior))  ||
				  (current->posterior == -infinity) ||
					((!std::isnan(proposal->posterior)) &&
					 (ratio > 0 || uniform(rng) < exp(ratio)))) {
				current_mutex.unlock();
 
				#ifdef DEBUG_MCMC
					  std::cerr << "# Accept" << std::endl;
				#endif
				
				
				current_mutex.lock(); // lock here when we are updating
				delete current; 
				current = proposal;
				current_mutex.unlock(); // lock here when we are updating
  
				history << true;
				++acceptances;
			}
			else {
				current_mutex.unlock();
				history << false;
				delete proposal; 
			}
				
			// and call on the sample if we meet all our criteria
			current_mutex.lock(); 
			callback(current);
			current_mutex.unlock();
				
			++samples;
			++FleetStatistics::global_sample_count;

			// and finally if we haven't improved then restart
			if(restart>0 && steps_since_improvement > restart){
				current_mutex.lock(); 
				steps_since_improvement = 0; // reset the couter
				auto tmp = current->restart();
				delete current;
				current = tmp;
				current->compute_posterior(*data);
				callback(current);
				if(current->posterior > themax->posterior) {
					delete themax; 			
					themax = current->copy(); 	
				}
				current_mutex.unlock(); 
			}
			
		} // end the main loop	
		
		
		
	}
	void run() {
		run(0,0); // run forever
	}
	
	double acceptance_ratio() {
		return history.mean();
	}
	
	double at_temperature(double t){
		return current->at_temperature(t);
	}
	
};

# include <algorithm>

using S = std::string; // just for convenience

S alphabet = "01"; // the alphabet we use (possibly specified on command line)
//S datastr  = "01,01011,010110111"; // the data, comma separated
S datastr  = "011;011011;011011011"; // the data, escape-semicolon separated



const size_t MAX_LENGTH = 64; // longest strings cons will handle
const double strgamma = 0.99; // penalty on string length
const size_t NTEMPS = 12;
double editDisParam = 1.0;





///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define FLEET_GRAMMAR_TYPES bool,S

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "Primitives.h"
#include "Builtins.h"

std::tuple PRIMITIVES = {

	Primitive("\u2205",        +[]()         -> S          { return S(""); }),

	Primitive("(%s/%s)", +[](S x, S y) -> S { 
		if(x.length() + y.length() > MAX_LENGTH) 
			throw VMSRuntimeError;
		else {
			return x.append("/").append(y);
		}				
	}),

	Primitive("(%s,%s)", +[](S x, S y) -> S { 
		if(x.length() + y.length() > MAX_LENGTH) 
			throw VMSRuntimeError;
		else {
			return x.append(",").append(y);
		}				
	}),

	Primitive("cons(%s,%s)", +[](S x, S y) -> S { 
		if(x.length() + y.length() > MAX_LENGTH) 
			throw VMSRuntimeError;
		else {
			return x.append(y);
		}				
	}),

	Primitive("car(%s)", +[](S x) -> S {return (x.empty() ? S("") : S(1,x.at(0)));}),
	Primitive("cdr(%s)", +[](S x) -> S {return (x.empty() ? S("") : x.substr(1,S::npos));}),
	Primitive("(%s==%s)",      +[](S x, S y) -> bool       { return x==y; }),
	Primitive("empty(%s)",     +[](S x) -> bool            { return x.length()==0; }),
		
	// but we also have to add a rule for the BuiltinOp that access x, our argument
	Builtin::X<S>("x", 10.0),
	Builtin::Flip("flip()", 10.0),
	Builtin::If<S>("if(%s,%s,%s)", 1.0),
	Builtin::SafeRecurse<S,S>("F(%s)")
};


// Includes critical files. Also defines some variables (mcts_steps, explore, etc.) that get processed from argv 
#include "Fleet.h" 



/* Define a class for handling my specific hypotheses and data. Everything is defaulty a PCFG prior and 
// * regeneration proposals, but I have to define a likelihood */
class MyHypothesis : public LOTHypothesis<MyHypothesis,Node, S, S> {
public:
	using Super =  LOTHypothesis<MyHypothesis,Node,S,S>;
	using Super::Super; // inherit the constructors
	
	// Very simple likelihood that just counts up the probability assigned to the output strings
//	double compute_single_likelihood(const t_datum& x) {
//		auto out = call(x.input, "<err>");
//		return logplusexp( log(x.reliability)+(out.count(x.output)?out[x.output]:-infinity),  // probability of generating the output
//					       log(1.0-x.reliability) + (x.output.length())*(log(1.0-gamma) + log(0.5)) + log(gamma) // probability under noise; 0.5 comes from alphabet size
//						   );
//	}

	// // original likelihood function
	// double compute_single_likelihood(const t_datum& x) {
	// 	auto out = call(x.input, "<err>", this, 256, 256); //256, 256);
		
	// 	// a likelihood based on the prefix probability -- we assume that we generate from the hypothesis
	// 	// and then might glue on some number of additional strings, flipping a gamma-weighted coin to determine
	// 	// how many to add
	// 	double lp = -infinity;
	// 	for(auto o : out.values()) { // add up the probability from all of the strings
	// 		if(is_prefix(o.first, x.output)) {
	// 			lp = logplusexp(lp, o.second + log(strgamma) + log((1.0-strgamma)/alphabet.length()) * (x.output.length() - o.first.length()));
	// 		}
	// 	}
	// 	return lp;
	// }

	// compute likelihood in term of the levenshtein distance
	double compute_single_likelihood(const t_datum& x) {
		auto out = call(x.input, "<err>", this, 256, 256); //256, 256);
		
		// a likelihood based on the levenshtein distance between hypothesis string and result string
		double lp = -infinity;
		for(auto o : out.values()) { // add up the probability from all of the strings
			lp = logplusexp(lp, - editDisParam*levenshtein_distance(o.first, x.output) + o.second);
		}
		return lp;
	}
};


// mydata stores the data for the inference model
MyHypothesis::t_data mydata;
// top stores the top hypotheses we have found
Fleet::Statistics::TopN<MyHypothesis> top;


// // define some functions to print out a hypothesis
// void print(MyHypothesis& h, S prefix, TopN& top) {
// 	COUT "# ";
// 	h.call("", "<err>").print();
// 	COUT "\n" << prefix << top.count(h) TAB  h.posterior TAB h.prior TAB h.likelihood TAB h.string() ENDL;
// }
// void print(MyHypothesis& h) {
// 	print(h, S("")); // default null prefix
// }

void print(MyHypothesis& h) {
	COUT top.count(h) TAB  h.posterior TAB h.prior TAB h.likelihood TAB QQ(h.string()) ENDL;
}


// This gets called on every sample -- here we add it to our best seen so far (top) and
// print it every thin samples unless thin=0
void callback(MyHypothesis& h) {
	
	// if we find a new best, print it out
//	if(h->posterior > top.best_score()) 
//		print(*h, "# NewTop:");
	
	// add to the top
	top << h; 
	
	// print out with thinning
	if(thin > 0 && FleetStatistics::global_sample_count % thin == 0) 
		print(h);
}


////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv){ 
	using namespace std;
	
	// default include to process a bunch of global variables: mcts_steps, mcc_steps, etc
	auto app = Fleet::DefaultArguments("A LOT-based learner of children's sorting algorithms");
	app.add_option("-a,--alphabet", alphabet, "Alphabet we will use"); 	// add my own args
	app.add_option("-d,--data", datastr, "escape-semicolon separated list of input data strings");
	app.add_option("-p,--param", editDisParam, "edit distance parameter");		
	CLI11_PARSE(app, argc, argv);
	Fleet_initialize(); // must happen afer args are processed since the alphabet is in the grammar

	top.set_size(ntop); // set by above macro


	// mydata stores the data for the inference model
	MyHypothesis::t_data mydata;
	
	// top stores the top hypotheses we have found
	Fleet::Statistics::TopN<MyHypothesis> top(ntop);
	
	// declare a grammar with our primitives
	Grammar grammar(PRIMITIVES);

	// here we create an alphabet op with an "arg" that stores the character (this is faster than alphabet.substring with i.arg as an index) 
	// here, op_ALPHABET converts arg to a string (and pushes it)
	for(size_t i=0;i<alphabet.length();i++) {
		grammar.add<S>     (BuiltinOp::op_ALPHABET, Q(alphabet.substr(i,1)), 5.0/alphabet.length(), (int)alphabet.at(i)); 
	}

	
	//------------------
	// set up the data
	//------------------
	
	// we will parse the data from a comma-separated list of "data" on the command line
	for(auto di : split(datastr, ';')) {
		mydata.push_back( MyHypothesis::t_datum({S(""), di}) );
		CERR "# Data: " << di ENDL; // output data to check
	}

	cout << "# Edit Distance Paramter: " << std::to_string(editDisParam) << endl;
	
	//------------------
	// Run
	//------------------
	
	MyHypothesis h0(&grammar);
	h0 = h0.restart();
	

	ParallelTempering samp(h0, &mydata, top, NTEMPS, 1000.0);
	tic(); // start the timer
	// ParallelTempering samp(h0, &mydata, callback, 8, 1000.0, false);
	samp.run(Control(mcmc_steps, runtime, nthreads), 1000, 3000); //30000);

	tic(); // end timer
	
//	tic();
//	auto thechain = MCMCChain<MyHypothesis>(h0, &mydata, callback);
//	thechain.run(mcmc_steps, runtime);
//	tic();
//	



	double Z = top.Z();
	
	// We have a bunch of hypotheses in "top"
	DiscreteDistribution<S> string_marginals;
	for(auto h : top.values()) {
		auto o = h.call(S(""), S("<err>"), &h, 2048, 2048, -20.0);

		for(auto& s : o.values()) { // for each string in the output
			size_t commaCnt = std::count(s.first.begin(), s.first.end(), ',');
			// if(s.first.length() == 10) {
			if(commaCnt == 9) {
				string_marginals.addmass(s.first, s.second + (h.posterior - Z)) ;
			}
		}	
	}
	
	for(auto s : string_marginals.sorted()) {
		COUT s.second TAB QQ(s.first) ENDL;
	}
		
	
	// // Show the best we've found
	// top.print();

	// for(auto h : top.values()) {
	// 	auto o = h.call(S(""), S("<err>"), &h, 2048, 2048, -20.0);

	// 	int cnt = 0;
	// 	for(auto& s : o.values()) { // for each string in the output
	// 		cout << s.first << "\n";
	// 		cnt++;
	// 		if (cnt > 0) {
	// 			cnt = 0;
	// 			break;
	// 		}
	// 	} 
	// }
	
	COUT "# Global sample count:" TAB FleetStatistics::global_sample_count ENDL;
	COUT "# Elapsed time:" TAB elapsed_seconds() << " seconds " ENDL;
	COUT "# Samples per second:" TAB FleetStatistics::global_sample_count/elapsed_seconds() ENDL;
	
}
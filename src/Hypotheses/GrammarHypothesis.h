#pragma once 

#include <Eigen/Core>

#include "Numerics.h"
#include "EigenNumerics.h"

/* Helper functions for computing counts, C, LL, P */

template<typename HYP>
Vector counts(HYP& h) {
	// returns a 1xnRules matrix of how often each rule occurs
	// TODO: This will not work right for Lexicons, where value is not there
	
	auto c = h.grammar->get_counts(h.value);

	Vector out = Vector::Zero(c.size());
	for(size_t i=0;i<c.size();i++){
		out(i) = c[i];
	}
	
	return out;
}


template<typename HYP>
Matrix counts(std::vector<HYP>& hypotheses) {
	/* Returns a matrix of hypotheses (rows) by counts of each grammar rule.
	   (requires that each hypothesis use the same grammar) */
	
	size_t nRules = hypotheses[0].grammar->count_rules();
	
	Matrix out = Matrix::Zero(hypotheses.size(), nRules); 
	
	for(size_t i=0;i<hypotheses.size();i++) {
		out.row(i) = counts(hypotheses[i]);
		assert(hypotheses[i].grammar == hypotheses[0].grammar); // just a check that the grammars are identical
	}
	return out;
}

template<typename HYP>
Matrix incremental_likelihoods(std::vector<HYP>& hypotheses, std::vector<typename HYP::t_data>& alldata) {
	// Each row here is a hypothesis, and each column is the likelihood for a sequence of data sets
	// Here we check if the previous data point is a subset of the current, and if so, 
	// then we just do the additiona likelihood o fthe set difference 
	// this way, we can pass incremental data without it being too crazy slow
	
	Matrix out = Matrix::Zero(hypotheses.size(), alldata.size()); 
	
	for(size_t h=0;h<hypotheses.size();h++) {
		for(size_t di=0;di<alldata.size();di++) {
			if(di > 0 and alldata[di].size() >= alldata[di-1].size() and
			   std::equal(alldata[di-1].begin(), alldata[di-1].end(), alldata[di].begin())) {
					out(h, di) = out(h,di-1) + hypotheses[h].compute_likelihood(slice(alldata[di], alldata[di-1].size()));
			}
			else {
				out(h,di) =  hypotheses[h].compute_likelihood(alldata[di]);				
			}
		}	
	}
	
	return out;
}

template<typename HYP>
Matrix model_predictions(std::vector<HYP>& hypotheses, std::vector<typename HYP::t_datum>& predict_data) {
	
	Matrix out = Matrix::Zero(hypotheses.size(), predict_data.size());
	for(size_t h=0;h<hypotheses.size();h++) {
	for(size_t di=0;di<predict_data.size();di++) {
		out(h,di) = 1.0*hypotheses[h].callOne(predict_data[di].input, 0);
	}
	}
	return out;	
}


template<typename HYP>	// HYP here is the type of the thing we do inference over
class GrammarHypothesis : public MCMCable<GrammarHypothesis<HYP>, 
										  t_null, 
										  std::tuple<std::vector<typename HYP::t_data>,
												     std::vector<typename HYP::t_datum>,
													 std::vector<size_t>,
													 std::vector<size_t>>>  {
	/* This class stores a hypothesis of grammar probabilities. The t_data here is defined to be the above tuple and datum is ignored */
public:

	typedef std::tuple<std::vector<typename HYP::t_data>,
					   std::vector<typename HYP::t_datum>,
					   std::vector<size_t>,
					   std::vector<size_t>>
			t_data;
	typedef t_null t_datum;
			
	Vector x; 
	Grammar* grammar;
	Matrix* C;
	Matrix* LL;
	Matrix* P;
	
	float logodds_baseline; // when we choose at random, whats the probability of true-vs-false?
	float logodds_forwardalpha; 
	
	GrammarHypothesis(){};
	
	GrammarHypothesis(Grammar* g, Matrix* c, Matrix* ll, Matrix* p) : grammar(g), C(c), LL(ll), P(p) {
		x = Vector::Ones(grammar->count_rules());
	}	
	
	Vector& getX()                { return x; }
	const Vector& getX() const    { return x; }
	float get_baseline() const    { return exp(-logodds_baseline) / (1+exp(-logodds_baseline)); }
	float get_forwardalpha()const { return exp(-logodds_forwardalpha) / (1+exp(-logodds_forwardalpha)); }
	
	double compute_prior() {
		// we're going to put a cauchy prior on the transformed space
		this->prior = 0.0;
		
		for(auto i=0;i<x.size();i++) {
			this->prior += normal_lpdf(x(i), 0.0, 1.0);
		}
		
		this->prior += normal_lpdf(logodds_baseline, 0.0, 1.0);
		this->prior += normal_lpdf(logodds_forwardalpha, 0.0, 1.0);
		return this->prior;
	}
	
	virtual double compute_single_likelihood(const t_datum& datum) { assert(0); }
	
	virtual double compute_likelihood(const t_data& data, const double breakout=-infinity) {
		// This runs the entire model (computing its posterior) to get the likelihood
		// of the human data
		
		Vector hprior = hypothesis_prior(*C); 
		Matrix hposterior = (*LL).colwise() + hprior; // the model's posterior

		// now normalize it and convert to probabilities
		for(int i=0;i<hposterior.cols();i++) { 	// normalize (must be a faster way) for each amount of given data (column)
			hposterior.col(i) = lognormalize(hposterior.col(i)).array().exp();
		}
			
		Vector hpredictive = (hposterior.array() * (*P).array()).colwise().sum(); // elementwise product and then column sum
		
		
		// and now get the human likelihood, using a binomial likelihood (binomial term not needed)
		float forwardalpha = get_forwardalpha();
		float baseline     = get_baseline();
		this->likelihood = 0.0; // of the human data
		for(size_t i=0;i<std::get<2>(data).size();i++) {
			double p = forwardalpha*hpredictive(i) + (1.0-forwardalpha)*baseline;
			this->likelihood += log(p)*std::get<2>(data)[i] + log(1.0-p)*std::get<3>(data)[i];
		}
//		Vector o = Vector::Ones(predictive.size());
//		Vector p = forwardalpha*predictive(i) + o*(1.0-forwardalpha)*baseline; // vector of probabilities
//		double proposalLL = p.array.log()*yes_responses + (o-p).array.log()*no_responses;
		
		return this->likelihood;		
	}
	
	virtual GrammarHypothesis restart() const {
		GrammarHypothesis out(grammar, C, LL, P);
		out.logodds_baseline = normal(rng);		
		out.logodds_forwardalpha = normal(rng);		
		
		for(auto i=0;i<x.size();i++) {
			out.x(i) = normal(rng);
		}
		return out;
	}
	
	
	virtual std::pair<GrammarHypothesis,double> propose() const {
		GrammarHypothesis out = *this;
		
		if(flip()){
			out.logodds_baseline     = logodds_baseline     + 0.1*normal(rng);
			out.logodds_forwardalpha = logodds_forwardalpha + 0.1*normal(rng);
		}		
		else {
			auto i = myrandom(x.size()); // add noise to one coefficient
			out.getX()(i) = x(i) + 0.10*normal(rng);
		}
		
		return std::make_pair(out, 0.0);	
	}
	
	Vector hypothesis_prior(Matrix& C) {
		// take a matrix of counts (each row is a hypothesis, is column is a primitive)
		// and return a prior for each hypothesis under my own X
		// (IF this was just  aPCFG, which its not, we'd use something like lognormalize(C*proposal.getX()))

			
		Vector out(C.rows()); // one for each hypothesis
		
		// get the marginal probability of the counts via  dirichlet-multinomial
		Vector etothex = x.array().exp(); // translate [-inf,inf] into [0,inf]
		for(auto i=0;i<C.rows();i++) {
			
			double lp = 0.0;
			size_t offset = 0; // 
			for(size_t nt=0;nt<N_NTs;nt++) { // each nonterminal in the grammar is a DM
				size_t nrules = grammar->count_rules( (nonterminal_t) nt);
				if(nrules==1) continue;
				Vector a = eigenslice(etothex,offset,nrules); // TODO: seqN doesn't seem tow ork with this c++ version
				Vector c = eigenslice(C.row(i),offset,nrules);
				double n = a.sum();
				if(n==0) continue; // skip zero count else lgamma is crazy
				double a0 = c.sum();
				assert(a0 != 0.0);
				lp += std::lgamma(n+1) + std::lgamma(a0) - std::lgamma(n+a0) 
				     + (c.array() + a.array()).array().lgamma().sum() 
					 - (c.array()+1).array().lgamma().sum() 
					 - a.array().lgamma().sum();
//				CERR nt TAB n TAB a0 TAB lp TAB std::lgamma(n+1) TAB std::lgamma(a0) TAB std::lgamma(n+a0) 
//				     TAB (c.array() + a.array()).array().lgamma().sum() 
//					 TAB (c.array()+1).array().lgamma().sum() 
//					 TAB a.array().lgamma().sum() ENDL;
				offset += nrules;
			}
		
			out(i) = lp;
//			CERR out(i) TAB lp ENDL;
		}
		
		
		return out;		
	}
	
	
	
	
	
	virtual bool operator==(const GrammarHypothesis<HYP>& h) const {
		return (getX()-h.getX()).array().abs().sum() == 0.0;
	}
	
	
	// TODO: FIX THESE
	
	
	
	
	
	virtual std::string string() const { return "<NA>"; }
	virtual size_t hash() const        { return 1.0; }
	
};

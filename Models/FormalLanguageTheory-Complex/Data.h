#ifndef DATA_H
#define DATA_H

#include <cstring>
#include <vector>
#include <algorithm>

// Load data from a file and puts stringprobs into obs, which is a dictionary
// of strings to ndata total elements, and obsv, which is a vector that is sorted by probability
template<typename tdata>
void load_data_file(std::vector<tdata> &data, const char* datapath) {

	FILE* fp = fopen(datapath, "r");
	if(fp==NULL) { fprintf(stderr, "*** ERROR: Cannot open file! [%s]", datapath); exit(1);}
	
	char* line = NULL; size_t len=0; 
        
	char buffer[10000];
    
	double cnt;
	while( getline(&line, &len, fp) != -1 ) {
		if( line[0] == '#' ) continue;  // skip comments
		else if (sscanf(line, "%s\t%lf\n", buffer, &cnt) == 2) { // floats
			data.push_back(tdata({std::string(""), std::string(buffer), cnt}) );
		}
		else {
			fprintf(stderr, "*** ERROR IN PARSING INPUT [%s]\n", line);
			exit(1);
		}
	}
	fclose(fp);

	double z = 0.0;
	for(size_t i=0;i<data.size();i++) {
		z += data[i].reliability;
	}
	
	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TODO: Put all this into another library


template<typename T, typename TDATA>
std::map<T, double> highest(const std::vector<TDATA>& m, unsigned long N) {
	// take a type of data and make a map of strings to reliability, pulling out the top N
	// and converting into a map
	std::map<T, double> out;
	
	std::vector<TDATA> v = m; 
	std::sort(v.begin(), v.end(), [](auto x, auto y){ return x.reliability > y.reliability; });
	
	for(size_t i=0;i<MIN(N, v.size()); i++) {
		out[v[i].output] = v[i].reliability; // remember: must be output since that's what we're modeling
	}
	return out;
}



template<typename TDATA>
void print_precision_and_recall(std::ostream& output, DiscreteDistribution<std::string>& x, std::vector<TDATA>& data, unsigned long N) {
	// How many of the top N generated strings appear *anywhere* in the data
	// And how many of the top N data appear *anywhere* in the generated strings
	
	N = MIN(N, data.size()); // can't take more strings than are in the data, ok?
	
	auto A = x.best(N);
	auto B = highest<std::string,TDATA>(data,   N);
	
	std::set<std::string> mdata; // make a map of all observed output strings
	for(auto v : data) mdata.insert(v.output); 
	
	unsigned long nprec = 0;
	for(auto a: A) {
		//CERR a.first TAB a.second ENDL;
		if(mdata.count(a)) 
			nprec++;
	}
	
	unsigned long nrec  = 0;
	for(auto b : B){
		if(x.count(b.first))
			nrec++;
	}
	
	output << double(nprec)/A.size() TAB double(nrec)/B.size();
}




#endif
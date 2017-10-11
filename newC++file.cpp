#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<set>
#include<map>
#include<string>
#include<memory>
#include<iterator>
#include<algorithm>

class QueryResult;	// declaration needed for return type in the query function

class TextQuery {
public: 
	using line_no = std::vector<std::string>::size_type;
	TextQuery(std::ifstream&);
	QueryResult query(const std::string&) const;
private:
	std::shared_ptr<std::vector<std::string>> file;	//	input file
	//	map of each word to the set of the lines in which that word appears
	std::map<std::string, std::shared_ptr<std::set<line_no>>> wm;
};

// read the input file and build the map of lines to line numbers
TextQuery::TextQuery(std::ifstream &is) : file(new std::vector<std::string>) {
	std::string text;
	while (getline(is, text)) {		// for each line in the file
		file->push_back(text);		// remember this line of text
		int n = file->size() - 1;	// the current line number
		std::istringstream line(text);	// separate the line into words
		std::string word;
		while (line >> word) {		// for each word in that line
			// if word isn't already in wm, subscripting adds a new entry
			auto &lines = wm[word];	// lines is a shared_ptr
			if (!lines)		// that pointer is null the first time we see word
				lines.reset(new std::set<line_no>);		// allocate a new set
			lines->insert(n);
		}
	}
}

class QueryResult {
	friend std::ostream& print(std::ostream&, const QueryResult&);
public:
	QueryResult(std::string s, std::shared_ptr<std::set<TextQuery::line_no>> p, std::shared_ptr<std::vector<std::string>> f): sought(s), lines(p), file(f) {}
	auto begin() { return lines->begin(); }
	auto end() { return lines->end(); }
	std::shared_ptr<std::vector<std::string>> get_file() { return file; }
private:
	std::string sought;		// word this query represents
	std::shared_ptr<std::set<TextQuery::line_no>> lines;	// lines it's on
	std::shared_ptr<std::vector<std::string>> file;	// input file
};

QueryResult TextQuery::query(const std::string& sought) const {
	// return a pointer to this set if we don't find sought
	static std::shared_ptr<std::set<line_no>> nodata(new std::set<line_no>);
	// use find and not a subscript to avoid adding word to wm
	auto loc = wm.find(sought);
	if (loc == wm.end())
		return QueryResult(sought, nodata, file);	// not found
	else
		return QueryResult(sought, loc->second, file);
}

// used in the function "print"
std::string make_plural(size_t ctr, const std::string &word, const std::string &ending) {
	return (ctr > 1) ? word + ending : word;
}

std::ostream& print(std::ostream &os, const QueryResult &qr) {
	// if the word was found, print the count and all occurrences
	os << qr.sought << " occurs " << qr.lines->size() << " " << make_plural(qr.lines->size(), "time", "s") << std::endl;
	// print each line in which the word appeared
	for (auto num : *qr.lines)
		os << "\t(line " << num + 1 << ") " << *(qr.file->begin() + num) << std::endl;
	return os;
}

class Query_base {
	friend class Query;
protected:
	using line_no = TextQuery::line_no;	//used in the eval functions
	virtual ~Query_base() = default;
private:
	// eval returns the QueryResult that matches this Query
	virtual QueryResult eval(const TextQuery&) const = 0;
	// rep is a string representation of the query
	virtual std::string rep() const = 0;
};

class Query {
	// these operator need access to the shared_ptr constructor
	friend Query operator~(const Query&);
	friend Query operator|(const Query&, const Query&);
	friend Query operator&(const Query&, const Query&);
public:
	Query(const std::string&);	//builds a new WordQuery
	// interface fuctions: call the corresponding Query_base operations
	QueryResult eval(const TextQuery &t) const { return q->eval(t); }
	std::string rep() const { return q->rep(); }
private:
	Query(std::shared_ptr<Query_base> query): q(query) {}
	std::shared_ptr<Query_base> q;
};

std::ostream& operator<<(std::ostream &os, const Query &query) {
	// Query::rep makes a virtyal call through its Query_base pointer to rep()
	return os << query.rep();
}

class WordQuery : public Query_base {
	friend class Query; // Query uses the WordQuery constructor
	WordQuery(const std::string &s): query_word(s) {}
	// concrete class: WordQuery defines all inherited pure virtual functions
	QueryResult eval(const TextQuery &t) const { return t.query(query_word); }
	std::string rep() const { return query_word; }
	std::string query_word;		// word for which to search
};

inline Query::Query(const std::string &s): q(new WordQuery(s)) {}

class NotQuery : public Query_base {
	friend Query operator~(const Query&);
	NotQuery(const Query &q): query(q) {}
	// concrete class: NotQuery defines all inherited pure virtual functions
	std::string rep() const { return "~(" + query.rep() + ")"; }
	QueryResult eval(const TextQuery&) const;
	Query query;
};

inline Query operator~(const Query &operand) {
	return std::shared_ptr<Query_base>(new NotQuery(operand));
}

class BinaryQuery : public Query_base {
protected:
	BinaryQuery(const Query &l, const Query &r, std::string s): lhs(l), rhs(r), opSym(s) {}
	// abstract class: BinaryQuery doesn't define eval
	std::string rep() const { return "(" + lhs.rep() + " " + opSym + " " + rhs.rep() + ")"; }
	Query lhs, rhs;		// right- and left-hand operands
	std::string opSym;
};

class AndQuery : public BinaryQuery {
	friend Query operator&(const Query&, const Query&);
	AndQuery(const Query &left, const Query &right) : BinaryQuery(left, right, "&") {}
	//	concrete class: AndQuery inherits rep and defines the remaining pure virtual
	QueryResult eval(const TextQuery&) const;
};

inline Query operator&(const Query &lhs, const Query &rhs) {
	return std::shared_ptr<Query_base>(new AndQuery(lhs, rhs));
}

class OrQuery : public BinaryQuery {
	friend Query operator|(const Query&, const Query&);
	OrQuery(const Query &left, const Query &right) : BinaryQuery(left, right, "|") {}
	//	concrete class: OrQuery inherits rep and defines the remaining pure virtual
	QueryResult eval(const TextQuery&) const;
};

inline Query operator|(const Query &lhs, const Query &rhs) {
	return std::shared_ptr<Query_base>(new OrQuery(lhs, rhs));
}

QueryResult OrQuery::eval(const TextQuery& text) const {
	// virtual calls through the Query members, lhs and rhs
	// the calls to eval return the QueryResult for each operand
	auto right = rhs.eval(text), left = lhs.eval(text);
	// copy the line numbers from the left-hand operand into the result set
	auto ret_lines = std::make_shared<std::set<line_no>>(left.begin(), left.end());
	// insert lines from the right-hand operand
	ret_lines->insert(right.begin(), right.end());
	// return the new QueryResult representing the union of lhs and rhs
	return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult AndQuery::eval(const TextQuery& text) const {
	// virtual calls through the Query operands to get result sets for the operands
	auto left = lhs.eval(text), right = rhs.eval(text);
	// set to hold the intersection of left and right
	auto ret_lines = std::make_shared<std::set<line_no>>();
	// writes the intersection of two ranges to a destination iterator
	// destination iterator in this call adds elements to ret
	std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), inserter(*ret_lines,ret_lines->begin() ) ); // use set.begin instead of inserter
	return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult NotQuery::eval(const TextQuery& text) const {
	// virtual call to eval through the Query operand
	auto result = query.eval(text);
	// start out with an empty result set
	auto ret_lines = std::make_shared<std::set<line_no>>();
	// we have to iterate through the lines on which our operand appears
	auto beg = result.begin(), end = result.end();
	// for each line in the input file, if that is not in result, add that line number to ret_lines
	auto sz = result.get_file()->size();
	for (size_t n = 0; n != sz; ++n) {
		// if we haven't processed all the lines in result
		// check whether this line is present
		if (beg == end || *beg != n)
			ret_lines->insert(n);	// if not in result, add this line
		else if (beg != end)
			++beg;	// otherwise get the next line number in result if there is one
	}
	return QueryResult(rep(), ret_lines, result.get_file());
}

int main()
{
	std::ifstream ifs("paragraph.txt");
	TextQuery tq(ifs);
	Query q1("theeeeeee");
	Query q2("LICENCE");
	print(std::cout, q1.eval(tq));
	print(std::cout, (~q2).eval(tq));
	print(std::cout, (q1 & q2).eval(tq));
	print(std::cout, (q1 | q2).eval(tq));
	return 0;
}
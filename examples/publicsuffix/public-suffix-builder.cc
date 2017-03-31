#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Require libidn for handling international domain names.
#include <idna.h>

#define PHF_DEBUG 2
#include "phf/builder.h"

#include "public-suffix-types.h"

using namespace public_suffix;

struct BuildContext
{
	std::size_t aux_tables = 0;
	std::size_t max_label_size = 0;
};

class Suffix
{
public:
	Suffix(Rule rule, const std::string &label) : rule_(rule), label_(label)
	{
	}

	auto GetSuffix(const std::string &label)
	{
		return std::find_if(
			next_level_.begin(), next_level_.end(),
			[&label](const Suffix &suffix) { return suffix.label_ == label; });
	}

	void AddSuffix(Rule rule, const std::string &next, const std::string &first,
		       const std::string &rest)
	{
		auto delim = next.find_last_of('.');
		if (delim == std::string::npos) {
			auto it = GetSuffix(next);
			if (it == next_level_.end())
				next_level_.emplace_back(rule, next);
			else if (it->rule_ != Rule::kDefault)
				throw std::runtime_error("Duplicate name: " + rest + "."
							 + first);
			else
				it->rule_ = rule;
		} else {
			std::string last = next.substr(delim + 1);
			std::string more = next.substr(0, delim);
			auto it = GetSuffix(last);
			if (it == next_level_.end()) {
				next_level_.emplace_back(Rule::kDefault, last);
				next_level_.back().AddSuffix(rule, more, first, rest);
			} else {
				it->AddSuffix(rule, more, first, rest);
			}
		}
	}

	void Dump(std::ostream &stream, size_t level) const
	{
		stream << "// ";
		for (size_t i = 0; i < level; i++)
			stream << "  ";
		if (rule_ == Rule::kException)
			stream << '!';
		else if (rule_ == Rule::kWildcard)
			stream << "*.";
		stream << label_;
		stream << "\n";

		for (const auto &suffix : next_level_)
			suffix.Dump(stream, level + 1);
	}

	void BuildPrepare(BuildContext &ctx)
	{
		auto size = label_.size() + 1;
		if (ctx.max_label_size < size)
			ctx.max_label_size = size;

		if (!next_level_.empty()) {
			for (auto &suffix : next_level_)
				suffix.BuildPrepare(ctx);

			auto aux_table_no = ctx.aux_tables++;
			node_ = "node_" + std::to_string(aux_table_no);
		}
	}

	void BuildNode()
	{
		if (!next_level_.empty()) {
			for (auto &suffix : next_level_)
				suffix.BuildNode();

			std::cout << "Node " << node_ << "[] = {\n";
			for (auto &suffix : next_level_) {
				std::cout << "\t{\"" << suffix.label_ << "\", "
					  << suffix.next_level_.size() << ", " << suffix.rule_;
				if (!suffix.next_level_.empty())
					std::cout << ", " << suffix.node_;
				std::cout << "},\n";
			}
			std::cout << "};\n\n";
		}
	}

private:
	Rule rule_;
	std::string label_;
	std::vector<Suffix> next_level_;

	std::string node_;

	friend class SuffixRoot;
};

class SuffixRoot
{
public:
	void AddSingle(const std::string &label)
	{
		auto it = std::find_if(first_level_.begin(), first_level_.end(),
				       [&label](auto suffix) { return suffix == label; });
		if (it != first_level_.end())
			throw std::runtime_error("Duplicate name: " + label);
		first_level_.emplace_back(label);
	}

	void AddDouble(Rule rule, const std::string &label)
	{
		auto it = second_level_.find(label);
		if (it == second_level_.end())
			second_level_.emplace(label, Suffix(rule, label));
		else if (it->second.rule_ != Rule::kDefault)
			throw std::runtime_error("Duplicate name: " + label);
		else
			it->second.rule_ = rule;
	}

	void AddMultiple(Rule rule, const std::string &first, const std::string &rest)
	{
		auto it = second_level_.find(first);
		if (it == second_level_.end())
			it = second_level_.emplace(first, Suffix(Rule::kDefault, first)).first;
		it->second.AddSuffix(rule, rest, first, rest);
	}

	void BuildMPHF()
	{
		auto seed = phf::random_device_seed{}();
		phf::builder<16, std::string, Fnv64> builder(4, seed);
		for (const auto &suffix : second_level_)
			builder.insert(suffix.second.label_);

		auto mph = builder.build();

		std::vector<Suffix *> index(second_level_.size());
		for (auto &suffix : second_level_) {
			auto i = (*mph)[suffix.second.label_];
			if (i >= index.size())
				throw std::runtime_error("mph produced invalid index "
							 + std::to_string(i));
			index[i] = &suffix.second;
		}

		BuildContext ctx;
		for (auto suffix : index) {
			suffix->BuildPrepare(ctx);
		}
		for (auto &label : first_level_) {
			auto size = label.size() + 1;
			if (ctx.max_label_size < size)
				ctx.max_label_size = size;
		}

		std::cout << "#include \"public-suffix-types.h\"\n\n";
		std::cout << "using namespace public_suffix;\n\n";
		std::cout << "namespace {\n\n";
		std::cout << "struct Node {\n";
		std::cout << "\tchar label[" << ctx.max_label_size << "];\n";
		std::cout << "\tuint8_t size;\n";
		std::cout << "\tRule rule;\n";
		std::cout << "\tNode* node;\n";
		std::cout << "};\n\n";

		for (auto &suffix : index) {
			if (!suffix->next_level_.empty())
				suffix->BuildNode();
		}

		std::cout << "Node second_level_nodes[] = {\n";
		for (auto suffix : index) {
			std::cout << "\t{\"" << suffix->label_ << "\", "
				  << suffix->next_level_.size() << ", " << suffix->rule_;
			if (!suffix->next_level_.empty())
				std::cout << ", " << suffix->node_;
			std::cout << "},\n";
		}
		std::cout << "};\n\n";

		std::cout << "Node first_level_nodes[] = {\n";
		for (auto &label : first_level_)
			std::cout << "\t{\"" << label << "\", 0, Rule::kWildcard},\n";
		std::cout << "};\n\n";

		std::cout << "} // namespace\n";
	}

	void Dump(std::ostream &stream) const
	{
		stream << "//\n";
		for (const auto &label : first_level_)
			stream << "// *." << label << "\n";
		stream << "//\n";
		for (const auto &suffix : second_level_)
			suffix.second.Dump(stream, 0);
		stream << "//\n";
	}

private:
	// Main suffix table. Contains all entries with at least two labels
	// such "co.uk", "foo.co.uk" etc.
	std::unordered_map<std::string, Suffix> second_level_;

	// Additional suffix table. Contains only wildcard entries with
	// single specified label such as "*.ck"
	std::vector<std::string> first_level_;
};

int
main(int ac, char *av[]) try {
	if (ac != 1) {
		std::cerr << "Usage: " << av[0] << "<input-file >output-file\n";
		return EXIT_FAILURE;
	}

	SuffixRoot root;

	for (;;) {
		std::string line;
		if (!std::getline(std::cin, line))
			break;

		// Trim everything after the first white space.
		std::string data = line.substr(0, line.find_first_of(" \t\r"));
		// Skip empty lines and comments.
		if (data.empty() || data.front() == '/')
			continue;

		// Trim a trailing dot if any. From this point onward it is
		// safe to access the char just after any found dot as it
		// cannot be at the string end.
		if (data.back() == '.') {
			data.pop_back();
			// Two consecutive dots are not allowed. A solitary
			// dot is not allowed as well.
			if (data.empty() || data.back() == '.')
				throw std::runtime_error("Invalid line: " + line);
		}

		// Presume the line contains just a regular host name.
		Rule rule = Rule::kRegular;
		size_t skip = 0;

		// Check for special cases: a wildcard or exception rule.
		if (data.front() == '!') {
			rule = Rule::kException;
			skip = 1;
		} else if (data.front() == '*') {
			rule = Rule::kWildcard;
			for (++skip; skip < data.size(); skip += 2) {
				if (data[skip] != '.')
					throw std::runtime_error("Invalid line: " + line);
				if (data[skip + 1] != '*')
					break;
			}
		}

		// Skip a leading dot if any. From this point onward it is safe
		// to access the char just before any found dot.
		if (skip < data.size() && data[skip] == '.') {
			++skip;
			// Two consecutive dots are not allowed. A solitary
			// dot is not allowed as well.
			if (skip >= data.size() || data[skip] == '.')
				throw std::runtime_error("Invalid line: " + line);
		}

		// Encode a possibly international name.
		char *idn_result;
		if (skip >= data.size())
			throw std::runtime_error("Invalid line: " + line);
		if (idna_to_ascii_8z(data.data() + skip, &idn_result, 0) != IDNA_SUCCESS)
			throw std::runtime_error("Invalid line: " + line);
		std::unique_ptr<char, decltype(&std::free)> idn_free(idn_result, &std::free);

		// Verify that the name contains only valid chars.
		data.assign(idn_result);
		for (auto c : data) {
			if (!std::isalnum(c) && c != '.' && c != '-')
				throw std::runtime_error("Invalid line: " + line);
		}

		// Seek where the rightmost label starts.
		auto delim = data.find_last_of('.');
		if (delim == std::string::npos) {
			// If this is a trivial TLD then don't bother with it.
			// However remember it if it as a wildcard rule.
			if (rule == Rule::kWildcard)
				root.AddSingle(data);
		} else {
			if (data[delim - 1] == '.')
				throw std::runtime_error("Invalid line: " + line);

			delim = data.find_last_of('.', delim - 1);
			if (delim == std::string::npos)
				root.AddDouble(rule, data);
			else
				root.AddMultiple(rule, data.substr(delim + 1),
						 data.substr(0, delim));
		}
	}

	//root.Dump(std::cout);
	root.BuildMPHF();

	return EXIT_SUCCESS;
} catch (std::exception &e) {
	std::cerr << "Exception occurred: " << e.what() << '\n';
	return EXIT_FAILURE;
}

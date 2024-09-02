#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>

template <typename T> using ParseNextDataFn = bool(*)(std::ifstream& /*stream*/, T& /*data*/, std::string& /*error*/);

template <typename D, template<typename>typename T> class FileParseItr {
public:
	FileParseItr() = delete;

	FileParseItr(const char*  s, T<D> p, bool bin=false) : parser(p) { std::filesystem::path f(s ? s : ""); init(f, bin); }
	FileParseItr(std::string& s, T<D> p, bool bin=false) : parser(p) { std::filesystem::path f(s); init(f, bin); }
	FileParseItr(std::filesystem::path f, T<D> p, bool bin=false) : parser(p) { init(f, bin); }

	FileParseItr(const char*  s, bool bin=false) { std::filesystem::path f(s ? s : ""); init(f, bin); }
	FileParseItr(std::string& s, bool bin=false) { std::filesystem::path f(s); init(f, bin); }
	FileParseItr(std::filesystem::path f, bool bin=false) { init(f, bin); }

	// Rule of 5: No Copy or Move constructors
	FileParseItr(const FileParseItr&) = delete;
	FileParseItr(FileParseItr&&) = delete;
	FileParseItr& operator=(const FileParseItr&) = delete;
	FileParseItr& operator=(FileParseItr&&) = delete;

	~FileParseItr() { if(fs.is_open()) fs.close(); }

	class iterator {
	public:
		using value_type = D;
		using difference_type = std::ptrdiff_t; //Note difference is note pointer diff
		using pointer = D*;
		using reference = D&;
		using iterator_category = std::input_iterator_tag;

		iterator(FileParseItr& p) : fpi(p) {}

		void next() {
			if ( ! fpi.parser(fpi.fs, fpi.data, fpi.err)) {
				eof = fpi.fs.eof();
				if ( ! eof) { fpi.parse_failure = true; }
				if (fpi.copf) {
					if (eof) { return; }
					next();
				} else if ( ! eof && fpi.err.empty()) {
					fpi.err = "Failed to parse next section of data";
				}
				return;
			}

			iNext = fpi.fs.tellg();
		}


		D& operator*() {
			return fpi.data;
		}

		const D& operator*() const {
			return fpi.data;
		}

		iterator &operator++() {
			if (fpi.failed() || (fpi.copf && eof)) {
				iterator e = end();
				iNext = e.iNext;
				eof = e.eof;
			}
			else { next(); }
			return *this;
		}

		// Post increment
		void operator++(int) {
			++*this;
		}

		friend bool operator==(const iterator &lhs, const iterator &rhs) {
			return lhs.iNext == rhs.iNext && lhs.eof == rhs.eof;
		}

		friend bool operator!=(const iterator &lhs, const iterator &rhs) {
			return lhs.iNext != rhs.iNext || lhs.eof != rhs.eof;
		}

		iterator end() {
			iterator e(fpi);
			std::istream& efs = e.fpi.fs;
			if (fpi.copf) { efs.clear(); }
			std::streampos pos = efs.tellg();
			efs.seekg(0, efs.end);
			e.iNext = efs.tellg();
			efs.seekg(pos, efs.beg);
			e.eof = true;
			return e;
		}

	protected:
		FileParseItr& fpi;
		std::streampos iNext;
		bool eof{false};
	};

	iterator begin() {
		iterator b(*this);
		if (failed()) { return b.end(); }
		b.next();
		return b;
	}

	iterator end() {
		iterator e(*this);
		return e.end();
	}

	bool failed() {
		bool failed = false;
		if ( ! err.empty()) {
			failed = copf ? !parse_failure : true;
		} else if ( ! fs) {
			if ( ! fs.eof()) {
				err = "Input file stream failure";
				failed = true;
			}
		} else if ( ! fs.is_open()) {
			err = "Failed to open file";
			failed = true;
		}
		return failed;
	}

	std::string error() const { return err; }
	const char* c_error() const { return err.c_str(); }

	FileParseItr& ContOnParseFailure(bool copf=false) { this->copf = copf; return *this; }

protected:
	std::filesystem::path path;
	std::ifstream fs;

	T<D> parser;
	D data;

	std::string err;

	bool binary{false};
	bool copf{false};
	bool parse_failure{false};
private:
	void init(std::filesystem::path &p, bool binary_mode) {
		path.clear();
		path = p;

		bool setup_failure = false;
		if (path.empty()) {
			err = "No file path provided";
			setup_failure = true;
		} else if ( ! std::filesystem::exists(path)) {
			err = "Provided file path does not exist";
			setup_failure = true;
		} else if ( ! std::filesystem::is_regular_file(path)) {
			err = "Provided file path is not a file";
			setup_failure = true;
		}

		if (setup_failure) { return; }

		binary = binary_mode;
		std::ios_base::openmode mode = std::ifstream::in;
		if (binary) { mode |= std::ifstream::binary; }

		fs.open(path.string(), mode);
	}

};

//####################################################################
// Testing
// Includes for external data processing objects and functions
#include <vector>
#include <map>
#include <stdio.h>
typedef std::map<std::string, std::string> DataMap;

enum class DataType {
	FOO = 0,
	BAR,
	BAZ,
};

std::vector<DataType> types = { DataType::FOO, DataType::BAR, DataType::BAZ };

struct Data {
	DataType t;
	DataMap info;
	void clear() { info.clear(); t = DataType::FOO; }
	void insert(std::string key, std::string value) { info.insert_or_assign(key, value); }
	void type(DataType &dt) { t = dt; }
};

template <typename T>
bool cust_parse(std::ifstream &stream, T &data, std::string& error) {
	data.clear();
	std::string line;
	if ( ! std::getline(stream, line)) { return false; }
	static int count = 0;
	data.type(types[count++ % 3]);
	data.insert("info", line);
	//if (count == 2) { error = "Custom internal error"; return false; }
	return true;
}

void test_iter(FileParseItr<Data, ParseNextDataFn>& test) {
	printf("###############Testing#################\n");
	int i = 0;
	for (auto& data : test) {
		printf("Line (%d) :", i++);
		int k=0;
		for (const auto& [key, line] : data.info) {
			++k;
			switch(data.t) {
				case DataType::FOO:
					printf("Foo: %s\n", line.c_str());
					break;
				case DataType::BAR:
					printf("Bar: %s\n", line.c_str());
					break;
				case DataType::BAZ:
					printf("Baz: %s\n", line.c_str());
					break;
				default:
					printf("UhOh DataType out of range\n");
			}
		}
		if ( ! k) { printf("\n"); }
		if (i >= 6) { break; }
	}
	if (test.failed()) {
		printf("Error: %s\n", test.error().c_str());
	}
}

template<typename T>
struct foo {
	bool operator()(std::ifstream& stream, T& data, std::string& /*error*/) {
		printf("%d -> ", count++);
		if ( ! std::getline(stream, data)) { return false; }
		return true;
	}
	int count{0};
};

int main() {
	//std::string filename("foo");
	std::string filename("forLoopParser.cpp");
	std::filesystem::path filePath(filename);

	FileParseItr<Data, ParseNextDataFn> test_std_path(filePath, cust_parse);
	test_iter(test_std_path);

	FileParseItr<Data, ParseNextDataFn> test_std_str(filename, cust_parse);
	test_iter(test_std_str);

	FileParseItr<std::string, foo> test_c_str(filename.c_str());
	for (const auto& data : test_c_str) {
		printf("%s\n", data.c_str());
	}

	return 0;
}


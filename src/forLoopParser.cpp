#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>

template <typename T> using ParseNextDataFn = bool(*)(std::ifstream& /*stream*/, T& /*data*/, std::string& /*error*/);
template <typename T> class FileParseItr {
public:
	FileParseItr() = delete;
	FileParseItr(const char*  s, ParseNextDataFn<T> func, bool bin=false) { pfn = func; std::filesystem::path p(s ? s : ""); init(p, bin); }
	FileParseItr(std::string& s, ParseNextDataFn<T> func, bool bin=false) { pfn = func; std::filesystem::path p(s); init(p, bin); }
	FileParseItr(std::filesystem::path p, ParseNextDataFn<T> func, bool bin=false) { pfn = func; init(p, bin); }
	~FileParseItr() { if(fs.is_open()) fs.close(); }

	class iterator {
	public:
		using value_type = T;
		using difference_type = std::ptrdiff_t; //Note difference is note pointer diff
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::input_iterator_tag;

		iterator(FileParseItr& p) : fpi(p) {}

		void next() {
			if ( ! fpi.pfn(fpi.fs, fpi.data, fpi.err)) {
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


		T& operator*() {
			return fpi.data;
		}

		const T& operator*() const {
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

	FileParseItr& ContOnParseFailure() { copf = true; return *this; }
	FileParseItr& FailOnParseFailure() { copf = false; return *this; }

protected:
	std::filesystem::path path;
	std::ifstream fs;

	ParseNextDataFn<T> pfn = nullptr;
	T data;

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
		} else if (pfn == nullptr) {
			err = "No parse function pointer provided";
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

void test_iter(FileParseItr<Data>& test) {
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

int main() {
	//std::string filename("foo");
	std::string filename("py_dag_opts.py");
	std::filesystem::path filePath(filename);

	FileParseItr<Data> test_no_func(filePath, nullptr);
	test_iter(test_no_func);

	FileParseItr<Data> test_empty_str(nullptr, cust_parse);
	test_iter(test_empty_str);

	FileParseItr<Data> test_std_path(filePath, cust_parse);
	test_iter(test_std_path);

	FileParseItr<Data> test_std_str(filename, cust_parse);
	test_iter(test_std_str);

	FileParseItr<Data> test_c_str(filename.c_str(), cust_parse);
	test_iter(test_c_str);

	return 0;
}


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

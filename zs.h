#ifndef ZS_H
#define ZS_H

#include <functional>
#include <string>

class ZS {
private:
public:
	typedef size_t filesize_t;
	std::string error;
	bool decompress(std::function<int (char *, int)> const &reader, std::function<int (char const *, int)> const &writer, filesize_t maxlen = -1);
	bool compress(std::function<int (char *, int)> const &in_fn, std::function<int (char const *, int)> const &out_fn);
};

#endif // ZS_H

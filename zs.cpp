
#include "zs.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <zstd.h>

namespace {
template <typename T> void free_(T *p);
template <> void free_(ZSTD_DCtx *p) { ZSTD_freeDCtx(p); }
template <> void free_(ZSTD_CCtx *p) { ZSTD_freeCCtx(p); }

template <typename T> class Context {
private:
	T *ctx;
public:
	explicit Context(T *p)
	{
		ctx = p;
	}
	~Context()
	{
		if (ctx) {
			free_(ctx);
		}
	}
	operator bool () const
	{
		return (bool)ctx;
	}
	operator T * () const
	{
		return ctx;
	}
};
}

bool ZS::decompress(std::function<int (char *, int)> const &in_fn, std::function<int (char const *, int)> const &out_fn, filesize_t maxlen)
{
	error = {};

	const size_t buffInSize = ZSTD_DStreamInSize();
	const size_t buffOutSize = ZSTD_DStreamOutSize();
	std::unique_ptr<char> buffer(new char [buffInSize + buffOutSize]);
	char *const buffIn = buffer.get();
	char *const buffOut = buffIn + buffInSize;

	Context<ZSTD_DCtx> dctx(ZSTD_createDCtx());
	if (!dctx) {
		error = "ZSTD_createDCtx() failed";
		return false;
	}

	const size_t toRead = buffInSize;
	filesize_t total = 0;
	bool isEmpty = true;
	while (1) {
		const int read = in_fn(buffIn, (int)toRead);
		if (read < 1) break;
		isEmpty = false;
		ZSTD_inBuffer input = { buffIn, (size_t)read, 0 };
		while (input.pos < input.size) {
			ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
			const size_t ret = ZSTD_decompressStream(dctx, &output , &input);
			if (ZSTD_isError(ret)) {
				error = ZSTD_getErrorName(ret);
				return false;
			}
			const int len = (int)output.pos;
			out_fn(buffOut, len);
			total += len;
			if (maxlen != (filesize_t)-1 && total >= maxlen) {
				goto done;
			}
		}
	}
done:;
	if (isEmpty) {
		error = "input is empty";
		return false;
	}
	return true;
}

bool ZS::compress(std::function<int (char *, int)> const &in_fn, std::function<int (char const *, int)> const &out_fn)
{
	error = {};

	const int CLEVEL = ZSTD_CLEVEL_DEFAULT;

	const size_t buffInSize = ZSTD_CStreamInSize();
	const size_t buffOutSize = ZSTD_CStreamOutSize();
	std::unique_ptr<char> buffer(new char [buffInSize + buffOutSize]);
	char *const buffIn = buffer.get();
	char *const buffOut = buffIn + buffInSize;

	Context<ZSTD_CCtx> cctx(ZSTD_createCCtx());
	if (!cctx) {
		error = "ZSTD_createCCtx() failed";
		return false;
	}

	size_t ret;
	ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, CLEVEL);
	if (ZSTD_isError(ret)) {
		error = ZSTD_getErrorName(ret);
		return false;
	}
	ret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
	if (ZSTD_isError(ret)) {
		error = ZSTD_getErrorName(ret);
		return false;
	}

	const size_t toRead = buffInSize;
	while (1) {
		const int read = in_fn(buffIn, (int)toRead);
		const bool lastChunk = (read < (int)toRead);
		const ZSTD_EndDirective mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
		ZSTD_inBuffer input = { buffIn, (size_t)read, 0 };
		int finished;
		do {
			ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
			const size_t remaining = ZSTD_compressStream2(cctx, &output , &input, mode);
			if (ZSTD_isError(remaining)) {
				error = ZSTD_getErrorName(remaining);
				return false;
			}
			out_fn(buffOut, (int)output.pos);
			finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
		} while (!finished);

		if (input.pos != input.size) {
			error = "zstd only returns 0 when the input is completely consumed";
			return false;
		}
		if (lastChunk) break;
	}

	return true;
}

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	char const *in_file = "zs.cpp";
	char const *compressed_file = "__compressed.zst";
	char const *decompressed_file = "__decompressed";
	{
		int fd_in = open(in_file, O_RDONLY);
		if (fd_in != -1) {
			int fd_out = open(compressed_file, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
			if (fd_out != -1) {
				ZS zs;
				auto in = [&](char *ptr, int len)->int{
					return read(fd_in, ptr, len);
				};
				auto out = [&](char const *ptr, int len)->int{
					return write(fd_out, ptr, len);
				};
				zs.compress(in, out);
			}
		}
	}
	{
		int fd_in = open(compressed_file, O_RDONLY);
		if (fd_in != -1) {
			int fd_out = open(decompressed_file, O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
			if (fd_out != -1) {
				ZS zs;
				auto in = [&](char *ptr, int len)->int{
					return read(fd_in, ptr, len);
				};
				auto out = [&](char const *ptr, int len)->int{
					return write(fd_out, ptr, len);
				};
				zs.decompress(in, out);
			}
		}
	}
	return 0;
}

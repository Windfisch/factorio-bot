#include <iostream>
#include <streambuf>
#include <vector>


class prefixbuf : public std::streambuf
{
	std::string prefix;
	std::streambuf* sbuf;
	bool need_prefix;

	int sync() {
		return this->sbuf->pubsync();
	}
	int overflow(int c) {
		if (c != std::char_traits<char>::eof()) {
			if (this->need_prefix
					&& !this->prefix.empty()
					&& this->prefix.size() != this->sbuf->sputn(&this->prefix[0], this->prefix.size())) {
				return std::char_traits<char>::eof();
			}
			this->need_prefix = c == '\n';
		}
		return this->sbuf->sputc(c);
	}

	public:
		prefixbuf(std::string const& prefix, std::streambuf* sbuf_) : prefix(prefix), sbuf(sbuf_), need_prefix(true) {}
};

class Logger : private virtual prefixbuf, public std::ostream
{
	public:
		Logger(std::string const& topic)
			: prefixbuf(get_prefix(topic) + ": ", std::cout.rdbuf())
			 , std::ios(static_cast<std::streambuf*>(this))
			 , std::ostream(static_cast<std::streambuf*>(this))
		{
			stack.push_back(topic);
		}
		~Logger()
		{
			stack.pop_back();
		}
	private:
		static std::string get_prefix(std::string tail)
		{
			if (stack.empty()) return tail;
			std::string result = stack[0];
			for (size_t i=1; i < stack.size(); i++)
				result += "." + stack[i];
			return result + "." + tail;
		}

		static std::vector<std::string> stack;
};

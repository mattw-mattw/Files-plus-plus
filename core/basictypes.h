// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <string>
#include <assert.h>
#include <set>
#include <functional>

#include <filesystem>
namespace fs = std::filesystem;

#include <megaapi.h>
namespace m = ::mega;

typedef std::shared_ptr<m::MegaApi> OwningApiPtr;
typedef std::weak_ptr<m::MegaApi> ApiPtr;

template<class T> 
struct copy_ptr : public std::unique_ptr<T>
{
	copy_ptr() {}
	explicit copy_ptr(T*) : unique_ptr(p) {}
	explicit copy_ptr(const copy_ptr& p) : unique_ptr(p ? p->copy() : nullptr) {}
	explicit copy_ptr(copy_ptr&& p) noexcept : unique_ptr(p.release()) {}
	void operator=(const copy_ptr& p) { reset(p ? p->copy() : nullptr); }
	void operator=(copy_ptr&& p) noexcept { reset(p.release()); }
	copy_ptr copy() const { return copy_ptr(*this); }
};

template<class T>
class NotifyQueue
{
	std::deque<T> queue;
	std::mutex m;
	std::condition_variable cv;
public:
	void push(T&& t)
	{
		std::lock_guard<std::mutex> g(m);
		queue.emplace_back(move(t));
		cv.notify_one();
	}
	bool pop(T& value, bool wait)
	{
		std::unique_lock<mutex> g(m);
        if (wait && queue.empty()) cv.wait(g);
        if (queue.empty()) return false;
		value = std::move(queue.front());
		queue.pop_front();
		return true;
	}
};

struct OwnStr : public std::unique_ptr<char[]>
{
    explicit OwnStr(char* s, bool nullIsEmptyStr) : unique_ptr<char[]>((s || !nullIsEmptyStr) ? s : new char[1]{ 0 }) {}
    char& operator[](size_t n) { return get()[n]; }
};

struct OwnString : public std::string
{
    explicit OwnString(const char* s) : std::string(s ? s : "") { delete[] s; }
};

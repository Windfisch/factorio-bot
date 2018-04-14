//#include "task.hpp"

#include <boost/heap/binomial_heap.hpp>
#include <memory>
#include <cassert>
#include <iostream>



/* problem: we need multiset::extract(). This is standardized with C++17, however
 * lots of systems still ship an old STL implementation which does not implement
 * this function (even if the compiler supports C++17).
 *
 * boost::container::multiset does contain extract(), and serves as a drop-in
 * replacement.
 *
 * However, I want to minimize dependencies on external libraries (especially when
 * the STL *should* have what i need), so this is currently configurable via the
 * preprocessor. This #defines and the boost dependency should vanish as soon as
 * the standard STL container works on all systems.
 */

#define STL_HAS_NO_EXTRACT

#ifdef STL_HAS_NO_EXTRACT
#include <boost/container/set.hpp>
#define MULTISET boost::container::multiset
#else
#include <set>
#define MULTISET std::set
#endif

namespace __PriorityQueue
{
	template <typename T> struct Comparator
	{
		bool operator()(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) const
		{
			return a->priority() < b->priority();
		}
	};

	template <typename T> using multiset_t = MULTISET< std::unique_ptr<T>, __PriorityQueue::Comparator<T> >;
}

template <typename T> struct PriorityQueue : public __PriorityQueue::multiset_t<T> {
	using multiset_t = __PriorityQueue::multiset_t<T>;

	typename multiset_t::iterator insert( typename multiset_t::value_type&& value )
	{
		assert(value->owner == nullptr);
		value->owner = this;
		auto iter = multiset_t::insert(std::move(value));
		(*iter)->my_iterator = iter;
		return iter;
	}

	typename std::unique_ptr<T> extract( typename multiset_t::const_iterator iter )
	{
		auto handle = multiset_t::extract(iter);
		handle.value()->owner = nullptr;
		return std::move(handle.value());
	}
	
	void emplace() = delete;
	void emplace_hint() = delete;
	void swap() = delete;
	void merge() = delete;
};



template <typename T> struct PriorityQueueItem
{
	PriorityQueueItem(float p = 0.0) : _priority(p) {}

	float priority() const { return _priority; }
	void set_priority(float p)
	{
		// if this object is contained in a priority queue, changing our priority
		// automatically changes our position in the priority queue
		if (owner)
		{
			auto tmp_owner = owner;
			auto tmp = owner->extract(my_iterator); // this will clear owner
			_priority = p;
			tmp_owner->insert(move(tmp)); // this sets the owner and my_iterator
		}
		else
		{
			_priority = p;
		}
	}

	float _priority;
	PriorityQueue<T>* owner; // can be nullptr
	typename __PriorityQueue::multiset_t<T>::iterator my_iterator; // only valid if owner != nullptr
};

struct Stuff : public PriorityQueueItem<Stuff> {
	const char* name;
	Stuff(const char* n, float p) : PriorityQueueItem(p), name(n) {}
};

int main()
{
	using namespace std;
	PriorityQueue< Stuff > q;

	q.insert( make_unique<Stuff>("alice", 4) );
	q.insert( make_unique<Stuff>("bob", 5) );
	q.insert( make_unique<Stuff>("charly", 3) );
	q.insert( make_unique<Stuff>("denise", 1) );
	q.insert( make_unique<Stuff>("eve", 2) );

	for (const auto& x : q) cout << x->name << ":" << x->priority() << " (" << x->owner << "), ";
	cout << endl;

	auto hass = q.begin();
	hass++; hass++;
	(*hass) -> set_priority(100);

	for (const auto& x : q) cout << x->name << ":" << x->priority() << ", ";
	cout << endl;

	(*q.begin())->set_priority(0);

	for (const auto& x : q) cout << x->name << ":" << x->priority() << ", ";
	cout << endl;
}

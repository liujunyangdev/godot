#ifndef LRU_H
#define LRU_H

#include "core/math/math_funcs.h"
#include "core/hash_map.h"
#include "core/list.h"

template <class TKey, class TData, class Hasher = HashMapHasherDefault, class Comparator = HashMapComparatorDefault<TKey>>
class LRUCache {
private:
	struct Pair {
		TKey key;
		TData data;

		Pair() {}
		Pair(const TKey &p_key, const TData &p_data) :
				key(p_key),
				data(p_data) {
		}
	};

	typedef typename List<Pair>::Element *Element;

	HashMap<TKey, Element, Hasher, Comparator> _map;
	size_t capacity;

public:
	List<Pair> list;

	const TData *insert(const TKey &p_key, const TData &p_value) {
		Element *e = _map.getptr(p_key);
		Element n = list.push_front(Pair(p_key, p_value));

		if (e) {
			list.erase(*e);
			_map.erase(p_key);
		}
		_map[p_key] = list.front();

		while (_map.size() > capacity) {
			Element d = list.back();
			_map.erase(d->get().key);
			list.pop_back();
		}

		return &n->get().data;
	}

	void clear() {
		_map.clear();
		list.clear();
	}

	bool has(const TKey &p_key) const {
		return _map.getptr(p_key);
	}

	const TData &get(const TKey &p_key) {
		Element *e = _map.getptr(p_key);
		CRASH_COND(!e);
		list.move_to_front(*e);
		return (*e)->get().data;
	};

	const TData *getptr(const TKey &p_key) {
		Element *e = _map.getptr(p_key);
		if (!e) {
			return nullptr;
		} else {
			list.move_to_front(*e);
			return &(*e)->get().data;
		}
	}

	_FORCE_INLINE_ size_t get_capacity() const { return capacity; }
	_FORCE_INLINE_ size_t get_size() const { return _map.size(); }

	void set_capacity(size_t p_capacity) {
		if (capacity > 0) {
			capacity = p_capacity;
			while (_map.size() > capacity) {
				Element d = list.back();
				_map.erase(d->get().key);
				list.pop_back();
			}
		}
	}

	LRUCache() {
		capacity = 64;
	}

	LRUCache(int p_capacity) {
		capacity = p_capacity;
	}
};

#endif // LRU_H
/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

#ifndef LRU_VALUE_CACHE_H
#define LRU_VALUE_CACHE_H

#include <list>
#include <map>

/** LRU Cache with explicit tracking of recent key and value.
 * Implementation is mostly derived from boost::lru_cache, but all
 * post-insertion copying is eliminated. This implementation is not good for
 * storing C-style pointers (as null-pointer is indistinguishable from
 * cache miss)
 */
template<class K, class V>
class LruValueCache
{
	private:
		//////////////////////////////////////////////////
		// LruValueCache. Private types
		//////////////////////////////////////////////////
		typedef std::list<K> ListType;
		typedef std::map<K, std::pair<V, typename ListType::iterator>> MapType;

	public:
		//////////////////////////////////////////////////
		// LruValueCache. Public instance methods
		//////////////////////////////////////////////////

		/** Constructor.
		 * @param capacity Maximum number of elements in cache
		 */
		LruValueCache(size_t capacity) :
			_capacity(capacity),
			_recentKey(nullptr),
			_recentValue(nullptr),
			_hits(0),
			_misses(0),
			_evictions(0)
		{
		}

		/** Add key/value to cache.
		 * @param key Key being added. If already there - value is replaced
		 * @param value Value being added
		 * @return Address of value in cache
		 */
		V *add(const K &key, const V &value)
		{
			typename MapType::iterator i = _map.find(key);
			if (i == _map.end()) {
				// Key not in cache
				if (_map.size() >= _capacity) {
					// Evicting most recent
					typename ListType::iterator oldestKeyIter = --_list.end();
					_map.erase(*oldestKeyIter);
					_list.erase(oldestKeyIter);
					++_evictions;
				}
				_list.push_front(key);
				i = _map.emplace(std::piecewise_construct,
						 std::forward_as_tuple(key),
						 std::forward_as_tuple(value, _list.begin()))
					    .first;
			} else {
				// Key in cache
				_list.splice(_list.begin(), _list, i->second.second);
				i->second.first = value;
			}
			_recentKey = &(_list.front());
			return _recentValue = &(i->second.first);
		}

		/** Cache lookup
		 * @param key Key to look up for
		 * @return Address of found value, nullptr if not found
		 */
		V *get(const K &key)
		{
			typename MapType::iterator i = _map.find(key);
			if (i == _map.end()) {
				++_misses;
				return nullptr;
			}
			++_hits;
			_list.splice(_list.begin(), _list, i->second.second);
			_recentKey = &(_list.front());
			return _recentValue = &(i->second.first);
		}

		/** Clear cache */
		void clear()
		{
			_map.clear();
			_list.clear();
			_recentKey = nullptr;
			_recentValue = nullptr;
		}

		/** Get recently accessed key (nullptr if there were no accesses) */
		const K *recentKey() const
		{
			return _recentKey;
		}

		/** Get recently accessed value (nullptr if there were no accesses).
		 * Const version
		 */
		const V *recentValue() const
		{
			return _recentValue;
		}

		/** Get recently accessed value (nullptr if there were no accesses).
		 * Nonconst version
		 */
		V *recentValue()
		{
			return _recentValue;
		}

		/** Number of evictions */
		int ecictions() const
		{
			return _evictions;
		}

		/** Number of search hits */
		int hits() const
		{
			return _hits;
		}

		/** Number of search misses */
		int misses() const
		{
			return _misses;
		}

	private:
		//////////////////////////////////////////////////
		// LruValueCache. Private instance data
		//////////////////////////////////////////////////

		size_t _capacity; /*!< Maximum number of elements in cache */
		ListType _list; /*!< List of keys - most recent first */
		MapType _map; /*!< Map of values and pointers to key list */
		K *_recentKey; /*!< Recent key value */
		V *_recentValue; /*!< Pointer to recent value or nullptr */
		int _hits; /*!< Number of cache hits */
		int _misses; /*!< Number of cache misses */
		int _evictions; /*!< Number of cache evictions */
};

#endif /* LRU_VALUE_CACHE_H */

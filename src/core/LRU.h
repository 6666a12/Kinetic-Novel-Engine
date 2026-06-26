#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <mutex>

template <typename K, typename V>
class ThreadSafeLRU
{
public:
    explicit ThreadSafeLRU(int capacity): _caps(capacity) {}

    std::optional<V> get(const &K key)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _map.find(key);
        if(it == _map.end()) return std::nullopt;
        move2Front(it);
        return it->second.first;
    }

    void put(const K& key, const V& value)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        auto it = _map.find(key);
        if(it != _map.end())
        {
            it->second.first = value;
            move2Front(it);
            return;
        }

        if(_map.size() >= _caps)
        {
            K oldkey = _list.back();
            _list.pop_back();
            _map.erase(oldkey);
        }

        _list.push_front(key);
        _map[key] = {value, _list.begin()};
    }

    void remove(){
        _list.clear();
        _map.clear();
    }

protected:
    using listIter = typename std::list<K>::iterator;

    mutable std::mutex _mutex;

    int _caps;
    std::list<K> _list;
    std::unordered_map<K, std::pair<V, listIter>> _map;

    void move2Front(typename std::unordered_map<K, std::pair<V, listIter>>::iterator it)
    {
        _list.erase(it->second.second);
        _list.push_front(it->first);
        it->second.second = _list.begin();
    }
};

/*
    EXAMPLES OF USING LRU

    ThreadSafeLRU<int, int> LRUexample(16);
    LRUexample.put(1, 12);
    LRUexample.put(2, 200);
    LRUexample.get(2); ==> 200
    LRUexample.get(3); ==> nullopt
*/

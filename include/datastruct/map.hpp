// Implementation of a hash map
#pragma once
#include <datastruct/list.hpp>
#include <util/subrange.hpp>

// Compute a hash of a value of type T. This function is used by the Map class
// to hash its keys. This function is meant to be specialized for any type that
// might be used as a key.
template<typename T>
u64 hash(T const& value) = delete;

// Define hash<T> specializations for the most common key types.
template<>
inline u64 hash<u8>(u8 const& value)   { return static_cast<u64>(value); }
template<>
inline u64 hash<u16>(u16 const& value) { return static_cast<u64>(value); }
template<>
inline u64 hash<u32>(u32 const& value) { return static_cast<u64>(value); }
template<>
inline u64 hash<u64>(u64 const& value) { return value; }
template<>
inline u64 hash<i8>(i8 const& value)   { return static_cast<u64>(value); }
template<>
inline u64 hash<i16>(i16 const& value) { return static_cast<u64>(value); }
template<>
inline u64 hash<i32>(i32 const& value) { return static_cast<u64>(value); }
template<>
inline u64 hash<i64>(i64 const& value) { return static_cast<u64>(value); }

// Implementation of a hash map with O(1) average complexity for insertion,
// deletion and lookup.
template<typename K, typename V>
class Map {
public:
    // Create an empty Map. This constructor does not perform any allocation, ie
    // the first allocation will be performed upon the first insertion. This is
    // so that is it possible to have a global Map variable, as heap allocation
    // is not available when calling global constructors.
    Map() : m_buckets(nullptr), m_numBuckets(0), m_size(0),
            m_allowRehash(true) {}

    // Create an empty Map for which the buckets are pre-allocated.
    // @param numBuckets: The number of buckets to allocate.
    // @param allowRehash: if true the map may rehash during insertion,
    // otherwise it never rehashes. Setting this to false is only intended to be
    // used during testing as it has a significant impact on performance.
    Map(u64 const numBuckets, bool const allowRehash = true) {
        ASSERT(!!numBuckets);
        m_buckets = new Bucket[numBuckets];
        if (!m_buckets) {
            ASSERT(!"Cannot allocate buckets for map");
        }

        m_numBuckets = numBuckets;
        m_size = 0;
        m_allowRehash = allowRehash;
    }

    // Construct a copy of an existing Map<T>. The new Map contains the same set
    // of keys associated to the same values. The values are copied by
    // copy-construction.
    // @param other: The map to copy.
    Map(Map const& other) : Map(other.m_numBuckets, true) {
        // The m_buckets is pre-allocated to have the same size as other's. We
        // simply have to copy each bucket into the new Map.
        for (u64 i(0); i < m_numBuckets; ++i) {
            m_buckets[i] = other.m_buckets[i];
        }
        m_size = other.m_size;
    }

    // Remove all elements from the map. This call the destructor on all keys
    // and values contained in the map.
    ~Map() {
        if (!!m_buckets) {
            delete[] m_buckets;
        }
    }

    // We don't have a usecase for these functions right now. Keep the
    // implementation and testing simple and mark them deleted.
    Map(Map && other) = delete;
    Map& operator=(Map && other) = delete;

    // Copy the content of a map into this map. The map is emptied and all (key,
    // value) pairs are copied.
    // @param other: The Map to copy.
    Map& operator=(Map const& other) {
        if (!!m_buckets) {
            delete[] m_buckets;
        }
        m_buckets = new Bucket[other.m_numBuckets];
        if (!m_buckets) {
            ASSERT(!"Cannot allocate buckets for map");
        }
        m_numBuckets = other.m_numBuckets;
        for (u64 i(0); i < m_numBuckets; ++i) {
            m_buckets[i] = other.m_buckets[i];
        }
        m_size = other.m_size;
        return *this;
    }

    // Compare two Maps. Two maps are considered equal if they both contain the
    // same set of keys, and for each key they associate the same value.
    // @param other: The map to compare against.
    // @return: true if this map is equal to other, false otherwise.
    bool operator==(Map const& other) const {
        if (m_size != other.m_size) {
            return false;
        }
        // Because both maps may have different numbers of buckets and the state
        // of each bucket depends on the insertion order, we cannot naively
        // compare the buckets. Instead we need to manually check that each key
        // of this map is contained in the other map and both are pointing to
        // the same value. This is sufficient because at this point both maps
        // have the same size / number of elements.
        for (u64 i(0); i < m_numBuckets; ++i) {
            Bucket const& bucket(m_buckets[i]);
            for (Entry const& entry: bucket) {
                if (!other.contains(entry.key)
                    || entry.value != other[entry.key]) {
                    return false;

                }
            }
        }
        return true;
    }

    // Get the size of the map, ie the number of elements inserted.
    u64 size() const {
        return m_size;
    }

    // Check if the map is empty.
    bool empty() const {
        return !size();
    }

    // Access the value with the given key. If the map does not contain the key,
    // the value is default-constructed.
    // @param key: The key to look up.
    // @return: A reference to the value associated with the key.
    V& operator[](K const& key) {
        maybeRehash();
        Bucket& bucket(bucketForKey(key));
        for (Entry& entry: bucket) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        // The key is not present in the map, insert it.
        // FIXME: This creates a lot of unecessary copies. We need an
        // emplace like functions in List<T>.
        bucket.pushFront(Entry(key, V()));
        ++m_size;
        return bucket.front().value;
    }

    // Access the value with the given key. Asserts that the key is contained in
    // the map.
    // @param key: The key to look up.
    // @return: A const-reference to the value associated with the key.
    V const& operator[](K const& key) const {
        if (size()) {
            Bucket const& bucket(bucketForKey(key));
            for (Entry const& entry: bucket) {
                if (entry.key == key) {
                    return entry.value;
                }
            }
        }
        ASSERT(!"Key not present in map");
        __builtin_unreachable();
    }

    // Check if the map contains a given key.
    // @param key: The key to lookup.
    // @return: true if the key is in the map, false otherwise.
    bool contains(K const& key) const {
        if (size()) {
            Bucket const& bucket(bucketForKey(key));
            for (Entry const& entry: bucket) {
                if (entry.key == key) {
                    return true;
                }
            }
        }
        return false;
    }

    // Remove a key and its associated value from the map. This operation is a
    // no-op if the key is not present in the map, this is a no-op.
    // @param key: The key to remove.
    void erase(K const& key) {
        if (size()) {
            Bucket& bucket(bucketForKey(key));
            typename Bucket::Iter it(bucket.begin());
            typename Bucket::Iter const end(bucket.end());
            while (it != end && (*it).key != key) {
                ++it;
            }
            if (it != end) {
                it.erase();
                --m_size;
            }
        }
    }

    // Remove all elements from the map.
    void clear() {
        for (u64 i(0); i < m_numBuckets; ++i) {
            m_buckets[i].clear();
        }
        m_size = 0;
    }

    // Get the number of buckets used by this map. This includes the buckets
    // which are not holding any key/values. Only useful for testing.
    u64 numBuckets() const {
        return m_numBuckets;
    }

private:
    // An entry of the hash map.
    struct Entry {
        K key;
        V value;
    };

    using Bucket = List<Entry>;

    // Get a reference to the bucket that is holding or would hold a key.
    // @param key: The key.
    // @return: A reference to the bucket associated with the key.
    Bucket& bucketForKey(K const& key) {
        ASSERT(!!m_buckets&&!!m_numBuckets);
        u64 const h(hash(key));
        return m_buckets[h % m_numBuckets];
    }
    Bucket const& bucketForKey(K const& key) const {
        ASSERT(!!m_buckets&&!!m_numBuckets);
        u64 const h(hash(key));
        return m_buckets[h % m_numBuckets];
    }

    // Grows the m_bucket array and re-hash all values into the new array.
    void rehash() {
        // FIXME: Re-hashing is extremely inefficient due to having to copy
        // elements from one List to another. It would be better if we could
        // somehow transfer a List<T>::Node from one list to another without a
        // copy.
        Bucket* const oldBuckets(m_buckets);
        u64 const oldNumBuckets(m_numBuckets);
        m_numBuckets = m_numBuckets ? 2 * m_numBuckets : MinNumBuckets;
        m_buckets = new Bucket[m_numBuckets];

        if (!m_buckets) {
            ASSERT(!"Cannot allocate buckets for map");
        }

        for (u64 i(0); i < oldNumBuckets; ++i) {
            Bucket const& bucket(oldBuckets[i]);
            for (Entry const& e : bucket) {
                bucketForKey(e.key).pushFront(e);
            }
        }

        if (!!oldBuckets) {
            delete[] oldBuckets;
        }
    }

    // Check if the map needs a re-hash and if so rehash the map. The map is
    // rehash when the load factor reaches 1.
    void maybeRehash() {
        if (m_allowRehash && m_numBuckets == m_size) {
            rehash();
        }
    }

    // The smallest size for the m_buckets array. Note that a default
    // constructed Map<T> can still have 0 buckets since default construction
    // does not allocate memory.
    static const u64 MinNumBuckets = 8;

    Bucket* m_buckets;
    u64 m_numBuckets;

    // The number of elements inserted the map.
    u64 m_size;

    // Used for testing. If true the map may rehash during an insertion,
    // otherwise the map never rehashes.
    bool m_allowRehash;
};

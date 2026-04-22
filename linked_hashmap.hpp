/**
 * implement a container like std::linked_hashmap
 */
#ifndef SJTU_LINKEDHASHMAP_HPP
#define SJTU_LINKEDHASHMAP_HPP

// only for std::equal_to<T> and std::hash<T>
#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {
    /**
     * In linked_hashmap, iteration ordering is differ from map,
     * which is the order in which keys were inserted into the map.
     * You should maintain a doubly-linked list running through all
     * of its entries to keep the correct iteration order.
     *
     * Note that insertion order is not affected if a key is re-inserted
     * into the map.
     */

template<
	class Key,
	class T,
	class Hash = std::hash<Key>,
	class Equal = std::equal_to<Key>
> class linked_hashmap {
public:
	/**
	 * the internal type of data.
	 * it should have a default constructor, a copy constructor.
	 * You can use sjtu::linked_hashmap as value_type by typedef.
	 */
	typedef pair<const Key, T> value_type;

private:
    // Node structure for both hash table chain and doubly-linked list
    struct Node {
        value_type data;
        Node* next_in_bucket;  // Next node in the same hash bucket
        Node* prev_in_bucket;  // Previous node in the same hash bucket
        Node* next_in_list;    // Next node in insertion order
        Node* prev_in_list;    // Previous node in insertion order

        Node(const value_type& val) : data(val), next_in_bucket(nullptr),
                                      prev_in_bucket(nullptr), next_in_list(nullptr),
                                      prev_in_list(nullptr) {}

        Node(value_type&& val) : data(std::move(val)), next_in_bucket(nullptr),
                                 prev_in_bucket(nullptr), next_in_list(nullptr),
                                 prev_in_list(nullptr) {}
    };

    // Hash table parameters
    static const size_t INITIAL_CAPACITY = 16;
    static constexpr double LOAD_FACTOR = 0.75;

    Node** buckets;           // Array of bucket heads
    size_t bucket_count;      // Current capacity
    size_t element_count;     // Number of elements

    // Doubly-linked list for insertion order
    Node* head;
    Node* tail;

    // Hash and equality functors
    Hash hasher;
    Equal key_equal;

    // Helper functions
    size_t hash_index(const Key& key) const {
        return bucket_count == 0 ? 0 : hasher(key) % bucket_count;
    }

    void rehash(size_t new_capacity) {
        // Create new buckets
        Node** new_buckets = new Node*[new_capacity]();

        // Rehash all elements
        Node* current = head;
        while (current) {
            size_t new_index = hasher(current->data.first) % new_capacity;

            // Insert at beginning of new bucket
            current->next_in_bucket = new_buckets[new_index];
            current->prev_in_bucket = nullptr;
            if (new_buckets[new_index]) {
                new_buckets[new_index]->prev_in_bucket = current;
            }
            new_buckets[new_index] = current;

            current = current->next_in_list;
        }

        // Delete old buckets and update
        delete[] buckets;
        buckets = new_buckets;
        bucket_count = new_capacity;
    }

    void ensure_capacity() {
        if (element_count >= bucket_count * LOAD_FACTOR) {
            rehash(bucket_count * 2);
        }
    }

    Node* find_node(const Key& key) const {
        if (bucket_count == 0) return nullptr;

        size_t index = hash_index(key);
        Node* current = buckets[index];

        while (current) {
            if (key_equal(current->data.first, key)) {
                return current;
            }
            current = current->next_in_bucket;
        }

        return nullptr;
    }

    void insert_node(Node* node) {
        // Add to hash table
        size_t index = hash_index(node->data.first);
        node->next_in_bucket = buckets[index];
        node->prev_in_bucket = nullptr;
        if (buckets[index]) {
            buckets[index]->prev_in_bucket = node;
        }
        buckets[index] = node;

        // Add to end of linked list
        node->next_in_list = nullptr;
        node->prev_in_list = tail;

        if (tail) {
            tail->next_in_list = node;
        } else {
            head = node;  // First element
        }
        tail = node;

        element_count++;
    }

    void remove_node(Node* node) {
        // Remove from hash table chain
        if (node->prev_in_bucket) {
            node->prev_in_bucket->next_in_bucket = node->next_in_bucket;
        } else {
            // Node is head of bucket
            size_t index = hash_index(node->data.first);
            buckets[index] = node->next_in_bucket;
        }

        if (node->next_in_bucket) {
            node->next_in_bucket->prev_in_bucket = node->prev_in_bucket;
        }

        // Remove from linked list
        if (node->prev_in_list) {
            node->prev_in_list->next_in_list = node->next_in_list;
        } else {
            head = node->next_in_list;
        }

        if (node->next_in_list) {
            node->next_in_list->prev_in_list = node->prev_in_list;
        } else {
            tail = node->prev_in_list;
        }

        element_count--;
        delete node;
    }

    void clear_all() {
        Node* current = head;
        while (current) {
            Node* next = current->next_in_list;
            delete current;
            current = next;
        }

        delete[] buckets;
        buckets = nullptr;
        bucket_count = 0;
        element_count = 0;
        head = tail = nullptr;
    }

    void copy_from(const linked_hashmap& other) {
        bucket_count = other.bucket_count;
        element_count = 0;
        buckets = new Node*[bucket_count]();
        head = tail = nullptr;

        // Copy elements in insertion order
        Node* current = other.head;
        while (current) {
            Node* new_node = new Node(current->data);
            insert_node(new_node);
            current = current->next_in_list;
        }
    }

public:
	/**
	 * see BidirectionalIterator at CppReference for help.
	 *
	 * if there is anything wrong throw invalid_iterator.
	 *     like it = linked_hashmap.begin(); --it;
	 *       or it = linked_hashmap.end(); ++end();
	 */
	class const_iterator;
	class iterator {
	private:
		// Pointer to node in the doubly-linked list
		Node* node_ptr;
		const linked_hashmap* container;

	public:
		// The following code is written for the C++ type_traits library.
		// Type traits is a C++ feature for describing certain properties of a type.
		// For instance, for an iterator, iterator::value_type is the type that the
		// iterator points to.
		// STL algorithms and containers may use these type_traits (e.g. the following
		// typedef) to work properly.
		// See these websites for more information:
		// https://en.cppreference.com/w/cpp/header/type_traits
		// About value_type: https://blog.csdn.net/u014299153/article/details/72419713
		// About iterator_category: https://en.cppreference.com/w/cpp/iterator
		using difference_type = std::ptrdiff_t;
		using value_type = typename linked_hashmap::value_type;
		using pointer = value_type*;
		using reference = value_type&;
		using iterator_category = std::output_iterator_tag;


		iterator() : node_ptr(nullptr), container(nullptr) {}

		iterator(Node* node, const linked_hashmap* cont) : node_ptr(node), container(cont) {}

		iterator(const iterator &other) : node_ptr(other.node_ptr), container(other.container) {}

		/**
		 * TODO iter++
		 */
		iterator operator++(int) {
			iterator temp = *this;
			if (node_ptr) {
				node_ptr = node_ptr->next_in_list;
			} else {
				throw invalid_iterator{};
			}
			return temp;
		}

		/**
		 * TODO ++iter
		 */
		iterator & operator++() {
			if (node_ptr) {
				node_ptr = node_ptr->next_in_list;
			} else {
				throw invalid_iterator{};
			}
			return *this;
		}

		/**
		 * TODO iter--
		 */
		iterator operator--(int) {
			iterator temp = *this;
			if (node_ptr) {
				node_ptr = node_ptr->prev_in_list;
			} else if (container && container->tail) {
				// end() iterator, move to last element
				node_ptr = container->tail;
			} else {
				throw invalid_iterator{};
			}
			return temp;
		}

		/**
		 * TODO --iter
		 */
		iterator & operator--() {
			if (node_ptr) {
				node_ptr = node_ptr->prev_in_list;
			} else if (container && container->tail) {
				// end() iterator, move to last element
				node_ptr = container->tail;
			} else {
				throw invalid_iterator{};
			}
			return *this;
		}

		/**
		 * a operator to check whether two iterators are same (pointing to the same memory).
		 */
		value_type & operator*() const {
			if (!node_ptr) {
				throw invalid_iterator{};
			}
			return node_ptr->data;
		}

		bool operator==(const iterator &rhs) const {
			return node_ptr == rhs.node_ptr && container == rhs.container;
		}

		bool operator==(const const_iterator &rhs) const;

		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const {
			return !(*this == rhs);
		}

		bool operator!=(const const_iterator &rhs) const;

		/**
		 * for the support of it->first.
		 * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
		 */
		value_type* operator->() const {
			if (!node_ptr) {
				throw invalid_iterator{};
			}
			return &(node_ptr->data);
		}

		friend class linked_hashmap;
		friend class const_iterator;
	};

	class const_iterator {
	private:
		const Node* node_ptr;
		const linked_hashmap* container;

	public:
		const_iterator() : node_ptr(nullptr), container(nullptr) {}

		const_iterator(const Node* node, const linked_hashmap* cont) : node_ptr(node), container(cont) {}

		const_iterator(const const_iterator &other) : node_ptr(other.node_ptr), container(other.container) {}

		const_iterator(const iterator &other) : node_ptr(other.node_ptr), container(other.container) {}

		// And other methods in iterator.
		const_iterator operator++(int) {
			const_iterator temp = *this;
			if (node_ptr) {
				node_ptr = node_ptr->next_in_list;
			} else {
				throw invalid_iterator{};
			}
			return temp;
		}

		const_iterator & operator++() {
			if (node_ptr) {
				node_ptr = node_ptr->next_in_list;
			} else {
				throw invalid_iterator{};
			}
			return *this;
		}

		const_iterator operator--(int) {
			const_iterator temp = *this;
			if (node_ptr) {
				node_ptr = node_ptr->prev_in_list;
			} else if (container && container->tail) {
				node_ptr = container->tail;
			} else {
				throw invalid_iterator{};
			}
			return temp;
		}

		const_iterator & operator--() {
			if (node_ptr) {
				node_ptr = node_ptr->prev_in_list;
			} else if (container && container->tail) {
				node_ptr = container->tail;
			} else {
				throw invalid_iterator{};
			}
			return *this;
		}

		const value_type & operator*() const {
			if (!node_ptr) {
				throw invalid_iterator{};
			}
			return node_ptr->data;
		}

		bool operator==(const iterator &rhs) const {
			return node_ptr == rhs.node_ptr && container == rhs.container;
		}

		bool operator==(const const_iterator &rhs) const {
			return node_ptr == rhs.node_ptr && container == rhs.container;
		}

		bool operator!=(const iterator &rhs) const {
			return !(*this == rhs);
		}

		bool operator!=(const const_iterator &rhs) const {
			return !(*this == rhs);
		}

		const value_type* operator->() const {
			if (!node_ptr) {
				throw invalid_iterator{};
			}
			return &(node_ptr->data);
		}

		friend class linked_hashmap;
		friend class iterator;
	};

	/**
	 * TODO two constructors
	 */
	linked_hashmap() : bucket_count(INITIAL_CAPACITY), element_count(0),
	                   head(nullptr), tail(nullptr) {
		buckets = new Node*[bucket_count]();
	}

	linked_hashmap(const linked_hashmap &other) {
		copy_from(other);
	}

	/**
	 * TODO assignment operator
	 */
	linked_hashmap & operator=(const linked_hashmap &other) {
		if (this != &other) {
			clear_all();
			copy_from(other);
		}
		return *this;
	}

	/**
	 * TODO Destructors
	 */
	~linked_hashmap() {
		clear_all();
	}

	/**
	 * TODO
	 * access specified element with bounds checking
	 * Returns a reference to the mapped value of the element with key equivalent to key.
	 * If no such element exists, an exception of type `index_out_of_bound'
	 */
	T & at(const Key &key) {
		Node* node = find_node(key);
		if (!node) {
			throw index_out_of_bound{};
		}
		return node->data.second;
	}

	const T & at(const Key &key) const {
		const Node* node = find_node(key);
		if (!node) {
			throw index_out_of_bound{};
		}
		return node->data.second;
	}

	/**
	 * TODO
	 * access specified element
	 * Returns a reference to the value that is mapped to a key equivalent to key,
	 *   performing an insertion if such key does not already exist.
	 */
	T & operator[](const Key &key) {
		Node* node = find_node(key);
		if (node) {
			return node->data.second;
		}

		// Insert new element with default value
		ensure_capacity();
		Node* new_node = new Node(value_type(key, T()));
		insert_node(new_node);
		return new_node->data.second;
	}

	/**
	 * behave like at() throw index_out_of_bound if such key does not exist.
	 */
	const T & operator[](const Key &key) const {
		return at(key);
	}

	/**
	 * return a iterator to the beginning
	 */
	iterator begin() {
		return iterator(head, this);
	}

	const_iterator cbegin() const {
		return const_iterator(head, this);
	}

	/**
	 * return a iterator to the end
	 * in fact, it returns past-the-end.
	 */
	iterator end() {
		return iterator(nullptr, this);
	}

	const_iterator cend() const {
		return const_iterator(nullptr, this);
	}

	/**
	 * checks whether the container is empty
	 * return true if empty, otherwise false.
	 */
	bool empty() const {
		return element_count == 0;
	}

	/**
	 * returns the number of elements.
	 */
	size_t size() const {
		return element_count;
	}

	/**
	 * clears the contents
	 */
	void clear() {
		clear_all();
		bucket_count = INITIAL_CAPACITY;
		buckets = new Node*[bucket_count]();
	}

	/**
	 * insert an element.
	 * return a pair, the first of the pair is
	 *   the iterator to the new element (or the element that prevented the insertion),
	 *   the second one is true if insert successfully, or false.
	 */
	pair<iterator, bool> insert(const value_type &value) {
		Node* existing = find_node(value.first);
		if (existing) {
			return pair<iterator, bool>(iterator(existing, this), false);
		}

		ensure_capacity();
		Node* new_node = new Node(value);
		insert_node(new_node);
		return pair<iterator, bool>(iterator(new_node, this), true);
	}

	/**
	 * erase the element at pos.
	 *
	 * throw if pos pointed to a bad element (pos == this->end() || pos points an element out of this)
	 */
	void erase(iterator pos) {
		if (pos.container != this || !pos.node_ptr) {
			throw invalid_iterator{};
		}
		remove_node(pos.node_ptr);
	}

	/**
	 * Returns the number of elements with key
	 *   that compares equivalent to the specified argument,
	 *   which is either 1 or 0
	 *     since this container does not allow duplicates.
	 */
	size_t count(const Key &key) const {
		return find_node(key) ? 1 : 0;
	}

	/**
	 * Finds an element with key equivalent to key.
	 * key value of the element to search for.
	 * Iterator to an element with key equivalent to key.
	 *   If no such element is found, past-the-end (see end()) iterator is returned.
	 */
	iterator find(const Key &key) {
		Node* node = find_node(key);
		return iterator(node, this);
	}

	const_iterator find(const Key &key) const {
		const Node* node = find_node(key);
		return const_iterator(node, this);
	}
};

// Define iterator comparison operators with const_iterator
template<class Key, class T, class Hash, class Equal>
bool linked_hashmap<Key, T, Hash, Equal>::iterator::operator==(const const_iterator &rhs) const {
    return node_ptr == rhs.node_ptr && container == rhs.container;
}

template<class Key, class T, class Hash, class Equal>
bool linked_hashmap<Key, T, Hash, Equal>::iterator::operator!=(const const_iterator &rhs) const {
    return !(*this == rhs);
}

}

#endif
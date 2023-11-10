#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace nlrs
{
// An STL-like set, implemented with a sorted std::vector.
template<typename Key, typename Compare = std::less<Key>>
class VectorSet
{
public:
    using const_iterator = typename std::vector<Key>::const_iterator;
    using size_type = typename std::vector<Key>::size_type;
    using value_type = typename std::vector<Key>::value_type;
    using const_pointer = const value_type*;
    using const_reference = const value_type&;

    constexpr VectorSet() noexcept = default;
    constexpr ~VectorSet() noexcept = default;

    constexpr VectorSet(const VectorSet&) = default;
    constexpr VectorSet(VectorSet&&) noexcept = default;

    constexpr VectorSet& operator=(const VectorSet&) = default;
    constexpr VectorSet& operator=(VectorSet&&) noexcept = default;

    constexpr explicit VectorSet(Compare comp) noexcept
        : m_data(),
          m_compare(std::move(comp))
    {
    }

    template<std::size_t N>
    constexpr VectorSet(const Key (&data)[N], Compare comp = Compare()) noexcept
        : m_data(data, data + N),
          m_compare(std::move(comp))
    {
    }

    template<typename InputIt>
    constexpr VectorSet(InputIt first, InputIt last, Compare comp = Compare()) noexcept
        : m_data(first, last),
          m_compare(std::move(comp))
    {
    }

    // Iterators

    [[nodiscard]] constexpr const_iterator begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return m_data.end(); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return m_data.cbegin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return m_data.cend(); }

    // Element access

    [[nodiscard]] constexpr const_pointer data() const noexcept { return m_data.data(); }

    [[nodiscard]] constexpr const_reference operator[](const size_type pos) const
    {
        assert(pos < m_data.size());
        return m_data[pos];
    }

    // Capacity

    [[nodiscard]] constexpr bool      empty() const noexcept { return m_data.empty(); }
    [[nodiscard]] constexpr size_type size() const noexcept { return m_data.size(); }

    // Modifiers

    constexpr void reserve(size_type new_cap) { m_data.reserve(new_cap); }
    constexpr void clear() noexcept { m_data.clear(); }

    constexpr std::pair<const_iterator, bool> insert(const Key& key);
    constexpr std::pair<const_iterator, bool> insert(Key&& key);
    constexpr const_iterator                  erase(const_iterator pos);
    constexpr const_iterator                  erase(const Key& key);

    // Lookup

    [[nodiscard]] constexpr const_iterator find(const Key& key) const noexcept;
    [[nodiscard]] constexpr bool           contains(const Key& key) const noexcept;
    template<typename T>
    [[nodiscard]] constexpr bool contains(T&& key) const noexcept;

    // Internals

    [[nodiscard]] constexpr Compare key_compare() const noexcept { return m_compare; }

private:
    std::vector<Key> m_data;
    // TODO: Apparently MSVC ignores [[no_unique_address]] even in C++20 mode.
    // Instead, msvc::no_unique_address is provided.
    [[no_unique_address]] Compare m_compare;
};

template<typename K, typename C>
constexpr std::pair<typename VectorSet<K, C>::const_iterator, bool> VectorSet<K, C>::insert(
    const K& key)
{
    const auto lower_bound = std::lower_bound(m_data.begin(), m_data.end(), key, m_compare);

    if (lower_bound != m_data.end() && !m_compare(key, *lower_bound))
    {
        return std::make_pair(lower_bound, false);
    }
    return std::make_pair(m_data.insert(lower_bound, key), true);
}

template<typename K, typename C>
constexpr std::pair<typename VectorSet<K, C>::const_iterator, bool> VectorSet<K, C>::insert(K&& key)
{
    const auto lower_bound = std::lower_bound(m_data.begin(), m_data.end(), key, m_compare);

    if (lower_bound != m_data.end() && !m_compare(key, *lower_bound))
    {
        return std::make_pair(lower_bound, false);
    }
    return std::make_pair(m_data.insert(lower_bound, std::move(key)), true);
}

template<typename K, typename C>
constexpr typename VectorSet<K, C>::const_iterator VectorSet<K, C>::erase(const_iterator pos)
{
    assert(std::distance(m_data.cbegin(), pos) >= 0);
    assert(std::distance(pos, m_data.cend()) > 0);
    return m_data.erase(pos);
}

template<typename K, typename C>
constexpr typename VectorSet<K, C>::const_iterator VectorSet<K, C>::erase(const K& key)
{
    const auto it = find(key);
    return it != end() ? erase(it) : end();
}

template<typename K, typename C>
constexpr typename VectorSet<K, C>::const_iterator VectorSet<K, C>::find(
    const K& key) const noexcept
{
    const auto [lower_bound, upper_bound] =
        std::equal_range(m_data.begin(), m_data.end(), key, m_compare);
    return lower_bound != upper_bound ? lower_bound : end();
}

template<typename K, typename C>
constexpr bool VectorSet<K, C>::contains(const K& key) const noexcept
{
    return std::binary_search(m_data.begin(), m_data.end(), key, m_compare);
}

template<typename K, typename C>
template<typename T>
constexpr bool VectorSet<K, C>::contains(T&& key) const noexcept
{
    return std::binary_search(m_data.begin(), m_data.end(), std::forward<T>(key), m_compare);
}
} // namespace nlrs

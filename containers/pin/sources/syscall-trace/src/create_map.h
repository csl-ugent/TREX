#include <map>
#include <utility>

/* HELPERS TO CREATE std::map INSTANCES MORE EASILY */

// Normally, one would use std::initializer_list initalisation to create a
// std::map in one line, but Pin doesn't support C++11, so we'll have to do it
// this way for now...

// Examples: create_map<T, U>(), create_map<T, U>(1, 2), create_map<T, U>(1,
// 2)(2, 3), ...
template <typename T, typename U> class create_map {
public:
  create_map() = default;

  create_map(const T &key, const U &val) { m_map[key] = val; }

  create_map<T, U> &operator()(const T &key, const U &val) {
    m_map[key] = val;
    return *this;
  }

  operator std::map<T, U>() { return m_map; }

private:
  std::map<T, U> m_map;
};

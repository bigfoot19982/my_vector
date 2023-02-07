#include <cstddef>
#include <new>
#include <memory>
#include <utility>

template <typename T>
class MyAllocator {
public:
  T* arr = nullptr;
  size_t cap = 0;

  static T* Allocate(size_t n) {
    return reinterpret_cast<T*>(::operator new(n * sizeof(T)));
  } 

  static void Deallocate(T* ptr) {
    ::operator delete(ptr);
  }

  static void Construct(T* ptr, const T& obj) {
    new (ptr) T(obj);
  }

  static void Construct(T* ptr, T&& obj) {
    new (ptr) T(std::move(obj));
  }

  static void Destroy(T* ptr) {
    ptr->~T();
  }
  
  MyAllocator() = default;
  MyAllocator(size_t n) : arr(this->Allocate(n)), cap(n) {}
  MyAllocator(const MyAllocator&) = delete;
  MyAllocator(MyAllocator&& other) {
    Swap(other);
  }
  MyAllocator& operator=(const MyAllocator&) = delete;
  MyAllocator& operator=(MyAllocator&& other) {
    if (this != other) {
      Swap(other);
    }
    return *this;
  }
  ~MyAllocator() { Deallocate(arr); }

  void Swap(MyAllocator<T>& rhs) {
    std::swap(this->arr, rhs.arr);
    std::swap(this->cap, rhs.cap);
  }
};


template <typename T>
class Vector {
private:
  MyAllocator<T> alloc;
  size_t sz = 0;

  void Swap(Vector& rhs) {
    alloc.Swap(rhs.alloc);
    std::swap(sz, rhs.sz);
  }

public:
  Vector() = default;
  Vector(size_t n) : alloc(n) {
    std::uninitialized_value_construct_n(alloc.arr, n);
    sz = n;
  }
  Vector(const Vector& other) : alloc(other.sz) {
    std::uninitialized_copy_n(other.alloc.arr, other.sz, alloc.arr);
    sz = other.sz;
  }
  
  Vector(Vector&& other) noexcept {
    Swap(other);
  }

  ~Vector() {
    std::destroy_n(alloc.arr, sz);
  }

  Vector& operator=(const Vector& other) {
    if (&other != this) {
      if (other.sz > alloc.cap) {
        Vector(other).Swap(*this);
      } else {
        for (int i = 0; i < std::min(other.sz, sz); ++i) {
          alloc.arr[i] = other.alloc.arr[i];
        }
        if (sz < other.sz) {
          std::uninitialized_copy_n(other.alloc.arr + sz, other.sz - sz, alloc.arr + sz);
        } else if (sz > other.sz) {
          std::destroy_n(alloc.arr + other.sz, sz - other.sz);
        }
        sz = other.sz;
      }
    }
    return *this;
  }

  Vector& operator=(Vector&& other) noexcept {
    if (this != &other) {
      other.Swap(*this);
    }
    return *this;
  }

  void Reserve(size_t n) {
    if (n > alloc.cap) {
      MyAllocator<T> alloc2(n);
      std::uninitialized_move_n(alloc.arr, sz, alloc2.arr);
      std::destroy_n(alloc.arr, sz);
      alloc2.Swap(alloc);
      alloc.cap = n;
    }
  }

  void Resize(size_t n) {
    Reserve(n);
    if (n < sz) {
      std::destroy_n(alloc.arr + n, sz - n);
    } else if (n > sz) {
      std::uninitialized_value_construct_n(alloc.arr + sz, n - sz);
    }
    sz = n;
  }

  void PushBack(const T& elem) {
    if (sz == alloc.cap) {
      Reserve(sz == 0 ? 1 : sz * 2);
    }
    alloc.Construct(alloc.arr + sz, elem);
    sz++;
  }

  void PushBack(T&& elem) {
    if (sz == alloc.cap) {
      Reserve(sz == 0 ? 1 : sz * 2);
    }
    alloc.Construct(alloc.arr + sz, std::move(elem));
    sz++;
  }

  template <typename ... Args>
  T& EmplaceBack(Args&&... args) {
    Reserve(sz+1);
    auto elem = new (alloc.arr + sz) T(std::forward<Args>(args)...);
    sz++;
    return *elem;
  }

  void PopBack() {
    std::destroy_at(alloc.arr + sz - 1);
    sz--;
  }

  size_t Size() const noexcept { return sz; }

  size_t Capacity() const noexcept { return alloc.cap; }

  const T& operator[](size_t i) const { return alloc.arr[i]; }
  T& operator[](size_t i) { return alloc.arr[i]; }

  


  using iterator = T*;
  using const_iterator = const T*;

  iterator begin() noexcept { return alloc.arr; }
  iterator end() noexcept { return alloc.arr + sz; }

  const_iterator begin() const noexcept { return alloc.arr; }
  const_iterator end() const noexcept { return alloc.arr + sz; }

  const_iterator cbegin() const noexcept { return alloc.arr; }
  const_iterator cend() const noexcept { return alloc.arr + sz; }

  // Вставляет элемент перед pos
  // Возвращает итератор на вставленный элемент
  iterator Insert(const_iterator pos, const T& elem) {
    int ind = pos - alloc.arr;
    slideOneRight(ind);
    alloc.Construct(alloc.arr + ind, elem);
    sz++;
    return alloc.arr + ind;
  }
  iterator Insert(const_iterator pos, T&& elem) {
    int ind = pos - alloc.arr;
    slideOneRight(ind);
    alloc.Construct(alloc.arr + ind, std::move(elem));
    sz++;
    return alloc.arr + ind;
  }

  // Конструирует элемент по заданным аргументам конструктора перед pos
  // Возвращает итератор на вставленный элемент
  template <typename ... Args>
  iterator Emplace(const_iterator pos, Args&&... args) {
    int ind = pos - alloc.arr;
    slideOneRight(ind);
    new (alloc.arr + ind) T(std::forward<Args>(args)...);
    sz++;
    return alloc.arr + ind;
  }

  // Удаляет элемент на позиции pos
  // Возвращает итератор на элемент, следующий за удалённым
  iterator Erase(const_iterator pos) {
    int ind = pos - alloc.arr;
    slideOneLeft(ind);
    return alloc.arr + ind;
  }

  void slideOneRight(int pos) {
    Reserve(sz + 1);
    for (auto it = alloc.arr + sz; it != alloc.arr + pos; --it) {
      std::uninitialized_move_n(std::prev(it), 1, it);
      alloc.Destroy(std::prev(it));
    }
    alloc.Destroy(alloc.arr + pos);
  }

  void slideOneLeft(int pos) {
    for (auto it = alloc.arr + pos; it != alloc.arr + sz-1; ++it) {
      std::swap(*it, *std::next(it));
    }
    PopBack();
  }
};

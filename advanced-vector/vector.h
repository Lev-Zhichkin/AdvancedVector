#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        this->buffer_ = std::move(other.buffer_);
        this->capacity_ = std::move(other.capacity_);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        this->buffer_ = std::move(rhs.buffer_);
        this->capacity_ = std::move(rhs.capacity_);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        :data_(std::move(other.data_)), size_(std::move(other.size_))
    {
        other.data_ = RawMemory<T>();
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.Capacity() > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (this->size_ > rhs.size_) {
                    auto it = std::copy_n(rhs.data_.GetAddress(), rhs.Size(), this->data_.GetAddress());
                    std::destroy_n(it, this->size_ - rhs.Size());
                    size_ = rhs.Size();
                }
                else {
                    auto it = std::copy_n(rhs.data_.GetAddress(), this->size_, this->data_.GetAddress());
                    auto rhs_it = rhs.data_.GetAddress() + this->size_;
                    std::uninitialized_copy_n(rhs_it, rhs.Size() - this->size_, it);
                    size_ = rhs.Size();
                }
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        this->data_ = std::move(rhs.data_);
        this->size_ = rhs.size_;
        rhs.data_ = RawMemory<T>();
        rhs.size_ = 0;
        return *this;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        }

        if (new_size < size_) {
            auto it = data_.GetAddress() + new_size;
            std::destroy_n(it, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            auto it = data_.GetAddress() + size_;
            std::uninitialized_value_construct_n(it, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : (size_ * 2));
            new (new_data + size_) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
            new_data = RawMemory<T>();
            ++size_;
        }
        else {
            new (data_ + size_) T(value);
            ++size_;
        }
    }
    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : (size_ * 2));
            new (new_data + size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
            new_data = RawMemory<T>();
            ++size_;
        }
        else {
            new (data_ + size_) T(std::move(value));
            ++size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : (size_ * 2));
            auto elem = new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
            new_data = RawMemory<T>();
            ++size_;
            return *elem;
        }
        else {
            auto elem = new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return *elem;
        }
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t index = pos - begin();
        if (size_ == index)
        {
            EmplaceBack(std::forward<Args>(args)...);
        }
        else if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : (size_ * 2));
            new (new_data + index) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress());
                std::uninitialized_move_n(data_ + index, size_ - index, new_data + (index + 1));
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), index, new_data.GetAddress());
                std::uninitialized_copy_n(data_ + index, size_ - index, new_data + (index + 1));
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
            new_data = RawMemory<T>();
            ++size_;
        }
        else {
            T elem(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(*(data_ + (size_ - 1))));
            std::move_backward(data_.GetAddress() + index, data_.GetAddress() + (size_ - 1), data_.GetAddress() + size_);
            data_[index] = std::move(elem);
            ++size_;
        }
        return data_ + index;
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        size_t index = pos - begin();
        std::move(data_ + (index + 1), data_ + size_, data_ + index);
        std::destroy_at(data_ + (size_ - 1));
        --size_;
        return data_ + index;
    }

    void PopBack() noexcept {
        std::destroy_at(data_ + (size_ - 1));
        --size_;
    }

    void Swap(Vector& other) noexcept {
        this->data_.Swap(other.data_);
        std::swap(this->size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
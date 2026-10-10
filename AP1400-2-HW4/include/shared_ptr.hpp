#include <iostream>
#include <atomic>
#include <string>
#include <utility>
#include <stdexcept>

template <class T>
class SharedPtr
{
    T *_p;                   // 指向被管理对象
    std::atomic<int> *count; // 指向引用计数（堆上分配，多个 SharedPtr 共享）

public:
    // ==================== 构造 / 析构 ====================

    // 带参构造：接受裸指针，默认 nullptr
    // explicit 防止 T* 隐式转换成 SharedPtr
    explicit SharedPtr(T *p = nullptr) : _p(p)
    {
        if (p)
            count = new std::atomic<int>(1); // 有资源，计数从 1 开始
        else
            count = new std::atomic<int>(0); // 空指针，计数为 0
    }

    // 析构：引用计数减 1，减到 0 时释放资源和计数块
    ~SharedPtr()
    {
        // count 可能为 nullptr（移动后被掏空），必须先判空
        // fetch_sub 返回"减之前"的值，==1 说明当前是最后一个持有者
        if (count && count->fetch_sub(1) == 1)
        {
            delete _p;    // 先释放资源
            delete count; // 再释放计数块
        }
        _p = nullptr;
        count = nullptr;
    }

    // ==================== 拷贝语义 ====================

    // 拷贝构造：共享资源，计数 +1
    SharedPtr(const SharedPtr &other)
        : _p(other._p), count(other.count) // 初始化列表直接初始化
    {
        // other 可能被移动过，count 为 nullptr
        if (count)
            count->fetch_add(1); // 原子加 1
    }

    // 拷贝赋值：先释放自己原有的资源，再共享对方的资源
    // 参数必须是 const 引用，才能绑定 const 对象和临时对象
    SharedPtr &operator=(const SharedPtr &other)
    {
        if (this != &other) // 防止自赋值
        {
            release(); // 释放自己当前持有的资源

            _p = other._p;
            count = other.count;
            if (count)
                count->fetch_add(1); // 共享对方资源，计数 +1
        }
        return *this;
    }

    // ==================== 移动语义 ====================

    // 移动构造：转移所有权，不增加计数
    // noexcept 让容器（如 vector）放心使用移动
    SharedPtr(SharedPtr &&other) noexcept
        : _p(other._p), count(other.count)
    {
        other._p = nullptr;    // 源对象置空
        other.count = nullptr; // 源对象置空
    }

    // 移动赋值：先释放自己，再接管对方的资源
    SharedPtr &operator=(SharedPtr &&other) noexcept
    {
        if (this != &other) // 防止自赋值（ptr = std::move(ptr)）
        {
            release(); // 释放自己当前持有的资源

            _p = other._p;
            count = other.count;
            other._p = nullptr;    // 源对象置空
            other.count = nullptr; // 源对象置空
        }
        return *this;
    }

    // ==================== 观察器 ====================

    // 返回当前引用计数；count 为空时返回 0
    int use_count() const
    {
        return count ? count->load() : 0;
    }

    // 返回裸指针，不转移所有权
    T *get() const { return _p; }

    // 解引用：返回对象的引用，空指针时抛异常
    T &operator*() const
    {
        if (!_p)
            throw std::runtime_error("解引用空指针");
        return *_p;
    }

    // 箭头运算符：返回裸指针，空指针时抛异常
    T *operator->() const
    {
        if (!_p)
            throw std::runtime_error("解引用空指针");
        return _p;
    }

    // ==================== 修改器 ====================

    // 释放当前资源，接管新指针 p（默认 nullptr，即置空）
    void reset(T *p = nullptr)
    {
        if (count && count->fetch_sub(1) == 1)
        {
            delete _p;
            delete count;
        }

        _p = p;
        if (p)
            count = new std::atomic<int>(1); // 新资源，计数从 1 开始
        else
            count = new std::atomic<int>(0); // 空指针，计数为 0
    }

    // ==================== 布尔转换 ====================

    // explicit 防止隐式转换成 bool / int
    // 只在 if、while、!、&&、||、三元条件等布尔上下文中生效
    explicit operator bool() const
    {
        return _p != nullptr;
    }

private:
    // 释放当前持有的资源：计数减 1，减到 0 时释放资源和计数块
    // 把重复的释放逻辑抽出来，供拷贝赋值、移动赋值、reset 复用
    void release()
    {
        if (count && count->fetch_sub(1) == 1)
        {
            delete _p;
            delete count;
        }
        _p = nullptr;
        count = nullptr;
    }
};

// ==================== 工厂函数 ====================

// 完美转发：把参数原样转发给 T 的构造函数
// 传左值 -> 拷贝构造 T；传右值 -> 移动构造 T
template <class T, class... Args>
SharedPtr<T> make_shared(Args &&...args)
{
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

// ==================== 测试 ====================

// int main()
// {
//     // 直接初始化
//     SharedPtr<std::string> ptr{new std::string{"hello"}};

//     if (ptr) // operator bool，非空为 true
//         std::cout << "true, use_count = " << ptr.use_count() << std::endl;

//     // 拷贝构造：计数 +1
//     SharedPtr<std::string> ptr2 = ptr;
//     std::cout << "after copy, use_count = " << ptr.use_count() << std::endl;

//     // 移动构造：计数不变，ptr2 被掏空
//     SharedPtr<std::string> ptr3 = std::move(ptr2);
//     std::cout << "after move, ptr2 valid = " << static_cast<bool>(ptr2)
//               << ", ptr3 use_count = " << ptr3.use_count() << std::endl;

//     // reset：释放旧资源，置空
//     ptr.reset();
//     if (!ptr) // 空指针为 false
//         std::cout << "false, use_count = " << ptr.use_count() << std::endl;

//     // make_shared：完美转发，直接构造
//     auto p4 = make_shared<std::string>("world");
//     std::cout << "p4 = " << *p4 << ", use_count = " << p4.use_count() << std::endl;

//     return 0;
// }
#include <iostream>
#include <string>



template <class T>
class UniquePtr
{
    T *_p;

public:
    explicit UniquePtr(T *_p) : _p(_p)
    { // 初始化列表可以这么写(重名没问题，尽管不重名最好)
      // explicit防止发生隐式转换
      // UniquePtr<int>p=new int(6);这样是不允许的, int*不能赋值给p
      // 如果没有explicit编译器会尝试将int*变成UniquePtr的对象，即隐式转换
      // 必须UniquePtr<int>p(new int(6));
    }
    UniquePtr() : _p(nullptr) {} // 默认构造函数
    ~UniquePtr()                 // 析构函数
    {
        delete _p; // 删除空指针也没问题，不报错
    }

    UniquePtr(const UniquePtr &) = delete;            // 禁用 拷贝构造函数
    UniquePtr &operator=(const UniquePtr &) = delete; // 禁用拷贝赋值运算符，一般来说不禁用时，拷贝赋值返回值是同类的引用，一般是*this

    UniquePtr(UniquePtr &&other) noexcept // noexcept表明这个函数不会抛异常，一些容器必须要求用不会抛异常的函数
    {                                     // 移动构造函数
        this->_p = other._p;              // 右值引用，转移资源所有权
        other._p = nullptr;
    }
    UniquePtr &operator=(UniquePtr &&other) noexcept
    {
        if (this != &other) // 防止自赋值
        {
            delete _p; // 防止内存泄漏
            _p = other._p;
            other._p = nullptr;
        }
        return *this; // 返回同类的引用
    }

    T *get() // 返回裸指针
    {
        return _p;
    }
    T &operator*() // 返回引用，避免拷贝开销，以及需要会对资源改动
    {
        if (!_p)
        {
            throw std::runtime_error("空指针不可解引用\n");
        }
        return *_p;
    }
    T *operator->()
    {
        if (!_p)
        {
            throw std::runtime_error("空指针不可解引用\n");
        }
        return _p;
    }

    void reset(T *p = nullptr)
    {
        if (_p != p)
        {
            delete _p;
            _p = p;
        }
    }
    T *release()
    {
        T *temp = _p;
        _p = nullptr;
        return temp;
    }
    explicit operator bool() const
    {
        return _p != nullptr;
    }
};

// template <class T>
// UniquePtr<T> make_unique(T d) // 这里是否会有局部变量销毁的风险？
// {
//     return UniquePtr<T>(new T(d)); // 新new一个避免局部变量销毁
// }
template <class T, class... Args>
UniquePtr<T> make_unique(Args &&...args) // 完美转发。args是参数包，&&配合模板以及 ...是完美转发！
{
    return UniquePtr<T>(new T(std::forward<Args>(args)...)); // 新new一个避免局部变量销毁
}

#include <chrono>

// int main()
// {
//     auto start = std::chrono::high_resolution_clock::now();

//     // 被测代码
//     for (int i = 0; i < 10000000; ++i)
//     {
//         UniquePtr<string> p = make_unique<string>("hello world"); //"hello world"临时对象，右值引用
//     }

//     auto end = std::chrono::high_resolution_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
//     std::cout << "耗时: " << duration.count() << " 微秒\n";
// }
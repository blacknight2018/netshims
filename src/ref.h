#ifndef IMP_WINDOWS_REF_H_
#define IMP_WINDOWS_REF_H_
/**			
    *		imp_windows_ref.h - 这个头文件用于建立动态智能指针				
*/
#include <Windows.h>

#ifndef OVERRIDE
#define OVERRIDE override
#endif

namespace ref {
    typedef volatile unsigned long long Atomic32;
/**
 *  所有的引用对象都必须继承于这个类 
*/
    class AutoSink
    {
    public:

        ///
        // 调用以增加对象的引用计数。 应该调用指向给定对象的指针的每个新副本。
        ///
        void virtual AddRef(void) const = 0;

        ///
	    //  调用减少对象的引用计数。 如果引用计数为0，则返回true，在这种情况下，对象应该自行删除。
	    ///
	    bool virtual Release(void) const = 0;

        ///
	    //  如果引用计数为1,则返回true
	    ///
	    bool virtual HasOneRef(void) const = 0;

    public:
        virtual ~AutoSink(){};
    };

    /**
     * 实现原子引用计数的类
    */
    class AutoRef : public AutoSink 
    {
    public:
        AutoRef():m_ref_count(0){};
		~AutoRef() {}

    public:
        void AddRef(void) const { InterlockedIncrement(&m_ref_count); }

	    bool Release(void) const { Atomic32 res = InterlockedDecrement(&m_ref_count); if (res == 0) return true; else return false; }

	    bool HasOneRef(void) const { if (InterlockedExchange(&m_ref_count, m_ref_count) == 1) return true; else return false; }

		Atomic32 GetRef(void) const { return InterlockedExchange(&m_ref_count, m_ref_count); }
    private:
        mutable Atomic32 m_ref_count;
    };

    /**
     *  用于增加原子引用计数的宏
    */
    #ifndef IMPLEMENT_NS_REFCOUNTING
    #define IMPLEMENT_NS_REFCOUNTING(ClassName) \
        public:\
            void AddRef() const OVERRIDE {m_ref.AddRef();}\
            bool Release() const OVERRIDE {if(m_ref.Release()) {delete static_cast<const ClassName*>(this);return true;} else return false;} \
            bool HasOneRef() const OVERRIDE { return m_ref.HasOneRef(); } \
        private:\
            AutoRef m_ref;
    #endif

    /**
     * 实现自动计数的模版类
     */
    template<class T>
    class AutoRefPtr
    {
    public:
        AutoRefPtr():m_ptr(nullptr){}

        AutoRefPtr(T *p):m_ptr(p){
            if(m_ptr){
                m_ptr->AddRef();
            }
        }

        AutoRefPtr(const AutoRefPtr<T> &r):m_ptr(r.m_ptr){
            if(m_ptr){
                m_ptr->AddRef();
            }
        }

        template<typename U>
        AutoRefPtr(const AutoRefPtr<U> &r):m_ptr(r.get()){
            if(m_ptr){
                m_ptr->AddRef();
            }            
        }

        ~AutoRefPtr(){
            if(m_ptr){
                m_ptr->Release();
            }
        }

    public:
        T *get() const {return m_ptr;}

        void set(T *ptr){
            if(ptr){
                if(m_ptr){
                    m_ptr->Release();
                }
                m_ptr = ptr;
                m_ptr->AddRef();
            }
        }

    public:
        operator T*() const {return m_ptr;}

        T* operator->() const {
            if(!m_ptr){
                throw (const char*)"ptr==NULL";
            }
            return m_ptr;
        }

        AutoRefPtr<T>& operator=(T *p){
            if(p){
                p->AddRef();
            }
            T *oldPtr = m_ptr;
            m_ptr = p;
            if(oldPtr){
                oldPtr->Release();
            }
            return (*this);
        }

        AutoRefPtr<T> &operator=(const AutoRefPtr<T> &r){
            return *this=r.m_ptr;
        }

        template<typename U>
        AutoRefPtr<T> &operator=(const AutoRefPtr<U> &r){
            return *this=r.get();
        }

	    void swap(T **pp) {
		    T *p = m_ptr;
		    m_ptr = *pp;
		    *pp = p;
	    }

	void swap(AutoRefPtr<T>& r) { swap(&r.m_ptr); }        

    private:
        T *m_ptr;
    };
}

#endif
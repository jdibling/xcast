#ifndef XCAST_THREADS
#define XCAST_THREADS

#include <boost/thread.hpp>

template<class Context>
class Thread 
{
public:
	Thread(unique_ptr<Context>&& rhs) 
		:	ctx_(std::move(rhs)),
			thread_(unique_ptr<boost::thread>(new boost::thread(boost::ref(*ctx_.get()))))
	{
	};

	Thread(Thread&& rhs)
		:	ctx_(std::move(rhs.ctx_)),
			thread_(std::move(rhs.thread_))
	{
	}

	Thread<Context>& operator=(Thread<Context>&& rhs)	// move assignment
	{
		ctx_ = std::move(rhs.ctx_);
		thread_ = std::move(rhs.thread_);
		return * this;
	}
	
	std::string ThreadID() const { return ctx_->ID(); }

	void join() const;

	unique_ptr<Context>			ctx_;
	unique_ptr<boost::thread>	thread_;
private:
	Thread(const Thread<Context>&);						// undefined -- not copy constructible
	Thread();											// undefined -- not default constructible
	Thread<Context>& operator=(const Thread<Context>&);	// undefined -- not copy assignable
};

template<class Context> void Thread<Context>::join() const
{
	thread_->join();
}

template<class Context>
class Threads
{
public:
	typedef vector<Thread<Context>> ThreadVec;
	ThreadVec threads_;
	void join_all() const;
};

template<class Context> void Threads<Context>::join_all() const
{
	for( ThreadVec::const_iterator it = threads_.begin(); it != threads_.end(); ++it )
		it->join();
}

#endif
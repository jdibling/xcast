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
	
	void join();

	unique_ptr<Context>			ctx_;
	unique_ptr<boost::thread>	thread_;
};

template<class Context> void Thread<Context>::join()
{
	thread_->join();
}

template<class Context>
class Threads
{
public:
	typedef vector<Thread<Context>> ThreadVec;
	ThreadVec threads_;
	void join_all();
};

template<class Context> void Threads<Context>::join_all()
{
	for( ThreadVec::iterator it = threads_.begin(); it != threads_.end(); ++it )
		it->join();
}

#endif
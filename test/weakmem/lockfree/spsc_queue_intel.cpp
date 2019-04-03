/* TAGS: ext c++ tso */
/* CC_OPTS: -DNUM_DISTINCT=5 -DNUM_ELEMS=5 */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */

// source: https://software.intel.com/en-us/articles/single-producer-single-consumer-queue
// modified to use acquire/release `std::atomic` (instead of volatile-based consume/release)
// multi-threaded test added

#include <atomic>
#include <cassert>
#include <stdlib.h>
#include <pthread.h>

// cache line size on modern x86 processors (in bytes)
size_t const cache_line_size = 64;

// single-producer/single-consumer queue
template<typename T>
class spsc_queue
{
public:
  spsc_queue()
  {
	  node* n = new node;
	  n->next_.store(0, std::memory_order_relaxed);
	  tail_.store(head_ = first_= tail_copy_ = n, std::memory_order_relaxed);
  }

  ~spsc_queue()
  {
	  node* n = first_;
	  do
	  {
		  node* next = n->next_.load(std::memory_order_relaxed);
		  delete n;
		  n = next;
	  }
	  while (n);
  }

  void enqueue(T v)
  {
	  node* n = alloc_node();
	  n->next_.store(0, std::memory_order_relaxed);
	  n->value_ = v;
	  head_->next_.store(n, std::memory_order_release);
	  head_ = n;
  }

  // returns 'false' if queue is empty
  bool dequeue(T& v)
  {
	  if (tail_.load(std::memory_order_relaxed)->next_.load(std::memory_order_acquire))
	  {
		  v = tail_.load(std::memory_order_relaxed)->next_.load(std::memory_order_relaxed)->value_;
		  tail_.store(tail_.load(std::memory_order_relaxed)->next_.load(std::memory_order_relaxed), std::memory_order_release);
		  return true;
	  }
	  else
	  {
		  return false;
	  }
  }

private:
  // internal node structure
  struct node
  {
	  std::atomic< node* > next_;
	  T value_;
  };

  // consumer part
  // accessed mainly by consumer, infrequently be producer
  std::atomic< node* > tail_; // tail of the queue

  // delimiter between consumer part and producer part,
  // so that they situated on different cache lines
  char cache_line_pad_ [cache_line_size];

  // producer part
  // accessed only by producer
  node* head_; // head of the queue
  node* first_; // last unused node (tail of node cache)
  node* tail_copy_; // helper (points somewhere between first_ and tail_)

  node* alloc_node()
  {
	  // first tries to allocate node from internal node cache,
	  // if attempt fails, allocates node via ::operator new()

	  if (first_ != tail_copy_)
	  {
		  node* n = first_;
		  first_ = first_->next_.load(std::memory_order_relaxed);
		  return n;
	  }
	  tail_copy_ = tail_.load(std::memory_order_acquire);
	  if (first_ != tail_copy_)
	  {
		  node* n = first_;
		  first_ = first_->next_.load(std::memory_order_relaxed);
		  return n;
	  }
	  node* n = new node;
	  return n;
  }

  spsc_queue(spsc_queue const&);
  spsc_queue& operator = (spsc_queue const&);
};

template< typename T >
void *writer_(void *q_) {
	writer( *static_cast< spsc_queue< T > * >( q_ ) );
	return 0;
}

template< typename T >
void writer( spsc_queue< T > &q ) {
    for ( int i = 0; i < NUM_ELEMS; ++i ) {
        q.enqueue( i % NUM_DISTINCT );
    }
}

template< typename T >
void reader( spsc_queue< T > &q ) {
    T val;
    T last = -1;
    for ( int cnt = 0; ; ) {
        if ( q.dequeue( val ) ) {
            assert( ((last + 1) % NUM_DISTINCT) == val );
            last = val;
            ++cnt;
        }
        assert( cnt <= NUM_ELEMS );
    }
}

// usage example
int main()
{
    spsc_queue< int > q;
    pthread_t wr;
    pthread_create( &wr, 0, writer_< int >, &q );
    reader( q );
}

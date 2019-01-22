#pragma once

#include <vector>

#include <stdint.h>
#include <string.h>

static const double HASH_UPPER = 0.77;

static
inline
uint64_t
FLAGS_SIZE(const uint64_t m_)
{
  return ((m_ < 16) ? 1 : (m_ >> 4));
}

static
inline
uint32_t
ROUNDUP32(uint32_t x)
{
  --x;
  x |= x>>1;
  x |= x>>2;
  x |= x>>4;
  x |= x>>8;
  x |= x>>16;
  ++x;

  return x;
}

template<typename KEY,typename VALUE>
class KHash
{
public:
  typedef uint32_t khint32_t;
  typedef uint64_t khint64_t;
  typedef uint32_t khint_t;
  typedef khint_t khiter_t;

public:
  KHash();
  ~KHash();

public:
  void clear(void);
  khiter_t get(const KEY &key_) const;
  khiter_t put(KEY &key_,
               int *rv_);
  VALUE&  val(const khiter_t i_);
  void    del(khiter_t i_);
  khint_t size(void) const;

private:
  int resize(const khint_t new_n_buckets_);

private:
  khint_t    _n_buckets;
  khint_t    _size;
  khint_t    _n_occupied;
  khint_t    _upper_bound;
  std::vector<khint32_t> _flags;
  std::vector<KEY>       _keys;
  std::vector<VALUE>     _vals;
};

template<typename KEY, typename VALUE>
KHash<KEY,VALUE>::KHash()
{

}

template<typename KEY, typename VALUE>
KHash<KEY,VALUE>::~KHash()
{
}

template<typename KEY, typename VALUE>
typename KHash<KEY,VALUE>::khiter_t
KHash<KEY,VALUE>::put(KEY &key_,
                      int *rv_)
{
  return 0;
}

template<typename KEY, typename VALUE>
typename KHash<KEY,VALUE>::khiter_t
KHash<KEY,VALUE>::get(const KEY &key_) const
{
  return 0;
}

template<typename KEY, typename VALUE>
void
KHash<KEY,VALUE>::clear(void)
{
  _flags.resize(FLAGS_SIZE(_n_buckets));
  ::memset(&_flags[0],0xAA,size);
  _size       = 0;
  _n_occupied = 0;
}

template<typename KEY, typename VALUE>
int
KHash<KEY,VALUE>::resize(const khint_t new_n_buckets_)
{
  khint_t j;
  khint_t new_n_buckets;
  khint32_t *new_flags;

  j = 1;
  new_flags = NULL;
  new_n_buckets = ROUNDUP32(new_n_buckets_);
  if(new_n_buckets_ < 4)
    new_n_buckets_ = 4;

  if(_size >= (khint_t)(new_n_buckets_ * HASH_UPPER + 0.5))
    {
      j = 0;
    }
  else
    {
      _flags.resize(FLAGS_SIZE(new_n_buckets_),0xAA);
      if(_n_buckets < new_n_buckets_)
        {
          _keys.resize(new_n_buckets_);
          _vals.resize(new_n_buckets_);
        }
    }

  return 0;
}

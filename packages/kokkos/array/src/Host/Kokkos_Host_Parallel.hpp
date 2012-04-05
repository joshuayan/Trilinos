/*
//@HEADER
// ************************************************************************
// 
//          Kokkos: Node API and Parallel Node Kernels
//              Copyright (2008) Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_HOST_PARALLEL_HPP
#define KOKKOS_HOST_PARALLEL_HPP

namespace Kokkos {
namespace Impl {

class HostInternal ;

//----------------------------------------------------------------------------
/** \brief  A thread within the pool. */

class HostThread {
public:

  typedef Host::size_type size_type ;

  inline size_type rank() const { return m_thread_rank ; }

  /** \brief  This thread waits for each fan-in thread in the barrier.
   *
   *  All threads must call this function.
   *  Entry condition: in the Active   state
   *  Exit  condition: in the Inactive state
   */
  void barrier()
  {
    HostThread * const thread_beg = m_fan_begin ;
    HostThread *       thread     = m_fan_end ;

    while ( thread_beg < thread ) {
      (--thread)->wait( HostThread::ThreadActive );
    }

    if ( m_thread_rank ) {
      // If this is not the root thread then it was activated.
      set( HostThread::ThreadInactive );
    }
  }

  /** \brief  This thread participates in the fan-in reduction.
   *
   *  All threads must call this function.
   *  Entry condition: in the Active   state
   *  Exit  condition: in the Inactive state
   */
  template< class ReduceTraits >
  inline
  void reduce( typename ReduceTraits::value_type & update )
  {
    typedef typename ReduceTraits::value_type value_type ;

    // Fan-in reduction of other threads' reduction data.
    // 1) Wait for source thread to complete its work and
    //    set its own state to 'Reducing'.
    // 2) Join source thread reduce data.
    // 3) Release source thread's reduction data and
    //    set the source thread's state to 'Inactive' state.

    HostThread * const thread_beg    = m_fan_begin ;
    HostThread *       thread_source = m_fan_end ;

    while ( thread_beg < thread_source ) {
      --thread_source ;

      // Wait until the source thread is finished with its work.
      thread_source->wait( HostThread::ThreadActive );

      // Join the source thread's reduction
      ReduceTraits::join( update ,
                          *((const value_type *) thread_source->m_reduce ) );

      thread_source->m_reduce = NULL ;
      thread_source->set( HostThread::ThreadInactive );
    }

    if ( m_thread_rank ) {
      // If this is not the root thread then it will give its
      // reduction data to another thread.
      // Set the reduction data and then set the 'Reducing' state.
      // Wait for the other thread to claim reduction data and
      // deactivate this thread.

      m_reduce = & update ;
      set(  HostThread::ThreadReducing );
      wait( HostThread::ThreadReducing );
    }
  }

  inline
  std::pair< size_type , size_type >
    work_range( const size_type work_count ) const
  {
    const size_type work_per_thread = ( work_count + m_thread_count - 1 ) / m_thread_count ;
    const size_type work_previous   = work_per_thread * m_thread_reverse_rank ;
    const size_type work_end        = work_count > work_previous ? work_count - work_previous : 0 ;

    return std::pair<size_type,size_type>( work_end > work_per_thread ?
                                           work_end - work_per_thread : 0 , work_end );
  }

  void driver();

private:

  ~HostThread() {}

  HostThread()
    : m_fan_begin( NULL )
    , m_fan_end(   NULL )
    , m_thread_count( 0 )
    , m_thread_rank( 0 )
    , m_thread_reverse_rank( 0 )
    , m_reduce( NULL )
    , m_state( 0 )
    {}

  /** \brief States of a worker thread */
  enum State { ThreadNull = 0    ///<  Does not exist
             , ThreadTerminating ///<  Exists, termination in progress
             , ThreadInactive    ///<  Exists, waiting for work
             , ThreadActive      ///<  Exists, performing work
             , ThreadReducing    ///<  Exists, waiting for reduction
             };

  void set(  const State flag ) { m_state = flag ; }
  void wait( const State flag );

  HostThread          * m_fan_begin ; ///< Begin of thread fan in
  HostThread          * m_fan_end ;   ///< End of thread fan in
  unsigned              m_thread_count ;
  unsigned              m_thread_rank ;
  unsigned              m_thread_reverse_rank ;
  const void * volatile m_reduce ;    ///< Reduction memory
  long         volatile m_state ;     ///< Thread control flag

  friend class HostInternal ;
};

//----------------------------------------------------------------------------
/** \brief  Base class for a parallel driver executing on a thread pool. */

template< class ValueType = void > class HostThreadWorker ;

template<>
class HostThreadWorker<void> {
public:

  /** \brief  Virtual method called on threads */
  virtual void execute_on_thread( HostThread & ) const = 0 ;

  virtual ~HostThreadWorker() {}

protected:

  HostThreadWorker() {}

  static void execute( const HostThreadWorker & );

private:

  HostThreadWorker( const HostThreadWorker & );
  HostThreadWorker & operator = ( const HostThreadWorker & );
};

template< class ValueType >
class HostThreadWorker {
public:

  /** \brief  Virtual method called on threads */
  virtual void execute_on_thread( HostThread & , ValueType & ) const = 0 ;

  virtual ~HostThreadWorker() {}

protected:

  HostThreadWorker() {}

private:

  HostThreadWorker( const HostThreadWorker & );
  HostThreadWorker & operator = ( const HostThreadWorker & );
};

//----------------------------------------------------------------------------

template< typename DstType , typename SrcType  >
class HostParallelCopy : public HostThreadWorker<void> {
public:

        DstType * const m_dst ;
  const SrcType * const m_src ;
  const Host::size_type m_count ;

  void execute_on_thread( HostThread & this_thread ) const
  {
    std::pair<Host::size_type,Host::size_type> range =
      this_thread.work_range( m_count );
    DstType * const x_end = m_dst + range.second ;
    DstType *       x     = m_dst + range.first ;
    const SrcType * y     = m_src + range.first ;

    for ( ; x_end != x ; ++x , ++y ) { *x = (DstType) *y ; }

    this_thread.barrier();
  }

  HostParallelCopy( DstType * dst , const SrcType * src ,
                    Host::size_type count )
    : HostThreadWorker<void>()
    , m_dst( dst ), m_src( src ), m_count( count )
    { HostThreadWorker<void>::execute( *this ); }
};

template< typename DstType >
class HostParallelFill : public HostThreadWorker<void> {
public:

  DstType * const m_dst ;
  const DstType   m_src ;
  const Host::size_type m_count ;

  void execute_on_thread( HostThread & this_thread ) const
  {
    std::pair<Host::size_type,Host::size_type> range =
      this_thread.work_range( m_count );
    DstType * const x_end = m_dst + range.second ;
    DstType *       x     = m_dst + range.first ;

    for ( ; x_end != x ; ++x ) { *x = m_src ; }

    this_thread.barrier();
  }

  template< typename SrcType >
  HostParallelFill( DstType * dst , const SrcType & src ,
                    Host::size_type count )
    : HostThreadWorker<void>()
    , m_dst( dst ), m_src( src ), m_count( count )
    { HostThreadWorker<void>::execute( *this ); }
};

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------

} // namespace Impl
} // namespace Kokkos

#endif /* #define KOKKOS_HOST_PARALLEL_HPP */


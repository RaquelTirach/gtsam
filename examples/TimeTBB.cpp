/* ----------------------------------------------------------------------------

* GTSAM Copyright 2010, Georgia Tech Research Corporation,
* Atlanta, Georgia 30332-0415
* All Rights Reserved
* Authors: Frank Dellaert, et al. (see THANKS for the full author list)

* See LICENSE for the license information
* -------------------------------------------------------------------------- */

/**
* @file    TimeTBB.cpp
* @brief   Measure task scheduling overhead in TBB
* @author  Richard Roberts
* @date    November 6, 2013
*/

#include <gtsam/global_includes.h>
#include <gtsam/base/Matrix.h>

#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>
#include <map>

using namespace std;
using namespace gtsam;
using boost::assign::list_of;

#ifdef GTSAM_USE_TBB

#include <tbb/tbb.h>
#undef max // TBB seems to include windows.h and we don't want these macros
#undef min

static const DenseIndex numberOfProblems = 1000000;
static const DenseIndex problemSize = 4;

typedef Eigen::Matrix<double, problemSize, problemSize> FixedMatrix;

/* ************************************************************************* */
struct ResultWithThreads
{
  typedef map<int, double>::value_type value_type;
  map<int, double> grainSizesWithoutAllocation;
  map<int, double> grainSizesWithAllocation;
};

typedef map<int, ResultWithThreads> Results;

/* ************************************************************************* */
struct WorkerWithoutAllocation
{
  vector<double>& results;

  WorkerWithoutAllocation(vector<double>& results) : results(results) {}

  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    for(size_t i = r.begin(); i != r.end(); ++i)
    {
      FixedMatrix m1 = FixedMatrix::Random();
      FixedMatrix m2 = FixedMatrix::Random();
      FixedMatrix prod = m1 * m2;
      results[i] = prod.norm();
    }
  }
};

/* ************************************************************************* */
map<int, double> testWithoutMemoryAllocation()
{
  // A function to do some matrix operations without allocating any memory

  // Now call it
  vector<double> results(numberOfProblems);

  const vector<size_t> grainSizes = list_of(1)(10)(100)(1000);
  map<int, double> timingResults;
  BOOST_FOREACH(size_t grainSize, grainSizes)
  {
    tbb::tick_count t0 = tbb::tick_count::now();
    tbb::parallel_for(tbb::blocked_range<size_t>(0, numberOfProblems), WorkerWithoutAllocation(results));
    tbb::tick_count t1 = tbb::tick_count::now();
    cout << "Without memory allocation, grain size = " << grainSize << ", time = " << (t1 - t0).seconds() << endl;
    timingResults[(int)grainSize] = (t1 - t0).seconds();
  }

  return timingResults;
}

/* ************************************************************************* */
struct WorkerWithAllocation
{
  vector<double>& results;

  WorkerWithAllocation(vector<double>& results) : results(results) {}

  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    tbb::cache_aligned_allocator<double> allocator;
    for(size_t i = r.begin(); i != r.end(); ++i)
    {
      double *m1data = allocator.allocate(problemSize * problemSize);
      Eigen::Map<Matrix> m1(m1data, problemSize, problemSize);
      double *m2data = allocator.allocate(problemSize * problemSize);
      Eigen::Map<Matrix> m2(m2data, problemSize, problemSize);
      double *proddata = allocator.allocate(problemSize * problemSize);
      Eigen::Map<Matrix> prod(proddata, problemSize, problemSize);

      m1 = Eigen::Matrix4d::Random(problemSize, problemSize);
      m2 = Eigen::Matrix4d::Random(problemSize, problemSize);
      prod = m1 * m2;
      results[i] = prod.norm();

      allocator.deallocate(m1data, problemSize * problemSize);
      allocator.deallocate(m2data, problemSize * problemSize);
      allocator.deallocate(proddata, problemSize * problemSize);
    }
  }
};

/* ************************************************************************* */
map<int, double> testWithMemoryAllocation()
{
  // A function to do some matrix operations with allocating memory

  // Now call it
  vector<double> results(numberOfProblems);

  const vector<size_t> grainSizes = list_of(1)(10)(100)(1000);
  map<int, double> timingResults;
  BOOST_FOREACH(size_t grainSize, grainSizes)
  {
    tbb::tick_count t0 = tbb::tick_count::now();
    tbb::parallel_for(tbb::blocked_range<size_t>(0, numberOfProblems), WorkerWithAllocation(results));
    tbb::tick_count t1 = tbb::tick_count::now();
    cout << "With memory allocation, grain size = " << grainSize << ", time = " << (t1 - t0).seconds() << endl;
    timingResults[(int)grainSize] = (t1 - t0).seconds();
  }

  return timingResults;
}

/* ************************************************************************* */
int main(int argc, char* argv[])
{
  cout << "numberOfProblems = " << numberOfProblems << endl;
  cout << "problemSize = " << problemSize << endl;

  const vector<int> numThreads = list_of(1)(4)(8);
  Results results;

  BOOST_FOREACH(size_t n, numThreads)
  {
    cout << "With " << n << " threads:" << endl;
    tbb::task_scheduler_init init((int)n);
    results[(int)n].grainSizesWithoutAllocation = testWithoutMemoryAllocation();
    results[(int)n].grainSizesWithAllocation = testWithMemoryAllocation();
    cout << endl;
  }

  cout << "Summary of results:" << endl;
  BOOST_FOREACH(const Results::value_type& threads_result, results)
  {
    const int threads = threads_result.first;
    const ResultWithThreads& result = threads_result.second;
    if(threads != 1)
    {
      BOOST_FOREACH(const ResultWithThreads::value_type& grainsize_time, result.grainSizesWithoutAllocation)
      {
        const int grainsize = grainsize_time.first;
        const double speedup = results[1].grainSizesWithoutAllocation[grainsize] / grainsize_time.second;
        cout << threads << " threads, without allocation, grain size = " << grainsize << ", speedup = " << speedup << endl;
      }
      BOOST_FOREACH(const ResultWithThreads::value_type& grainsize_time, result.grainSizesWithAllocation)
      {
        const int grainsize = grainsize_time.first;
        const double speedup = results[1].grainSizesWithAllocation[grainsize] / grainsize_time.second;
        cout << threads << " threads, with allocation, grain size = " << grainsize << ", speedup = " << speedup << endl;
      }
    }
  }

  return 0;
}

#else

/* ************************************************************************* */
int main(int argc, char* argv [])
{
  cout << "GTSAM is compiled without TBB, please compile with TBB to use this program." << endl;
  return 0;
}

#endif

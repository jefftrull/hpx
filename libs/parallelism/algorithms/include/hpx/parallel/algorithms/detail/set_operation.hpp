//  Copyright (c) 2007-2015 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <hpx/config.hpp>
#include <hpx/assert.hpp>

#include <hpx/execution/executors/execution_information.hpp>
#include <hpx/executors/execution_policy.hpp>
#include <hpx/parallel/util/detail/algorithm_result.hpp>
#include <hpx/parallel/util/foreach_partitioner.hpp>
#include <hpx/parallel/util/partitioner.hpp>

#if !defined(HPX_HAVE_CXX17_SHARED_PTR_ARRAY)
#include <boost/shared_array.hpp>
#else
#include <memory>
#endif

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace hpx { namespace parallel { inline namespace v1 { namespace detail {
    /// \cond NOINTERNAL

    ///////////////////////////////////////////////////////////////////////////
    template <typename FwdIter>
    struct set_operations_buffer
    {
        template <typename T>
        class rewritable_ref
        {
        public:
            rewritable_ref()
              : item_(0)
            {
            }
            rewritable_ref(T const& item)
              : item_(item)
            {
            }

            rewritable_ref& operator=(T const& item)
            {
                item_ = &item;
                return *this;
            }

            // different versions of clang-format produce different results
            // clang-format off
            operator T const&() const
            {
                HPX_ASSERT(item_ != 0);
                return *item_;
            }
            // clang-format on

        private:
            T const* item_;
        };

        typedef typename std::iterator_traits<FwdIter>::value_type value_type;
        typedef typename std::conditional<std::is_scalar<value_type>::value,
            value_type, rewritable_ref<value_type>>::type type;
    };

    struct set_chunk_data
    {
        set_chunk_data()
        {
            start = len = start_index = (std::size_t)(-1);
        }
        std::size_t start;
        std::size_t len;
        std::size_t start_index;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename ExPolicy, typename RanIter1, typename RanIter2,
        typename FwdIter, typename F, typename Combiner, typename SetOp>
    typename util::detail::algorithm_result<ExPolicy, FwdIter>::type
    set_operation(ExPolicy policy, RanIter1 first1, RanIter1 last1,
        RanIter2 first2, RanIter2 last2, FwdIter dest, F&& f,
        Combiner&& combiner, SetOp&& setop)
    {
        typedef typename std::iterator_traits<RanIter1>::difference_type
            difference_type1;
        typedef typename std::iterator_traits<RanIter2>::difference_type
            difference_type2;

        // allocate intermediate buffers
        difference_type1 len1 = std::distance(first1, last1);
        difference_type2 len2 = std::distance(first2, last2);

        typedef typename set_operations_buffer<FwdIter>::type buffer_type;

        std::size_t cores = execution::processing_units_count(
            policy.parameters(), policy.executor());

        std::size_t step = (len1 + cores - 1) / cores;

#if defined(HPX_HAVE_CXX17_SHARED_PTR_ARRAY)
        std::shared_ptr<buffer_type[]> buffer(
            new buffer_type[combiner(len1, len2)]);
        std::shared_ptr<set_chunk_data[]> chunks(new set_chunk_data[cores]);
#else
        boost::shared_array<buffer_type> buffer(
            new buffer_type[combiner(len1, len2)]);
        boost::shared_array<set_chunk_data> chunks(new set_chunk_data[cores]);
#endif

        // first step, is applied to all partitions
        auto f1 = [=](set_chunk_data* curr_chunk,
                      std::size_t part_size) -> void {
            HPX_ASSERT(part_size == 1);

            // find start in sequence 1
            std::size_t start1 = (curr_chunk - chunks.get()) * step;
            std::size_t end1 = (std::min)(start1 + step, std::size_t(len1));

            if (start1 >= end1)
                return;

            bool first_partition = (start1 == 0);
            bool last_partition = (end1 == std::size_t(len1));

            // all but the last chunk require special handling
            if (!last_partition)
            {
                // this chunk will be handled by the next one if all
                // elements of this partition are equal
                if (!f(first1[start1], first1[end1]))
                    return;

                // move backwards to find earliest element which is equal to
                // the last element of the current chunk
                while (end1 != 0 && !f(first1[end1 - 1], first1[end1]))
                    --end1;
            }

            // move backwards to find earliest element which is equal to
            // the first element of the current chunk
            while (start1 != 0 && !f(first1[start1 - 1], first1[start1]))
                --start1;

            // find start and end in sequence 2
            std::size_t start2 = 0;
            if (!first_partition)
            {
                start2 =
                    std::lower_bound(first2, first2 + len2, first1[start1], f) -
                    first2;
            }

            std::size_t end2 = len2;
            if (!last_partition)
            {
                end2 = std::lower_bound(
                           first2 + start2, first2 + len2, first1[end1], f) -
                    first2;
            }

            // perform requested set-operation into the proper place of the
            // intermediate buffer
            curr_chunk->start = combiner(start1, start2);
            auto buffer_dest = buffer.get() + curr_chunk->start;
            curr_chunk->len =
                setop(first1 + start1, first1 + end1, first2 + start2,
                    first2 + end2, buffer_dest, f) -
                buffer_dest;
        };

        // second step, is executed after all partitions are done running
        auto f2 = [buffer, chunks, cores, dest](
                      std::vector<future<void>> &&) -> FwdIter {
            // accumulate real length
            set_chunk_data* chunk = chunks.get();
            chunk->start_index = 0;
            for (size_t i = 1; i != cores; ++i)
            {
                set_chunk_data* curr_chunk = chunk++;
                chunk->start_index = curr_chunk->start_index + curr_chunk->len;
            }

            // finally, copy data to destination
            parallel::util::
                foreach_partitioner<hpx::execution::parallel_policy>::call(
                    hpx::execution::par, chunks.get(), cores,
                    [buffer, dest](
                        set_chunk_data* chunk, std::size_t, std::size_t) {
                        if (chunk->start == (size_t)(-1) ||
                            chunk->start_index == (size_t)(-1) ||
                            chunk->len == (size_t)(-1))
                            return;

                        std::copy(buffer.get() + chunk->start,
                            buffer.get() + chunk->start + chunk->len,
                            dest + chunk->start_index);
                    },
                    [](set_chunk_data* last) -> set_chunk_data* {
                        return last;
                    });

            return dest;
        };

        // fill the buffer piecewise
        return parallel::util::partitioner<ExPolicy, FwdIter, void>::call(
            policy, chunks.get(), cores, std::move(f1), std::move(f2));
    }

    /// \endcond
}}}}    // namespace hpx::parallel::v1::detail

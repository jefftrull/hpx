////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Bryce Lelbach
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#if !defined(HPX_703646B3_0567_484E_AD34_A752B8163B30)
#define HPX_703646B3_0567_484E_AD34_A752B8163B30

#include <hpx/config.hpp>
#include <hpx/threading_base/scheduler_state.hpp>

namespace hpx
{
    namespace threads
    {
        // return whether thread manager is in the state described by 'mask'
        HPX_EXPORT bool threadmanager_is(state st);
        HPX_EXPORT bool threadmanager_is_at_least(state st);
    }

    namespace agas
    {
        // return whether resolver client is in state described by 'mask'
        HPX_EXPORT bool router_is(state st);
    }
}

#endif // HPX_703646B3_0567_484E_AD34_A752B8163B30


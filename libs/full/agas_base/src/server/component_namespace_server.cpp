////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Bryce Adelstein-Lelbach
//  Copyright (c) 2012-2017 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#include <hpx/config.hpp>
#include <hpx/agas_base/server/component_namespace.hpp>
#include <hpx/assert.hpp>
#include <hpx/async_distributed/continuation.hpp>
#include <hpx/components_base/agas_interface.hpp>
#include <hpx/modules/format.hpp>
#include <hpx/naming/credit_handling.hpp>
#include <hpx/timing/scoped_timer.hpp>
#include <hpx/util/get_and_reset_value.hpp>
#include <hpx/util/insert_checked.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// TODO: Remove the use of the name "prefix"

namespace hpx { namespace agas {

    naming::gid_type bootstrap_component_namespace_gid()
    {
        return naming::gid_type(agas::component_ns_msb, agas::component_ns_lsb);
    }

    naming::id_type bootstrap_component_namespace_id()
    {
        return naming::id_type(agas::component_ns_msb, agas::component_ns_lsb,
            naming::id_type::unmanaged);
    }
}}    // namespace hpx::agas

namespace hpx { namespace agas { namespace server {

    void component_namespace::register_server_instance(
        char const* servicename, error_code& ec)
    {
        // now register this AGAS instance with AGAS :-P
        instance_name_ = agas::service_name;
        instance_name_ += servicename;
        instance_name_ += agas::server::component_namespace_service_name;

        // register a gid (not the id) to avoid AGAS holding a reference to this
        // component
        agas::register_name(
            launch::sync, instance_name_, get_unmanaged_id().get_gid(), ec);
    }

    void component_namespace::unregister_server_instance(error_code& ec)
    {
        agas::unregister_name(launch::sync, instance_name_, ec);
        this->base_type::finalize();
    }

    void component_namespace::finalize()
    {
        if (!instance_name_.empty())
        {
            error_code ec(lightweight);
            agas::unregister_name(launch::sync, instance_name_, ec);
        }
    }

    components::component_type component_namespace::bind_prefix(
        std::string const& key, std::uint32_t prefix)
    {    // {{{ bind_prefix implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.bind_prefix_.time_,
            counter_data_.bind_prefix_.enabled_);
        counter_data_.increment_bind_prefix_count();

        std::unique_lock<mutex_type> l(mutex_);

        component_id_table_type::left_map::iterator
            cit = component_ids_.left.find(key),
            cend = component_ids_.left.end();

        // This is the first request, so we use the type counter, and then
        // increment it.
        if (component_ids_.left.find(key) == cend)
        {
            if (HPX_UNLIKELY(!util::insert_checked(
                    component_ids_.left.insert(
                        std::make_pair(key, type_counter)),
                    cit)))
            {
                l.unlock();

                HPX_THROW_EXCEPTION(lock_error,
                    "component_namespace::bind_prefix",
                    "component id table insertion failed due to a locking "
                    "error or memory corruption");
                return components::component_invalid;
            }

            // If the insertion succeeded, we need to increment the type
            // counter.
            ++type_counter;
        }

        factory_table_type::iterator fit = factories_.find(cit->second),
                                     fend = factories_.end();

        if (fit != fend)
        {
            prefixes_type& prefixes = fit->second;
            prefixes_type::iterator pit = prefixes.find(prefix);

            if (pit != prefixes.end())
            {
                // Duplicate type registration for this locality.
                l.unlock();

                HPX_THROW_EXCEPTION(duplicate_component_id,
                    "component_namespace::bind_prefix",
                    hpx::util::format(
                        "component id is already registered for the given "
                        "locality, key({1}), prefix({2}), ctype({3})",
                        key, prefix, cit->second));
                return components::component_invalid;
            }

            fit->second.insert(prefix);

            // First registration for this locality, we still return no_success to
            // convey the fact that another locality already registered this
            // component type.
            LAGAS_(info) << hpx::util::format(
                "component_namespace::bind_prefix, key({1}), prefix({2}), "
                "ctype({3}), response(no_success)",
                key, prefix, cit->second);

            return cit->second;
        }

        // Instead of creating a temporary and then inserting it, we insert
        // an empty set, then put the prefix into said set. This should
        // prevent a copy, though most compilers should be able to optimize
        // this without our help.
        if (HPX_UNLIKELY(!util::insert_checked(
                factories_.insert(std::make_pair(cit->second, prefixes_type())),
                fit)))
        {
            l.unlock();

            HPX_THROW_EXCEPTION(lock_error, "component_namespace::bind_prefix",
                "factory table insertion failed due to a locking "
                "error or memory corruption");
            return components::component_invalid;
        }

        fit->second.insert(prefix);

        LAGAS_(info) << hpx::util::format(
            "component_namespace::bind_prefix, key({1}), prefix({2}), "
            "ctype({3})",
            key, prefix, cit->second);

        return cit->second;
    }    // }}}

    components::component_type component_namespace::bind_name(
        std::string const& key)
    {    // {{{ bind_name implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.bind_name_.time_, counter_data_.bind_name_.enabled_);
        counter_data_.increment_bind_name_count();

        std::unique_lock<mutex_type> l(mutex_);

        component_id_table_type::left_map::iterator
            it = component_ids_.left.find(key),
            end = component_ids_.left.end();

        // If the name is not in the table, register it (this is only done so
        // we can implement a backwards compatible get_component_id).
        if (it == end)
        {
            if (HPX_UNLIKELY(!util::insert_checked(
                    component_ids_.left.insert(
                        std::make_pair(key, type_counter)),
                    it)))
            {
                l.unlock();

                HPX_THROW_EXCEPTION(lock_error,
                    "component_namespace::bind_name",
                    "component id table insertion failed due to a locking "
                    "error or memory corruption");
                return components::component_invalid;
            }

            // If the insertion succeeded, we need to increment the type
            // counter.
            ++type_counter;
        }

        LAGAS_(info) << hpx::util::format(
            "component_namespace::bind_name, key({1}), ctype({2})", key,
            it->second);

        return it->second;
    }    // }}}

    std::vector<std::uint32_t> component_namespace::resolve_id(
        components::component_type key)
    {    // {{{ resolve_id implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.resolve_id_.time_,
            counter_data_.resolve_id_.enabled_);
        counter_data_.increment_resolve_id_count();

        // If the requested component type is a derived type, use only its derived
        // part for the lookup.
        if (key != components::get_base_type(key))
            key = components::get_derived_type(key);

        std::lock_guard<mutex_type> l(mutex_);

        factory_table_type::const_iterator it = factories_.find(key),
                                           end = factories_.end();

        // REVIEW: Should we differentiate between these two cases? Should we
        // throw an exception if it->second.empty()? It should be impossible.
        if (it == end || it->second.empty())
        {
            LAGAS_(info) << hpx::util::format(
                "component_namespace::resolve_id, key({1}), localities(0)",
                key);

            return std::vector<std::uint32_t>();
        }

        else
        {
            std::vector<std::uint32_t> p;

            prefixes_type const& prefixes = it->second;
            prefixes_type::const_iterator pit = prefixes.begin(),
                                          pend = prefixes.end();

            for (; pit != pend; ++pit)
                p.push_back(*pit);

            LAGAS_(info) << hpx::util::format(
                "component_namespace::resolve_id, "
                "key({1}), localities({2})",
                key, prefixes.size());

            return p;
        }
    }    // }}}

    bool component_namespace::unbind(std::string const& key)
    {    // {{{ unbind implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.unbind_name_.time_,
            counter_data_.unbind_name_.enabled_);
        counter_data_.increment_unbind_name_count();

        std::lock_guard<mutex_type> l(mutex_);

        component_id_table_type::left_map::iterator it =
            component_ids_.left.find(key);

        // REVIEW: Should this be an error?
        if (it == component_ids_.left.end())
        {
            LAGAS_(info) << hpx::util::format("component_namespace::unbind, "
                                              "key({1}), response(no_success)",
                key);

            return false;
        }

        // REVIEW: If there are no localities with this type, should we throw
        // an exception here?
        factories_.erase(it->second);
        component_ids_.left.erase(it);

        LAGAS_(info) << hpx::util::format(
            "component_namespace::unbind, key({1})", key);

        return true;
    }    // }}}

    // TODO: catch exceptions
    void component_namespace::iterate_types(
        iterate_types_function_type const& f)
    {    // {{{ iterate implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.iterate_types_.time_,
            counter_data_.iterate_types_.enabled_);
        counter_data_.increment_iterate_types_count();

        std::vector<typename component_id_table_type::left_map::value_type>
            types;
        types.reserve(component_ids_.size());

        {
            std::lock_guard<mutex_type> l(mutex_);

            for (component_id_table_type::left_map::iterator
                     it = component_ids_.left.begin(),
                     end = component_ids_.left.end();
                 it != end; ++it)
            {
                types.push_back(*it);
            }
        }

        for (auto&& type : types)
        {
            f(type.first, type.second);
        }

        LAGAS_(info) << "component_namespace::iterate_types";
    }    // }}}

    template <typename Map>
    std::string get_component_name(Map const& m, components::component_type t)
    {
        if (t < components::component_last)
            return components::get_component_type_name(t);

        typename Map::const_iterator it = m.find(t);
        if (it == m.end())
            return "";

        return (*it).second;
    }

    std::string component_namespace::get_component_type_name(
        components::component_type t)
    {    // {{{ get_component_type_name implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.get_component_type_name_.time_,
            counter_data_.get_component_type_name_.enabled_);
        counter_data_.increment_get_component_type_name_count();

        std::lock_guard<mutex_type> l(mutex_);

        std::string result;

        if (t == components::component_invalid)
        {
            result = "component_invalid";
        }
        else if (components::get_derived_type(t) == 0)
        {
            result = get_component_name(component_ids_.right, t);
        }
        else if (components::get_derived_type(t) != 0)
        {
            result = get_component_name(
                component_ids_.right, components::get_derived_type(t));
            result += "/";
            result += get_component_name(
                component_ids_.right, components::get_base_type(t));
        }

        if (result.empty())
        {
            LAGAS_(info) << hpx::util::format(
                "component_namespace::get_component_typename, "
                "key({1}/{2}), response(no_success)",
                int(components::get_derived_type(t)),
                int(components::get_base_type(t)));

            return result;
        }

        LAGAS_(info) << hpx::util::format(
            "component_namespace::get_component_typename, key({1}/{2}), "
            "response({3})",
            int(components::get_derived_type(t)),
            int(components::get_base_type(t)), result);

        return result;
    }    // }}}

    std::uint32_t component_namespace::get_num_localities(
        components::component_type key)
    {    // {{{ get_num_localities implementation
        util::scoped_timer<std::atomic<std::int64_t>> update(
            counter_data_.num_localities_.time_,
            counter_data_.num_localities_.enabled_);
        counter_data_.increment_num_localities_count();

        // If the requested component type is a derived type, use only its derived
        // part for the lookup.
        if (key != components::get_base_type(key))
            key = components::get_derived_type(key);

        std::lock_guard<mutex_type> l(mutex_);

        factory_table_type::const_iterator it = factories_.find(key),
                                           end = factories_.end();
        if (it == end)
        {
            LAGAS_(info) << hpx::util::format(
                "component_namespace::get_num_"
                "localities, key({1}), localities(0)",
                key);

            return std::uint32_t(0);
        }

        std::uint32_t num_localities =
            static_cast<std::uint32_t>(it->second.size());

        LAGAS_(info) << hpx::util::format(
            "component_namespace::get_num_localities, key({1}), "
            "localities({2})",
            key, num_localities);

        return num_localities;
    }    // }}}

    // access current counter values
    std::int64_t component_namespace::counter_data::get_bind_prefix_count(
        bool reset)
    {
        return util::get_and_reset_value(bind_prefix_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_bind_name_count(
        bool reset)
    {
        return util::get_and_reset_value(bind_name_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_resolve_id_count(
        bool reset)
    {
        return util::get_and_reset_value(resolve_id_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_unbind_name_count(
        bool reset)
    {
        return util::get_and_reset_value(unbind_name_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_iterate_types_count(
        bool reset)
    {
        return util::get_and_reset_value(iterate_types_.count_, reset);
    }

    std::int64_t
    component_namespace::counter_data ::get_component_type_name_count(
        bool reset)
    {
        return util::get_and_reset_value(
            get_component_type_name_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_num_localities_count(
        bool reset)
    {
        return util::get_and_reset_value(num_localities_.count_, reset);
    }

    std::int64_t component_namespace::counter_data::get_overall_count(
        bool reset)
    {
        return util::get_and_reset_value(bind_prefix_.count_, reset) +
            util::get_and_reset_value(bind_name_.count_, reset) +
            util::get_and_reset_value(resolve_id_.count_, reset) +
            util::get_and_reset_value(unbind_name_.count_, reset) +
            util::get_and_reset_value(iterate_types_.count_, reset) +
            util::get_and_reset_value(get_component_type_name_.count_, reset) +
            util::get_and_reset_value(num_localities_.count_, reset);
    }

    void component_namespace::counter_data::enable_all()
    {
        bind_prefix_.enabled_ = true;
        bind_name_.enabled_ = true;
        resolve_id_.enabled_ = true;
        unbind_name_.enabled_ = true;
        iterate_types_.enabled_ = true;
        get_component_type_name_.enabled_ = true;
        num_localities_.enabled_ = true;
    }

    // access execution time counters
    std::int64_t component_namespace::counter_data::get_bind_prefix_time(
        bool reset)
    {
        return util::get_and_reset_value(bind_prefix_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_bind_name_time(
        bool reset)
    {
        return util::get_and_reset_value(bind_name_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_resolve_id_time(
        bool reset)
    {
        return util::get_and_reset_value(resolve_id_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_unbind_name_time(
        bool reset)
    {
        return util::get_and_reset_value(unbind_name_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_iterate_types_time(
        bool reset)
    {
        return util::get_and_reset_value(iterate_types_.time_, reset);
    }

    std::int64_t
    component_namespace::counter_data ::get_component_type_name_time(bool reset)
    {
        return util::get_and_reset_value(get_component_type_name_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_num_localities_time(
        bool reset)
    {
        return util::get_and_reset_value(num_localities_.time_, reset);
    }

    std::int64_t component_namespace::counter_data::get_overall_time(bool reset)
    {
        return util::get_and_reset_value(bind_prefix_.time_, reset) +
            util::get_and_reset_value(bind_name_.time_, reset) +
            util::get_and_reset_value(resolve_id_.time_, reset) +
            util::get_and_reset_value(unbind_name_.time_, reset) +
            util::get_and_reset_value(iterate_types_.time_, reset) +
            util::get_and_reset_value(get_component_type_name_.time_, reset) +
            util::get_and_reset_value(num_localities_.time_, reset);
    }

    // increment counter values
    void component_namespace::counter_data::increment_bind_prefix_count()
    {
        if (bind_prefix_.enabled_)
        {
            ++bind_prefix_.count_;
        }
    }

    void component_namespace::counter_data::increment_bind_name_count()
    {
        if (bind_name_.enabled_)
        {
            ++bind_name_.count_;
        }
    }

    void component_namespace::counter_data::increment_resolve_id_count()
    {
        if (resolve_id_.enabled_)
        {
            ++resolve_id_.count_;
        }
    }

    void component_namespace::counter_data::increment_unbind_name_count()
    {
        if (unbind_name_.enabled_)
        {
            ++unbind_name_.count_;
        }
    }

    void component_namespace::counter_data::increment_iterate_types_count()
    {
        if (iterate_types_.enabled_)
        {
            ++iterate_types_.count_;
        }
    }

    void
    component_namespace::counter_data::increment_get_component_type_name_count()
    {
        if (get_component_type_name_.enabled_)
        {
            ++get_component_type_name_.count_;
        }
    }

    void component_namespace::counter_data::increment_num_localities_count()
    {
        if (num_localities_.enabled_)
        {
            ++num_localities_.count_;
        }
    }

}}}    // namespace hpx::agas::server

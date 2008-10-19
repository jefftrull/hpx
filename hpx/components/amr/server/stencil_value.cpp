//  Copyright (c) 2007-2008 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx.hpp>
#include <hpx/runtime/components/component_factory.hpp>
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>

#include <boost/serialization/version.hpp>
#include <boost/serialization/export.hpp>

#include <hpx/components/amr/server/stencil_value.hpp>
#include <hpx/components/amr/server/stencil_value.ipp>

///////////////////////////////////////////////////////////////////////////////
/// Define types of stencil components exposed by this module
typedef hpx::components::amr::server::detail::stencil_value<double, 1> stencil_value_double_1_type;
typedef hpx::components::amr::server::detail::stencil_value<double, 3> stencil_value_double_3_type;
typedef hpx::components::amr::server::detail::stencil_value<double, 5> stencil_value_double_5_type;

///////////////////////////////////////////////////////////////////////////////
/// The following construct registers a minimal factory needed for the creation
/// of new stencil instances. The name used as the second macro parameter
/// must match the component name used in the ini configuration file used
/// for this component. For instance the configuration file amr.ini may look 
/// like:
/// 
/// [hpx.components.stencil_double_3]      # this must match the string below
/// name = amr                    # this must match the name of the shared library
/// path = $[hpx.location]/lib    # this is the default location where to find the shared library
///
HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(stencil_value_double_1_type::wrapping_type, stencil_double_1);
HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(stencil_value_double_3_type::wrapping_type, stencil_double_3);
HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(stencil_value_double_5_type::wrapping_type, stencil_double_5);

///////////////////////////////////////////////////////////////////////////////
// For any component derived from manage_component_base we must use the 
// following in exactly one source file
HPX_REGISTER_MANAGED_COMPONENT(stencil_value_double_1_type);
HPX_REGISTER_MANAGED_COMPONENT(stencil_value_double_3_type);
HPX_REGISTER_MANAGED_COMPONENT(stencil_value_double_5_type);

///////////////////////////////////////////////////////////////////////////////
HPX_REGISTER_ACTION(stencil_value_double_1_type::call_action);
HPX_REGISTER_ACTION(stencil_value_double_1_type::get_output_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_1_type::connect_input_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_1_type::set_functional_component_action);

HPX_REGISTER_ACTION(stencil_value_double_3_type::call_action);
HPX_REGISTER_ACTION(stencil_value_double_3_type::get_output_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_3_type::connect_input_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_3_type::set_functional_component_action);

HPX_REGISTER_ACTION(stencil_value_double_5_type::call_action);
HPX_REGISTER_ACTION(stencil_value_double_5_type::get_output_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_5_type::connect_input_ports_action);
HPX_REGISTER_ACTION(stencil_value_double_5_type::set_functional_component_action);


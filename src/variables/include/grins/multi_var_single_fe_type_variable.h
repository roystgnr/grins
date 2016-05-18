//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// GRINS - General Reacting Incompressible Navier-Stokes
//
// Copyright (C) 2014-2015 Paul T. Bauman, Roy H. Stogner
// Copyright (C) 2010-2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

#ifndef GRINS_MULTI_VAR_SINGLE_FE_TYPE_VARIABLE_H
#define GRINS_MULTI_VAR_SINGLE_FE_TYPE_VARIABLE_H

// GRINS
#include "grins/single_fe_type_variable.h"

namespace GRINS
{
  //! Base class for variables with multiple components that all have the same FEType
  class MultiVarSingleFETypeVariable : public SingleFETypeVariable
  {
  public:

    //! Deprecated, old style constructor
    /*! This constructor is used for when there is possibly old deprecated
        styles of input for which we do additional checks/warnings. Otherwise,
        you should use the new constructor. */
    MultiVarSingleFETypeVariable( const GetPot& input,
                                  const std::string& physics_name,
                                  const std::string& old_var_prefix,
                                  const std::vector<std::string>& old_var_names,
                                  const std::vector<std::string>& default_names,
                                  const std::string& subsection,
                                  const std::string& default_family,
                                  const std::string& default_order,
                                  bool is_constraint_var );

    //! Primary constructor
    /*! Will parse from input section [Variables/<subsection>]. */
    MultiVarSingleFETypeVariable( const GetPot& input,
                                  const std::string& subsection,
                                  const std::vector<std::string>& default_names,
                                  bool is_constraint_var);

    ~MultiVarSingleFETypeVariable(){};

  };

} // end namespace GRINS

#endif // GRINS_MULTI_VAR_SINGLE_FE_TYPE_VARIABLE_H
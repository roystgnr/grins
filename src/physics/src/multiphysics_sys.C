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


// This class
#include "grins/multiphysics_sys.h"

// GRINS
#include "grins/assembly_context.h"

// libMesh
#include "libmesh/composite_function.h"
#include "libmesh/getpot.h"

namespace GRINS
{

  MultiphysicsSystem::MultiphysicsSystem( libMesh::EquationSystems& es,
					  const std::string& name,
					  const unsigned int number )
    : FEMSystem(es, name, number),
      _use_numerical_jacobians_only(false)
  {
    return;
  }

  MultiphysicsSystem::~MultiphysicsSystem()
  {
    return;
  }

  void MultiphysicsSystem::attach_physics_list( PhysicsList physics_list )
  {
    _physics_list = physics_list;
    return;
  }

  void MultiphysicsSystem::read_input_options( const GetPot& input )
  {
    // Read options for MultiphysicsSystem first
    this->verify_analytic_jacobians = input("linear-nonlinear-solver/verify_analytic_jacobians", 0.0 );
    this->print_solution_norms = input("screen-options/print_solution_norms", false );
    this->print_solutions = input("screen-options/print_solutions", false );
    this->print_residual_norms = input("screen-options/print_residual_norms", false );

    // backwards compatibility with old config files.
    /*! \todo Remove old print_residual nomenclature */
    this->print_residuals = input("screen-options/print_residual", false );
    if (this->print_residuals)
      libmesh_deprecated();

    this->print_residuals = input("screen-options/print_residuals", this->print_residuals );
    this->print_jacobian_norms = input("screen-options/print_jacobian_norms", false );
    this->print_jacobians = input("screen-options/print_jacobians", false );
    this->print_element_solutions = input("screen-options/print_element_solutions", false );
    this->print_element_residuals = input("screen-options/print_element_residuals", false );
    this->print_element_jacobians = input("screen-options/print_element_jacobians", false );

    _use_numerical_jacobians_only = input("linear-nonlinear-solver/use_numerical_jacobians_only", false );

    numerical_jacobian_h =
      input("linear-nonlinear-solver/numerical_jacobian_h",
            numerical_jacobian_h);
  }

  void MultiphysicsSystem::init_data()
  {
    // Need this to be true because of our overloading of the
    // mass_residual function.
    // This is data in FEMSystem. MUST be set before FEMSystem::init_data.
    use_fixed_solution = true;

    // Initalize all the variables. We pass this pointer for the system.
    /* NOTE: We CANNOT fuse this loop with the others. This loop
       MUST complete first. */
    /*! \todo Figure out how to tell compilers not to fuse this loop when
      they want to be aggressive. */
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	(physics_iter->second)->init_variables( this );
      }

    // Now set time_evolving variables
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	(physics_iter->second)->set_time_evolving_vars( this );
      }

    // Set whether the problem we're solving is steady or not
    // Since the variable is static, just call one Physics class
    {
      (_physics_list.begin()->second)->set_is_steady((this->time_solver)->is_steady());
    }

    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	// Initialize builtin BC's for each physics
	(physics_iter->second)->init_bcs( this );
      }

    // Next, call parent init_data function to intialize everything.
    libMesh::FEMSystem::init_data();

    // After solution has been initialized we can project initial
    // conditions to it
    libMesh::CompositeFunction<libMesh::Number> ic_function;
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	// Initialize builtin IC's for each physics
	(physics_iter->second)->init_ics( this, ic_function );
      }

    if (ic_function.n_subfunctions())
      {
        this->project_solution(&ic_function);
      }

    return;
  }

  libMesh::AutoPtr<libMesh::DiffContext> MultiphysicsSystem::build_context()
  {
    AssemblyContext* context = new AssemblyContext(*this);

    libMesh::AutoPtr<libMesh::DiffContext> ap(context);

    libMesh::DifferentiablePhysics* phys = libMesh::FEMSystem::get_physics();

    libmesh_assert(phys);

    // If we are solving a moving mesh problem, tell that to the Context
    context->set_mesh_system(phys->get_mesh_system());
    context->set_mesh_x_var(phys->get_mesh_x_var());
    context->set_mesh_y_var(phys->get_mesh_y_var());
    context->set_mesh_z_var(phys->get_mesh_z_var());

    ap->set_deltat_pointer( &deltat );

    // If we are solving the adjoint problem, tell that to the Context
    ap->is_adjoint() = this->get_time_solver().is_adjoint();

    return ap;
  }

  void MultiphysicsSystem::register_postprocessing_vars( const GetPot& input,
                                                         PostProcessedQuantities<libMesh::Real>& postprocessing )
  {
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
        (physics_iter->second)->register_postprocessing_vars( input, postprocessing );
      }

    return;
  }

  void MultiphysicsSystem::init_context( libMesh::DiffContext& context )
  {
    AssemblyContext& c = libMesh::libmesh_cast_ref<AssemblyContext&>(context);

    //Loop over each physics to initialize relevant variable structures for assembling system
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	(physics_iter->second)->init_context( c );
      }

    return;
  }


  bool MultiphysicsSystem::_general_residual( bool request_jacobian,
					      libMesh::DiffContext& context,
                                              ResFuncType resfunc,
                                              CacheFuncType cachefunc)
  {
    AssemblyContext& c = libMesh::libmesh_cast_ref<AssemblyContext&>(context);
  
    bool compute_jacobian = true;
    if( !request_jacobian || _use_numerical_jacobians_only ) compute_jacobian = false;

    CachedValues cache;

    // Now compute cache for this element
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
        // boost::shared_ptr gets confused by operator->*
	((*(physics_iter->second)).*cachefunc)( c, cache );
      }

    // Loop over each physics and compute their contributions
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
        if(c.has_elem())
          {
            if( (physics_iter->second)->enabled_on_elem( &c.get_elem() ) )
              {
                ((*(physics_iter->second)).*resfunc)( compute_jacobian, c, cache );
              }
          }
        else
          {
            ((*(physics_iter->second)).*resfunc)( compute_jacobian, c, cache );
          }
      }

    // TODO: Need to think about the implications of this because there might be some
    // TODO: jacobian terms we don't want to compute for efficiency reasons
    return compute_jacobian;
  }

  bool MultiphysicsSystem::element_time_derivative( bool request_jacobian,
						    libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::element_time_derivative,
       &GRINS::Physics::compute_element_time_derivative_cache);
  }

  bool MultiphysicsSystem::side_time_derivative( bool request_jacobian,
						 libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::side_time_derivative,
       &GRINS::Physics::compute_side_time_derivative_cache);
  }

  bool MultiphysicsSystem::nonlocal_time_derivative( bool request_jacobian,
						     libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::nonlocal_time_derivative,
       &GRINS::Physics::compute_nonlocal_time_derivative_cache);
  }

  bool MultiphysicsSystem::element_constraint( bool request_jacobian,
					       libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::element_constraint,
       &GRINS::Physics::compute_element_constraint_cache);
  }

  bool MultiphysicsSystem::side_constraint( bool request_jacobian,
					    libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::side_constraint,
       &GRINS::Physics::compute_side_constraint_cache);
  }

  bool MultiphysicsSystem::nonlocal_constraint( bool request_jacobian,
					        libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::nonlocal_constraint,
       &GRINS::Physics::compute_nonlocal_constraint_cache);
  }

  bool MultiphysicsSystem::mass_residual( bool request_jacobian,
					  libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::mass_residual,
       &GRINS::Physics::compute_mass_residual_cache);
  }

  bool MultiphysicsSystem::nonlocal_mass_residual( bool request_jacobian,
					           libMesh::DiffContext& context )
  {
    return this->_general_residual
      (request_jacobian,
       context,
       &GRINS::Physics::nonlocal_mass_residual,
       &GRINS::Physics::compute_nonlocal_mass_residual_cache);
  }

  std::tr1::shared_ptr<Physics> MultiphysicsSystem::get_physics( const std::string physics_name )
  {
    if( _physics_list.find( physics_name ) == _physics_list.end() )
      {
	std::cerr << "Error: Could not find physics " << physics_name << std::endl;
	libmesh_error();
      }

    return _physics_list[physics_name];
  }

  bool MultiphysicsSystem::has_physics( const std::string physics_name ) const
  {
    bool has_physics = false;

    if( _physics_list.find(physics_name) != _physics_list.end() )
      has_physics = true;

    return has_physics;
  }

  void MultiphysicsSystem::compute_postprocessed_quantity( unsigned int quantity_index,
                                                           const AssemblyContext& context,
                                                           const libMesh::Point& point,
                                                           libMesh::Real& value )
  {
    for( PhysicsListIter physics_iter = _physics_list.begin();
         physics_iter != _physics_list.end();
         physics_iter++ )
      {
        // Only compute if physics is active on current subdomain or globally
        if( (physics_iter->second)->enabled_on_elem( &context.get_elem() ) )
          {
            (physics_iter->second)->compute_postprocessed_quantity( quantity_index, context, point, value );
          }
      }
    return;
  }

#ifdef GRINS_USE_GRVY_TIMERS
  void MultiphysicsSystem::attach_grvy_timer( GRVY::GRVY_Timer_Class* grvy_timer )
  {
    _timer = grvy_timer;

    // Attach timers to each physics
    for( PhysicsListIter physics_iter = _physics_list.begin();
	 physics_iter != _physics_list.end();
	 physics_iter++ )
      {
	(physics_iter->second)->attach_grvy_timer( grvy_timer );
      }

    return;
  }
#endif


} // namespace GRINS

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
#include "grins/elastic_membrane.h"

// GRINS
#include "grins_config.h"
#include "grins/math_constants.h"
#include "grins/assembly_context.h"
#include "grins/solid_mechanics_bc_handling.h"
#include "grins/generic_ic_handler.h"
#include "grins/elasticity_tensor.h"
#include "grins/postprocessed_quantities.h"

// libMesh
#include "libmesh/getpot.h"
#include "libmesh/quadrature.h"
#include "libmesh/boundary_info.h"
#include "libmesh/fem_system.h"
#include "libmesh/fe_interface.h"

namespace GRINS
{
  template<typename StressStrainLaw>
  ElasticMembrane<StressStrainLaw>::ElasticMembrane( const GRINS::PhysicsName& physics_name, const GetPot& input,
                                                     bool is_compressible )
    : ElasticMembraneBase(physics_name,input),
      _stress_strain_law(input),
      _h0( input("Physics/"+physics_name+"/h0", 1.0 ) ),
      _is_compressible(is_compressible)
  {
    // Force the user to set h0
    if( !input.have_variable("Physics/"+physics_name+"/h0") )
      {
        std::cerr << "Error: Must specify initial thickness for "+physics_name << std::endl
                  << "       Input the option Physics/"+physics_name+"/h0" << std::endl;
        libmesh_error();
      }

    this->_bc_handler = new SolidMechanicsBCHandling( physics_name, input );

    this->_ic_handler = new GenericICHandler(physics_name, input);

    return;
  }

  template<typename StressStrainLaw>
  ElasticMembrane<StressStrainLaw>::~ElasticMembrane()
  {
    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::init_variables( libMesh::FEMSystem* system )
  {
    // First call base class
    ElasticMembraneBase::init_variables(system);

    // Now build lambda_sq variable if we need it
    if(_is_compressible)
      {
        /*! \todo Might want to make Order/FEType inputable */
        _lambda_sq_var = system->add_variable( "lambda_sq", GRINSEnums::FIRST, GRINSEnums::LAGRANGE);
      }

    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::register_postprocessing_vars( const GetPot& input,
                                                                       PostProcessedQuantities<libMesh::Real>& postprocessing )
  {
    std::string section = "Physics/"+elastic_membrane+"/output_vars";

    if( input.have_variable(section) )
      {
        unsigned int n_vars = input.vector_variable_size(section);

        for( unsigned int v = 0; v < n_vars; v++ )
          {
            std::string name = input(section,"DIE!",v);

            if( name == std::string("stress") )
              {
                // sigma_xx, sigma_xy, sigma_yy, sigma_yx = sigma_xy
                // sigma_zz = 0 by assumption of this Physics
                _stress_indices.resize(7);

                this->_stress_indices[0] = postprocessing.register_quantity("stress_xx");

                this->_stress_indices[1] = postprocessing.register_quantity("stress_xy");

                this->_stress_indices[2] = postprocessing.register_quantity("stress_yy");

                this->_stress_indices[3] = postprocessing.register_quantity("sigma_max");

                this->_stress_indices[4] = postprocessing.register_quantity("sigma_1");

                this->_stress_indices[5] = postprocessing.register_quantity("sigma_2");

                this->_stress_indices[6] = postprocessing.register_quantity("sigma_3");
              }
            else if( name == std::string( "stress_zz" ) )
              {
                // This is mostly for sanity checking the plane stress condition
                this->_stress_zz_index = postprocessing.register_quantity("stress_zz");
              }
            else if( name == std::string("strain") )
              {
                // eps_xx, eps_xy, eps_yy, eps_yx = eps_xy
                _strain_indices.resize(3);

                this->_strain_indices[0] = postprocessing.register_quantity("strain_xx");

                this->_strain_indices[1] = postprocessing.register_quantity("strain_xy");

                this->_strain_indices[2] = postprocessing.register_quantity("strain_yy");
              }
            else
              {
                std::cerr << "Error: Invalue output_vars value for "+elastic_membrane << std::endl
                          << "       Found " << name << std::endl
                          << "       Acceptable values are: stress" << std::endl
                          << "                              strain" << std::endl;
                libmesh_error();
              }
          }
      }

    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::element_time_derivative( bool compute_jacobian,
                                                                   AssemblyContext& context,
                                                                   CachedValues& /*cache*/ )
  {
    const unsigned int n_u_dofs = context.get_dof_indices(_disp_vars.u_var()).size();

    const std::vector<libMesh::Real> &JxW =
      this->get_fe(context)->get_JxW();

    // Residuals that we're populating
    libMesh::DenseSubVector<libMesh::Number> &Fu = context.get_elem_residual(_disp_vars.u_var());
    libMesh::DenseSubVector<libMesh::Number> &Fv = context.get_elem_residual(_disp_vars.v_var());
    libMesh::DenseSubVector<libMesh::Number> &Fw = context.get_elem_residual(_disp_vars.w_var());

    libMesh::DenseSubMatrix<libMesh::Number>& Kuu = context.get_elem_jacobian(_disp_vars.u_var(),_disp_vars.u_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kuv = context.get_elem_jacobian(_disp_vars.u_var(),_disp_vars.v_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kuw = context.get_elem_jacobian(_disp_vars.u_var(),_disp_vars.w_var());

    libMesh::DenseSubMatrix<libMesh::Number>& Kvu = context.get_elem_jacobian(_disp_vars.v_var(),_disp_vars.u_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kvv = context.get_elem_jacobian(_disp_vars.v_var(),_disp_vars.v_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kvw = context.get_elem_jacobian(_disp_vars.v_var(),_disp_vars.w_var());

    libMesh::DenseSubMatrix<libMesh::Number>& Kwu = context.get_elem_jacobian(_disp_vars.w_var(),_disp_vars.u_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kwv = context.get_elem_jacobian(_disp_vars.w_var(),_disp_vars.v_var());
    libMesh::DenseSubMatrix<libMesh::Number>& Kww = context.get_elem_jacobian(_disp_vars.w_var(),_disp_vars.w_var());

    unsigned int n_qpoints = context.get_element_qrule().n_points();

    // All shape function gradients are w.r.t. master element coordinates
    const std::vector<std::vector<libMesh::Real> >& dphi_dxi =
      this->get_fe(context)->get_dphidxi();

    const std::vector<std::vector<libMesh::Real> >& dphi_deta =
      this->get_fe(context)->get_dphideta();

    const libMesh::DenseSubVector<libMesh::Number>& u_coeffs = context.get_elem_solution( _disp_vars.u_var() );
    const libMesh::DenseSubVector<libMesh::Number>& v_coeffs = context.get_elem_solution( _disp_vars.v_var() );
    const libMesh::DenseSubVector<libMesh::Number>& w_coeffs = context.get_elem_solution( _disp_vars.w_var() );

    // Need these to build up the covariant and contravariant metric tensors
    const std::vector<libMesh::RealGradient>& dxdxi  = this->get_fe(context)->get_dxyzdxi();
    const std::vector<libMesh::RealGradient>& dxdeta = this->get_fe(context)->get_dxyzdeta();

    const unsigned int dim = 2; // The manifold dimension is always 2 for this physics

    for (unsigned int qp=0; qp != n_qpoints; qp++)
      {
        // Gradients are w.r.t. master element coordinates
        libMesh::Gradient grad_u, grad_v, grad_w;
        for( unsigned int d = 0; d < n_u_dofs; d++ )
          {
            libMesh::RealGradient u_gradphi( dphi_dxi[d][qp], dphi_deta[d][qp] );
            grad_u += u_coeffs(d)*u_gradphi;
            grad_v += v_coeffs(d)*u_gradphi;
            grad_w += w_coeffs(d)*u_gradphi;
          }

        libMesh::RealGradient grad_x( dxdxi[qp](0), dxdeta[qp](0) );
        libMesh::RealGradient grad_y( dxdxi[qp](1), dxdeta[qp](1) );
        libMesh::RealGradient grad_z( dxdxi[qp](2), dxdeta[qp](2) );


        libMesh::TensorValue<libMesh::Real> a_cov, a_contra, A_cov, A_contra;
        libMesh::Real lambda_sq = 0;

        this->compute_metric_tensors( qp, *(this->get_fe(context)), context,
                                      grad_u, grad_v, grad_w,
                                      a_cov, a_contra, A_cov, A_contra,
                                      lambda_sq );

        // Compute stress and elasticity tensors
        libMesh::TensorValue<libMesh::Real> tau;
        ElasticityTensor C;
        _stress_strain_law.compute_stress_and_elasticity(dim,a_contra,a_cov,A_contra,A_cov,tau,C);

        libMesh::Real jac = JxW[qp];

        for (unsigned int i=0; i != n_u_dofs; i++)
	  {
            libMesh::RealGradient u_gradphi( dphi_dxi[i][qp], dphi_deta[i][qp] );

            for( unsigned int alpha = 0; alpha < dim; alpha++ )
              {
                for( unsigned int beta = 0; beta < dim; beta++ )
                  {
                    Fu(i) -= 0.5*tau(alpha,beta)*_h0*( (grad_x(beta) + grad_u(beta))*u_gradphi(alpha) +
                                                       (grad_x(alpha) + grad_u(alpha))*u_gradphi(beta) )*jac;

                    Fv(i) -= 0.5*tau(alpha,beta)*_h0*( (grad_y(beta) + grad_v(beta))*u_gradphi(alpha) +
                                                       (grad_y(alpha) + grad_v(alpha))*u_gradphi(beta) )*jac;

                    Fw(i) -= 0.5*tau(alpha,beta)*_h0*( (grad_z(beta) + grad_w(beta))*u_gradphi(alpha) +
                                                       (grad_z(alpha) + grad_w(alpha))*u_gradphi(beta) )*jac;
                  }
              }
          }

        if( compute_jacobian )
          {
            for (unsigned int i=0; i != n_u_dofs; i++)
              {
                libMesh::RealGradient u_gradphi_i( dphi_dxi[i][qp], dphi_deta[i][qp] );

                for (unsigned int j=0; j != n_u_dofs; j++)
                  {
                    libMesh::RealGradient u_gradphi_j( dphi_dxi[j][qp], dphi_deta[j][qp] );

                    for( unsigned int alpha = 0; alpha < dim; alpha++ )
                      {
                        for( unsigned int beta = 0; beta < dim; beta++ )
                          {
                            const libMesh::Real diag_term = 0.5*_h0*jac*tau(alpha,beta)*( u_gradphi_j(beta)*u_gradphi_i(alpha) +
                                                                                          u_gradphi_j(alpha)*u_gradphi_i(beta) );
                            Kuu(i,j) -= diag_term;

                            Kvv(i,j) -= diag_term;

                            Kww(i,j) -= diag_term;

                            for( unsigned int lambda = 0; lambda < dim; lambda++ )
                              {
                                for( unsigned int mu = 0; mu < dim; mu++ )
                                  {
                                    const libMesh::Real dgamma_du = 0.5*( u_gradphi_j(lambda)*(grad_x(mu)+grad_u(mu)) +
                                                                          (grad_x(lambda)+grad_u(lambda))*u_gradphi_j(mu) );

                                    const libMesh::Real dgamma_dv = 0.5*( u_gradphi_j(lambda)*(grad_y(mu)+grad_v(mu)) +
                                                                          (grad_y(lambda)+grad_v(lambda))*u_gradphi_j(mu) );

                                    const libMesh::Real dgamma_dw = 0.5*( u_gradphi_j(lambda)*(grad_z(mu)+grad_w(mu)) +
                                                                          (grad_z(lambda)+grad_w(lambda))*u_gradphi_j(mu) );

                                    const libMesh::Real C1 = 0.5*_h0*jac*C(alpha,beta,lambda,mu);

                                    const libMesh::Real x_term = C1*( (grad_x(beta)+grad_u(beta))*u_gradphi_i(alpha) +
                                                                      (grad_x(alpha)+grad_u(alpha))*u_gradphi_i(beta) );

                                    const libMesh::Real y_term = C1*( (grad_y(beta)+grad_v(beta))*u_gradphi_i(alpha) +
                                                                      (grad_y(alpha)+grad_v(alpha))*u_gradphi_i(beta) );

                                    const libMesh::Real z_term = C1*( (grad_z(beta)+grad_w(beta))*u_gradphi_i(alpha) +
                                                                      (grad_z(alpha)+grad_w(alpha))*u_gradphi_i(beta) );

                                    Kuu(i,j) -= x_term*dgamma_du;

                                    Kuv(i,j) -= x_term*dgamma_dv;

                                    Kuw(i,j) -= x_term*dgamma_dw;

                                    Kvu(i,j) -= y_term*dgamma_du;

                                    Kvv(i,j) -= y_term*dgamma_dv;

                                    Kvw(i,j) -= y_term*dgamma_dw;

                                    Kwu(i,j) -= z_term*dgamma_du;

                                    Kwv(i,j) -= z_term*dgamma_dv;

                                    Kww(i,j) -= z_term*dgamma_dw;
                                  }
                              }
                          }
                      }
                  }
              }
          }

      }

    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::element_constraint( bool compute_jacobian,
                                                             AssemblyContext& context,
                                                             CachedValues& /*cache*/ )
  {
    // Only compute the constraint is tracking lambda_sq as an independent variable
    if( _is_compressible )
      {
        unsigned int n_qpoints = context.get_element_qrule().n_points();

        const unsigned int n_u_dofs = context.get_dof_indices(_disp_vars.u_var()).size();

        const std::vector<libMesh::Real> &JxW = context.get_element_fe(this->_lambda_sq_var)->get_JxW();

        libMesh::DenseSubVector<libMesh::Number>& Fl = context.get_elem_residual(this->_lambda_sq_var);

        const std::vector<std::vector<libMesh::Real> >& phi =
          context.get_element_fe(this->_lambda_sq_var)->get_phi();

        const unsigned int n_lambda_sq_dofs = context.get_dof_indices(this->_lambda_sq_var).size();

        const libMesh::DenseSubVector<libMesh::Number>& u_coeffs = context.get_elem_solution( _disp_vars.u_var() );
        const libMesh::DenseSubVector<libMesh::Number>& v_coeffs = context.get_elem_solution( _disp_vars.v_var() );
        const libMesh::DenseSubVector<libMesh::Number>& w_coeffs = context.get_elem_solution( _disp_vars.w_var() );

        // All shape function gradients are w.r.t. master element coordinates
        const std::vector<std::vector<libMesh::Real> >& dphi_dxi =
          this->get_fe(context)->get_dphidxi();

        const std::vector<std::vector<libMesh::Real> >& dphi_deta =
          this->get_fe(context)->get_dphideta();

        for (unsigned int qp=0; qp != n_qpoints; qp++)
          {
            libMesh::Real jac = JxW[qp];

            libMesh::Gradient grad_u, grad_v, grad_w;
            for( unsigned int d = 0; d < n_u_dofs; d++ )
              {
                libMesh::RealGradient u_gradphi( dphi_dxi[d][qp], dphi_deta[d][qp] );
                grad_u += u_coeffs(d)*u_gradphi;
                grad_v += v_coeffs(d)*u_gradphi;
                grad_w += w_coeffs(d)*u_gradphi;
              }

            libMesh::TensorValue<libMesh::Real> a_cov, a_contra, A_cov, A_contra;
            libMesh::Real lambda_sq = 0;

            this->compute_metric_tensors( qp, *(this->get_fe(context)), context,
                                          grad_u, grad_v, grad_w,
                                          a_cov, a_contra, A_cov, A_contra,
                                          lambda_sq );

            libMesh::Real stress_33 = _stress_strain_law.compute_33_stress( a_contra, a_cov, A_contra, A_cov );

            for (unsigned int i=0; i != n_lambda_sq_dofs; i++)
              {
                Fl(i) += stress_33*phi[i][qp]*jac;
              }

            if( compute_jacobian )
              {
                libmesh_not_implemented();
              }
          }
      } // is_compressible

    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::side_time_derivative( bool /*compute_jacobian*/,
                                                               AssemblyContext& /*context*/,
                                                               CachedValues& /*cache*/ )
  {
    /*
      std::vector<BoundaryID> ids = context.side_boundary_ids();

      for( std::vector<BoundaryID>::const_iterator it = ids.begin();
      it != ids.end(); it++ )
      {
      libmesh_assert (*it != libMesh::BoundaryInfo::invalid_id);

      _bc_handler->apply_neumann_bcs( context, cache, compute_jacobian, *it );
      }
    */

    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::mass_residual( bool /*compute_jacobian*/,
                                                        AssemblyContext& /*context*/,
                                                        CachedValues& /*cache*/ )
  {
    libmesh_not_implemented();
    return;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::compute_postprocessed_quantity( unsigned int quantity_index,
                                                                         const AssemblyContext& context,
                                                                         const libMesh::Point& point,
                                                                         libMesh::Real& value )
  {
    bool is_stress = false;
    if( !_stress_indices.empty() )
      is_stress= ( _stress_indices[0] == quantity_index ||
                   _stress_indices[1] == quantity_index ||
                   _stress_indices[2] == quantity_index ||
                   _stress_indices[3] == quantity_index ||
                   _stress_indices[4] == quantity_index ||
                   _stress_indices[5] == quantity_index ||
                   _stress_indices[6] == quantity_index ||
                   _stress_zz_index == quantity_index   );

    bool is_strain = false;
    if( !_strain_indices.empty() )
      is_strain = ( _strain_indices[0] == quantity_index ||
                    _strain_indices[1] == quantity_index ||
                    _strain_indices[2] == quantity_index   );

    if( is_stress || is_strain )
      {
        const unsigned int n_u_dofs = context.get_dof_indices(_disp_vars.u_var()).size();

        const libMesh::DenseSubVector<libMesh::Number>& u_coeffs = context.get_elem_solution( _disp_vars.u_var() );
        const libMesh::DenseSubVector<libMesh::Number>& v_coeffs = context.get_elem_solution( _disp_vars.v_var() );
        const libMesh::DenseSubVector<libMesh::Number>& w_coeffs = context.get_elem_solution( _disp_vars.w_var() );

        // Build new FE for the current point. We need this to build tensors at point.
        libMesh::AutoPtr<libMesh::FEGenericBase<libMesh::Real> > fe_new =
          this->build_new_fe( context.get_elem(), this->get_fe(context),
                              point );

        const std::vector<std::vector<libMesh::Real> >& dphi_dxi =
          fe_new->get_dphidxi();

        const std::vector<std::vector<libMesh::Real> >& dphi_deta =
          fe_new->get_dphideta();

        // Need these to build up the covariant and contravariant metric tensors
        const std::vector<libMesh::RealGradient>& dxdxi  = fe_new->get_dxyzdxi();
        const std::vector<libMesh::RealGradient>& dxdeta = fe_new->get_dxyzdeta();

        // Gradients are w.r.t. master element coordinates
        libMesh::Gradient grad_u, grad_v, grad_w;
        for( unsigned int d = 0; d < n_u_dofs; d++ )
          {
            libMesh::RealGradient u_gradphi( dphi_dxi[d][0], dphi_deta[d][0] );
            grad_u += u_coeffs(d)*u_gradphi;
            grad_v += v_coeffs(d)*u_gradphi;
            grad_w += w_coeffs(d)*u_gradphi;
          }

        libMesh::RealGradient grad_x( dxdxi[0](0), dxdeta[0](0) );
        libMesh::RealGradient grad_y( dxdxi[0](1), dxdeta[0](1) );
        libMesh::RealGradient grad_z( dxdxi[0](2), dxdeta[0](2) );

        libMesh::TensorValue<libMesh::Real> a_cov, a_contra, A_cov, A_contra;
        libMesh::Real lambda_sq = 0;

        // We're only computing one point at a time, so qp = 0 always
        this->compute_metric_tensors(0, *fe_new, context, grad_u, grad_v, grad_w,
                                     a_cov, a_contra, A_cov, A_contra, lambda_sq );

        // We have everything we need for strain now, so check if we are computing strain
        if( is_strain )
          {
            if( _strain_indices[0] == quantity_index )
              {
                value = 0.5*(A_cov(0,0) - a_cov(0,0));
              }
            else if( _strain_indices[1] == quantity_index )
              {
                value = 0.5*(A_cov(0,1) - a_cov(0,1));
              }
            else if( _strain_indices[2] == quantity_index )
              {
                value = 0.5*(A_cov(1,1) - a_cov(1,1));
              }
            else
              {
                //Wat?!
                libmesh_error();
              }
            return;
          }

        if( is_stress )
          {
            libMesh::Real det_a = a_cov(0,0)*a_cov(1,1) - a_cov(0,1)*a_cov(1,0);
            libMesh::Real det_A = A_cov(0,0)*A_cov(1,1) - A_cov(0,1)*A_cov(1,0);

            libMesh::Real I3 = lambda_sq*det_A/det_a;

            libMesh::TensorValue<libMesh::Real> tau;
            _stress_strain_law.compute_stress(2,a_contra,a_cov,A_contra,A_cov,tau);

            if( _stress_indices[0] == quantity_index )
              {
                // Need to convert to Cauchy stress
                value = tau(0,0)/std::sqrt(I3);
              }
            else if( _stress_indices[1] == quantity_index )
              {
                // Need to convert to Cauchy stress
                value = tau(0,1)/std::sqrt(I3);
              }
            else if( _stress_indices[2] == quantity_index )
              {
                // Need to convert to Cauchy stress
                value = tau(1,1)/std::sqrt(I3);
              }
            else if( _stress_indices[3] == quantity_index )
              {
                value = 0.5*(tau(0,0) + tau(1,1)) + std::sqrt(0.25*(tau(0,0)-tau(1,1))*(tau(0,0)-tau(1,1))
                                                              + tau(0,1)*tau(0,1) );
              }
            else if( _stress_indices[4] == quantity_index ||
                     _stress_indices[5] == quantity_index ||
                     _stress_indices[6] == quantity_index   )
              {
                if(_is_compressible)
                  {
                    tau(2,2) = _stress_strain_law.compute_33_stress( a_contra, a_cov, A_contra, A_cov );
                  }

                libMesh::Real stress_I1 = tau(0,0) + tau(1,1) + tau(2,2);
                libMesh::Real stress_I2 = 0.5*(stress_I1*stress_I1 - (tau(0,0)*tau(0,0) + tau(1,1)*tau(1,1)
                                                                      + tau(2,2)*tau(2,2) + tau(0,1)*tau(0,1)
                                                                      + tau(1,0)*tau(1,0)) );

                libMesh::Real stress_I3 = tau(2,2)*( tau(0,0)*tau(1,1) - tau(1,0)*tau(0,1) );

                /* Formulae for principal stresses from:
                   http://en.wikiversity.org/wiki/Principal_stresses */

                // I_2^2 - 3*I_2
                libMesh::Real C1 = (stress_I1*stress_I1 - 3*stress_I2);

                // 2*I_1^3 - 9*I_1*_I2 + 27*I_3
                libMesh::Real C2 = (2*stress_I1*stress_I1*stress_I1 - 9*stress_I1*stress_I2 + 27*stress_I3)/54;

                libMesh::Real theta = std::acos( C2/(2*std::sqrt(C1*C1*C1)) )/3.0;

                if( _stress_indices[4] == quantity_index )
                  {
                    value = (stress_I1 + 2.0*std::sqrt(C1)*std::cos(theta))/3.0;
                  }

                if( _stress_indices[5] == quantity_index )
                  {
                    value = (stress_I1 + 2.0*std::sqrt(C1)*std::cos(theta+Constants::two_pi/3.0))/3.0;
                  }

                if( _stress_indices[6] == quantity_index )
                  {
                    value = (stress_I1 + 2.0*std::sqrt(C1)*std::cos(theta+2.0*Constants::two_pi/3.0))/3.0;
                  }
              }
            else if( _stress_zz_index == quantity_index )
              {
                value = _stress_strain_law.compute_33_stress( a_contra, a_cov, A_contra, A_cov );
              }
            else
              {
                //Wat?!
                libmesh_error();
              }
          } // is_stress

      }

    return;
  }

  template<typename StressStrainLaw>
  libMesh::AutoPtr<libMesh::FEGenericBase<libMesh::Real> > ElasticMembrane<StressStrainLaw>::build_new_fe( const libMesh::Elem& elem,
                                                                                                           const libMesh::FEGenericBase<libMesh::Real>* fe,
                                                                                                           const libMesh::Point p )
  {
    using namespace libMesh;
    FEType fe_type = fe->get_fe_type();

    // If we don't have an Elem to evaluate on, then the only functions
    // we can sensibly evaluate are the scalar dofs which are the same
    // everywhere.
    libmesh_assert(&elem || fe_type.family == SCALAR);

    unsigned int elem_dim = &elem ? elem.dim() : 0;

    AutoPtr<FEGenericBase<libMesh::Real> >
      fe_new(FEGenericBase<libMesh::Real>::build(elem_dim, fe_type));

    // Map the physical co-ordinates to the master co-ordinates using the inverse_map from fe_interface.h
    // Build a vector of point co-ordinates to send to reinit
    Point master_point = &elem ?
      FEInterface::inverse_map(elem_dim, fe_type, &elem, p) :
      Point(0);

    std::vector<Point> coor(1, master_point);

    // Reinitialize the element and compute the shape function values at coor
    fe_new->reinit (&elem, &coor);

    return fe_new;
  }

  template<typename StressStrainLaw>
  void ElasticMembrane<StressStrainLaw>::compute_metric_tensors( unsigned int qp,
                                                                 const libMesh::FEBase& elem,
                                                                 const AssemblyContext& context,
                                                                 const libMesh::Gradient& grad_u,
                                                                 const libMesh::Gradient& grad_v,
                                                                 const libMesh::Gradient& grad_w,
                                                                 libMesh::TensorValue<libMesh::Real>& a_cov,
                                                                 libMesh::TensorValue<libMesh::Real>& a_contra,
                                                                 libMesh::TensorValue<libMesh::Real>& A_cov,
                                                                 libMesh::TensorValue<libMesh::Real>& A_contra,
                                                                 libMesh::Real& lambda_sq )
  {
    const std::vector<libMesh::RealGradient>& dxdxi  = elem.get_dxyzdxi();
    const std::vector<libMesh::RealGradient>& dxdeta = elem.get_dxyzdeta();

    const std::vector<libMesh::Real>& dxidx  = elem.get_dxidx();
    const std::vector<libMesh::Real>& dxidy  = elem.get_dxidy();
    const std::vector<libMesh::Real>& dxidz  = elem.get_dxidz();

    const std::vector<libMesh::Real>& detadx  = elem.get_detadx();
    const std::vector<libMesh::Real>& detady  = elem.get_detady();
    const std::vector<libMesh::Real>& detadz  = elem.get_detadz();

    libMesh::RealGradient dxi( dxidx[qp], dxidy[qp], dxidz[qp] );
    libMesh::RealGradient deta( detadx[qp], detady[qp], detadz[qp] );

    libMesh::RealGradient dudxi( grad_u(0), grad_v(0), grad_w(0) );
    libMesh::RealGradient dudeta( grad_u(1), grad_v(1), grad_w(1) );

    // Covariant metric tensor of reference configuration
    a_cov.zero();
    a_cov(0,0) = dxdxi[qp]*dxdxi[qp];
    a_cov(0,1) = dxdxi[qp]*dxdeta[qp];
    a_cov(1,0) = dxdeta[qp]*dxdxi[qp];
    a_cov(1,1) = dxdeta[qp]*dxdeta[qp];

    libMesh::Real det_a = a_cov(0,0)*a_cov(1,1) - a_cov(0,1)*a_cov(1,0);

    // Covariant metric tensor of current configuration
    A_cov.zero();
    A_cov(0,0) = (dxdxi[qp] + dudxi)*(dxdxi[qp] + dudxi);
    A_cov(0,1) = (dxdxi[qp] + dudxi)*(dxdeta[qp] + dudeta);
    A_cov(1,0) = (dxdeta[qp] + dudeta)*(dxdxi[qp] + dudxi);
    A_cov(1,1) = (dxdeta[qp] + dudeta)*(dxdeta[qp] + dudeta);

    // Contravariant metric tensor of reference configuration
    a_contra.zero();
    a_contra(0,0) = dxi*dxi;
    a_contra(0,1) = dxi*deta;
    a_contra(1,0) = deta*dxi;
    a_contra(1,1) = deta*deta;

    // Contravariant metric tensor in current configuration is A_cov^{-1}
    libMesh::Real det_A = A_cov(0,0)*A_cov(1,1) - A_cov(0,1)*A_cov(1,0);

    A_contra.zero();
    A_contra(0,0) =  A_cov(1,1)/det_A;
    A_contra(0,1) = -A_cov(0,1)/det_A;
    A_contra(1,0) = -A_cov(1,0)/det_A;
    A_contra(1,1) =  A_cov(0,0)/det_A;

    a_cov(2,2)    = 1.0;
    a_contra(2,2) = 1.0;


    // If the material is compressible, then lambda_sq is an independent variable
    if( _is_compressible )
      {
        lambda_sq = context.interior_value(this->_lambda_sq_var, qp);
      }
    else
      {
        // If the material is incompressible, lambda^2 is known
        lambda_sq = det_a/det_A;
      }

    A_cov(2,2) = lambda_sq;
    A_contra(2,2) = 1.0/lambda_sq;

    return;
  }

} // end namespace GRINS

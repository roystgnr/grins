//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
// 
// GRINS - General Reacting Incompressible Navier-Stokes 
//
// Copyright (C) 2010-2012 The PECOS Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the Version 2 GNU General
// Public License as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301 USA
//
//-----------------------------------------------------------------------el-
//
// $Id$
//
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

#include "inc_navier_stokes_stab_helper.h"

GRINS::IncompressibleNavierStokesStabilizationHelper::IncompressibleNavierStokesStabilizationHelper(const GetPot& input)
  : StabilizationHelper(),
    _C( input("Stabilization/tau_constant", 1 ) ),
    _tau_factor( input("Stabilization/tau_factor", 0.5 ) )
{
  return;
}

GRINS::IncompressibleNavierStokesStabilizationHelper::~IncompressibleNavierStokesStabilizationHelper()
{
  return;
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::UdotGradU( libMesh::Gradient& U, 
										       libMesh::Gradient& grad_u, 
										       libMesh::Gradient& grad_v ) const
{
  return libMesh::RealGradient( U*grad_u, U*grad_v );
}
    
libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::UdotGradU( libMesh::Gradient& U, 
										       libMesh::Gradient& grad_u, 
										       libMesh::Gradient& grad_v, 
										       libMesh::Gradient& grad_w ) const
{
  return libMesh::RealGradient( U*grad_u, U*grad_v, U*grad_w );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_GradU( libMesh::RealTensor& hess_u, 
										       libMesh::RealTensor& hess_v ) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_u(1,1),
				hess_v(0,0) + hess_v(1,1) );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_GradU( libMesh::RealTensor& hess_u, 
										       libMesh::RealTensor& hess_v,
										       libMesh::RealTensor& hess_w ) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_u(1,1) + hess_u(2,2),
				hess_v(0,0) + hess_v(1,1) + hess_v(2,2),
				hess_w(0,0) + hess_w(1,1) + hess_w(2,2) );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_GradU_T( libMesh::RealTensor& hess_u, 
											 libMesh::RealTensor& hess_v ) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_v(0,1),
				hess_u(1,0) + hess_v(1,1) );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_GradU_T( libMesh::RealTensor& hess_u, 
											 libMesh::RealTensor& hess_v,
											 libMesh::RealTensor& hess_w ) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_v(0,1) + hess_w(0,2),
				hess_u(1,0) + hess_v(1,1) + hess_w(1,2),
				hess_u(2,0) + hess_v(2,1) + hess_w(2,2) );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_divU_I( libMesh::RealTensor& hess_u, 
											libMesh::RealTensor& hess_v ) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_v(1,0),
				hess_u(0,1) + hess_v(1,1) );
}

libMesh::RealGradient GRINS::IncompressibleNavierStokesStabilizationHelper::div_divU_I( libMesh::RealTensor& hess_u,
											libMesh::RealTensor& hess_v,
											libMesh::RealTensor& hess_w) const
{
  return libMesh::RealGradient( hess_u(0,0) + hess_v(1,0) + hess_w(2,0),
				hess_u(0,1) + hess_v(1,1) + hess_w(2,1),
				hess_u(0,2) + hess_v(1,2) + hess_w(2,2) );
}
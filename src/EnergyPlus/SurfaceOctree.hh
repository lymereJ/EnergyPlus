#ifndef EnergyPlus_SurfaceOctree_hh_INCLUDED
#define EnergyPlus_SurfaceOctree_hh_INCLUDED

// EnergyPlus Headers
#include <EnergyPlus/EnergyPlus.hh>

// ObjexxFCL Headers
#include <ObjexxFCL/Array1.fwd.hh>
#include <ObjexxFCL/Vector3.hh>

// C++ Headers
#include <cassert>
#include <cstdint>
#include <vector>

namespace EnergyPlus {

// Forward
namespace DataSurfaces { struct SurfaceData; }

namespace Octree {

// Package: Surface Octree System
//
// Author: Stuart Mentzer (Stuart_Mentzer@objexx.com)
//
// Purpose: Spatial sort of surfaces for fast, scalable identification of active surfaces
// for some algorithms such as solar shading, solar reflection, and daylighting obstruction
//
// See the .cc file for details

class SurfaceOctreeCube
{

public: // Types

	using Real = Real64;
	using Surface = DataSurfaces::SurfaceData;
	using Vertex = ObjexxFCL::Vector3< Real >;
	using Surfaces = std::vector< Surface * >;

public: // Creation

	// Default Constructor
	SurfaceOctreeCube()
	{
#if defined(_MSC_VER) && _MSC_VER < 1900 && !defined(__INTEL_COMPILER) // Visual C++ 2013 doesn't support array initializers in member declaration
		SurfaceOctreeCube * * p = &cubes_[ 0 ][ 0 ][ 0 ]; for ( int i = 0; i < 8; ++i ) p[ i ] = nullptr;
#endif
	}

	// Surfaces Outer Cube Constructor
	SurfaceOctreeCube( ObjexxFCL::Array1< Surface > & surfaces )
	{
#if defined(_MSC_VER) && _MSC_VER < 1900 && !defined(__INTEL_COMPILER) // Visual C++ 2013 doesn't support array initializers in member declaration
		SurfaceOctreeCube * * p = &cubes_[ 0 ][ 0 ][ 0 ]; for ( int i = 0; i < 8; ++i ) p[ i ] = nullptr;
#endif
		init( surfaces );
	}

	// Box Constructor
	SurfaceOctreeCube(
	 std::uint16_t const depth,
	 Vertex const & l,
	 Vertex const & u,
	 Real const w
	) :
	 depth_( depth ),
	 l_( l ),
	 u_( u ),
	 c_( cen( l, u ) ),
	 w_( w )
	{
#if defined(_MSC_VER) && _MSC_VER < 1900 && !defined(__INTEL_COMPILER) // Visual C++ 2013 doesn't support array initializers in member declaration
		SurfaceOctreeCube * * p = &cubes_[ 0 ][ 0 ][ 0 ]; for ( int i = 0; i < 8; ++i ) p[ i ] = nullptr;
#endif
		assert( valid() );
	}

	// Destructor
	~SurfaceOctreeCube()
	{
		SurfaceOctreeCube * * p = &cubes_[ 0 ][ 0 ][ 0 ];
		for ( int i = 0; i < 8; ++i ) {
			if ( p[ i ] ) delete p[ i ];
		}
	}

public: // Properties

	// Depth
	std::uint16_t
	depth() const
	{
		return depth_;
	}

	// Lower Corner
	Vertex const &
	l() const
	{
		return l_;
	}

	// Upper Corner
	Vertex const &
	u() const
	{
		return u_;
	}

	// Center Point
	Vertex const &
	c() const
	{
		return c_;
	}

	// Width
	Real
	w() const
	{
		return w_;
	}

	// Surfaces
	Surfaces const &
	surfaces() const
	{
		return surfaces_;
	}

	// Surfaces
	Surfaces::size_type
	surfaces_size() const
	{
		return surfaces_.size();
	}

	// Surfaces Begin Iterator
	Surfaces::const_iterator
	surfaces_begin() const
	{
		return surfaces_.begin();
	}

	// Surfaces Begin Iterator
	Surfaces::iterator
	surfaces_begin()
	{
		return surfaces_.begin();
	}

	// Surfaces End Iterator
	Surfaces::const_iterator
	surfaces_end() const
	{
		return surfaces_.end();
	}

	// Surfaces End Iterator
	Surfaces::iterator
	surfaces_end()
	{
		return surfaces_.end();
	}

public: // Predicates

	// Vertex in Cube?
	bool
	contains( Vertex const & v ) const
	{
		return ( l_.x <= v.x ) && ( v.x <= u_.x ) && ( l_.y <= v.y ) && ( v.y <= u_.y ) && ( l_.z <= v.z ) && ( v.z <= u_.z );
	}

	// Surface in Cube?
	bool
	contains( Surface const & surface ) const;

public: // Methods

	// Surfaces Outer Cube Initilization
	void
	init( ObjexxFCL::Array1< Surface > & surfaces );

private: // Methods

	// Valid?
	bool
	valid() const;

	// Add a Surface
	void
	add( Surface & surface )
	{
		surfaces_.push_back( &surface );
	}

	// Branch to Sub-Tree
	void
	branch();

	// Surface Branch Processing
	void
	surface_branch( Surface & surface );

private: // Static Methods

	// Vertex in Cube?
	static
	bool
	contains(
	 Vertex const & l,
	 Vertex const & u,
	 Vertex const & v
	)
	{
		return ( l.x <= v.x ) && ( v.x <= u.x ) && ( l.y <= v.y ) && ( v.y <= u.y ) && ( l.z <= v.z ) && ( v.z <= u.z );
	}

	// Surface in Cube?
	static
	bool
	contains(
	 Vertex const & l,
	 Vertex const & u,
	 Surface const & surface
	);

private: // Data

	static std::uint16_t const maxDepth_ = 65535u; // Max tree depth //Do Tune this
	static std::uint8_t const maxSurfaces_ = 10u; // Max surfaces in a cube before subdivision is processed //Do Tune this

	std::uint16_t depth_ = 0u; // Depth in tree
	Vertex l_ = Vertex( 0.0 ); // Lower corner
	Vertex u_ = Vertex( 0.0 ); // Upper corner
	Vertex c_ = Vertex( 0.0 ); // Center point
	Real w_ = 0.0; // Side width
#if defined(_MSC_VER) && _MSC_VER < 1900 && !defined(__INTEL_COMPILER) // Visual C++ 2013 doesn't support array initializers in member declaration
	SurfaceOctreeCube * cubes_[ 2 ][ 2 ][ 2 ]; // Children (nullptrs if not present)
#else
	SurfaceOctreeCube * cubes_[ 2 ][ 2 ][ 2 ] = { { { nullptr, nullptr }, { nullptr, nullptr } },{ { nullptr, nullptr }, { nullptr, nullptr } } }; // Children (nullptrs if not present)
#endif
	Surfaces surfaces_; // Surfaces

}; // SurfaceOctreeCube

// Globals
extern SurfaceOctreeCube surfaceOctree;

} // Octree
} // EnergyPlus

//=================================================================================
// NOTICE
//
// Copyright (c) 1996-2015 The Board of Trustees of the University of Illinois
// and The Regents of the University of California through Ernest Orlando Lawrence
// Berkeley National Laboratory. All rights reserved.
//
// Portions of the EnergyPlus software package have been developed and copyrighted
// by other individuals, companies and institutions. These portions have been
// incorporated into the EnergyPlus software package under license. For a complete
// list of contributors, see "Notice" located in main.cc.
//
// NOTICE: The U.S. Government is granted for itself and others acting on its
// behalf a paid-up, nonexclusive, irrevocable, worldwide license in this data to
// reproduce, prepare derivative works, and perform publicly and display publicly.
// Beginning five (5) years after permission to assert copyright is granted,
// subject to two possible five year renewals, the U.S. Government is granted for
// itself and others acting on its behalf a paid-up, non-exclusive, irrevocable
// worldwide license in this data to reproduce, prepare derivative works,
// distribute copies to the public, perform publicly and display publicly, and to
// permit others to do so.
//
// TRADEMARKS: EnergyPlus is a trademark of the US Department of Energy.
//=================================================================================

#endif
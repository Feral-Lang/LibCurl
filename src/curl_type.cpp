/*
	Copyright (c) 2020, Electrux
	All rights reserved.
	Using the BSD 3-Clause license for the project,
	main LICENSE file resides in project's root directory.
	Please read that file and understand the license terms
	before using or altering the project.
*/

#include "../include/curl_type.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// CURL class //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

var_curl_t::var_curl_t( CURL * const val, const size_t & src_id, const size_t & idx, const bool owner )
	: var_base_t( type_id< var_curl_t >(), src_id, idx, false, false ), m_val( val ), m_owner( owner )
{}
var_curl_t::~var_curl_t()
{
	if( m_owner && m_val ) curl_easy_cleanup( m_val );
}

var_base_t * var_curl_t::copy( const size_t & src_id, const size_t & idx )
{
	return new var_curl_t( m_val, src_id, idx, false );
}

void var_curl_t::set( var_base_t * from )
{
	if( m_owner && m_val ) curl_easy_cleanup( m_val );
	m_owner = false;
	m_val = CURL( from )->get();
}

CURL * const var_curl_t::get() { return m_val; }

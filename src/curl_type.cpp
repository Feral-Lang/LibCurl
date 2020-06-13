/*
	MIT License

	Copyright (c) 2020 Feral Language repositories

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so.
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

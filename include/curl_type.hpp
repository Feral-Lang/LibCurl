/*
	Copyright (c) 2020, Electrux
	All rights reserved.
	Using the GNU GPL 3.0 license for the project,
	main LICENSE file resides in project's root directory.
	Please read that file and understand the license terms
	before using or altering the project.
*/

#ifndef CURL_HPP
#define CURL_HPP

#include <curl/curl.h>
#include <feral/VM/VM.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// CURL class //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class var_curl_t : public var_base_t
{
	CURL * m_val;
	bool m_owner;
public:
	var_curl_t( CURL * const val, const size_t & src_id, const size_t & idx, const bool owner = true );
	~var_curl_t();

	var_base_t * copy( const size_t & src_id, const size_t & idx );
	void set( var_base_t * from );

	CURL * const get();
};
#define CURL( x ) static_cast< var_curl_t * >( x )

#endif // CURL_HPP
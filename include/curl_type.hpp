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

#ifndef FERAL_LIB_CURL_TYPE_HPP
#define FERAL_LIB_CURL_TYPE_HPP

#include <curl/curl.h>
#include <feral/VM/VM.hpp>

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VAR_CURL /////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

class var_curl_t : public var_base_t
{
	CURL *m_val;
	bool m_owner;

public:
	var_curl_t(CURL *const val, const size_t &src_id, const size_t &idx,
		   const bool owner = true);
	~var_curl_t();

	var_base_t *copy(const size_t &src_id, const size_t &idx);
	void set(var_base_t *from);

	CURL *const get();
};
#define CURL(x) static_cast<var_curl_t *>(x)

#endif // FERAL_LIB_CURL_TYPE_HPP
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

#ifndef FERAL_LIB_CURL_HPP
#define FERAL_LIB_CURL_HPP

#include <curl/curl.h>
#include <feral/VM/VM.hpp>

#include "curl_type.hpp"

// for passing to various callbacks
struct curl_vm_data_t
{
	vm_state_t *vm;
	size_t src_id;
	size_t idx;
};

extern var_base_t *progress_callback;
extern std::vector<curl_slist *> hss;

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curl_progress_func(void *ptr, curl_off_t to_download, curl_off_t downloaded,
		       curl_off_t to_upload, curl_off_t uploaded);

var_base_t *feral_curl_easy_set_opt_native(vm_state_t &vm, const fn_data_t &fd);

// set some default values
var_base_t *feral_curl_set_default_progress_func(vm_state_t &vm, const fn_data_t &fd);
var_base_t *feral_curl_set_default_progress_func_tick(vm_state_t &vm, const fn_data_t &fd);

#endif // FERAL_LIB_CURL_HPP
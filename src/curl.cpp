/*
	Copyright (c) 2020, Electrux
	All rights reserved.
	Using the BSD 3-Clause license for the project,
	main LICENSE file resides in project's root directory.
	Please read that file and understand the license terms
	before using or altering the project.
*/

#include "curl.hpp"

static curl_vm_data_t curl_vm_data = { nullptr, 0, 0 };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// CURL class //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// initialize this in the init_curl function
static int curl_typeid;

var_curl_t::var_curl_t( CURL * const val, const size_t & src_id, const size_t & idx, const bool owner )
	: var_base_t( curl_typeid, src_id, idx ), m_val( val ), m_owner( owner )
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// CURL functions ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

var_base_t * feral_curl_easy_init( vm_state_t & vm, const fn_data_t & fd )
{
	srcfile_t * src = vm.src_stack.back()->src();
	CURL * curl = curl_easy_init();
	if( !curl ) {
		src->fail( fd.idx, "failed to run curl_easy_init()" );
		return nullptr;
	}
	curl_vm_data.vm = & vm;
	// set all required easy_opts (especially callbacks)
	curl_easy_setopt( curl, CURLOPT_XFERINFOFUNCTION, curl_progress_func );
	curl_easy_setopt( curl, CURLOPT_XFERINFODATA, & curl_vm_data );
	return make< var_curl_t >( curl );
}

var_base_t * feral_curl_easy_perform( vm_state_t & vm, const fn_data_t & fd )
{
	CURL * curl = CURL( fd.args[ 0 ] )->get();
	srcfile_t * src_file = vm.src_stack.back()->src();

	curl_vm_data.src_id = fd.src_id;
	curl_vm_data.idx = fd.idx;

	return make< var_int_t >( curl_easy_perform( curl ) );
}

var_base_t * feral_curl_easy_str_err_from_int( vm_state_t & vm, const fn_data_t & fd )
{
	srcfile_t * src_file = vm.src_stack.back()->src();
	if( fd.args[ 1 ]->type() != VT_INT ) {
		src_file->fail( fd.args[ 1 ]->idx(), "expected error code to be of type 'int', found: %s",
				vm.type_name( fd.args[ 1 ]->type() ).c_str() );
		return nullptr;
	}
	return make< var_str_t >( curl_easy_strerror( ( CURLcode )INT( fd.args[ 1 ] )->get().get_si() ) );
}

INIT_MODULE( curl )
{
	var_src_t * src = vm.src_stack.back();

	src->add_nativefn( "new_easy", feral_curl_easy_init );
	src->add_nativefn( "set_opt_native", feral_curl_easy_set_opt_native, 3 );
	src->add_nativefn( "strerr", feral_curl_easy_str_err_from_int, 1 );
	src->add_nativefn( "set_default_progress_func_native", feral_curl_set_default_progress_func, 1 );
	src->add_nativefn( "set_default_progress_func_tick_native", feral_curl_set_default_progress_func_tick, 1 );

	// get the type id for curl type (register_type)
	curl_typeid = vm.register_new_type( "curl_t", src_id, idx );

	vm.add_typefn_native( curl_typeid, "perform", feral_curl_easy_perform, 0, src_id, idx );

	// all the enum values

	// CURLcode
	src->add_nativevar( "E_OK", make_all< var_int_t >( CURLE_OK, src_id, idx ) );
	src->add_nativevar( "E_UNSUPPORTED_PROTOCOL", make_all< var_int_t >( CURLE_UNSUPPORTED_PROTOCOL, src_id, idx ) );
	src->add_nativevar( "E_FAILED_INIT", make_all< var_int_t >( CURLE_FAILED_INIT, src_id, idx ) );
	src->add_nativevar( "E_URL_MALFORMAT", make_all< var_int_t >( CURLE_URL_MALFORMAT, src_id, idx ) );
	src->add_nativevar( "E_NOT_BUILT_IN", make_all< var_int_t >( CURLE_NOT_BUILT_IN, src_id, idx ) );
	src->add_nativevar( "E_COULDNT_RESOLVE_PROXY", make_all< var_int_t >( CURLE_COULDNT_RESOLVE_PROXY, src_id, idx ) );
	src->add_nativevar( "E_COULDNT_RESOLVE_HOST", make_all< var_int_t >( CURLE_COULDNT_RESOLVE_HOST, src_id, idx ) );
	src->add_nativevar( "E_COULDNT_CONNECT", make_all< var_int_t >( CURLE_COULDNT_CONNECT, src_id, idx ) );
	src->add_nativevar( "E_WEIRD_SERVER_REPLY", make_all< var_int_t >( CURLE_WEIRD_SERVER_REPLY, src_id, idx ) );
	src->add_nativevar( "E_REMOTE_ACCESS_DENIED", make_all< var_int_t >( CURLE_REMOTE_ACCESS_DENIED, src_id, idx ) );
	src->add_nativevar( "E_FTP_ACCEPT_FAILED", make_all< var_int_t >( CURLE_FTP_ACCEPT_FAILED, src_id, idx ) );
	src->add_nativevar( "E_FTP_WEIRD_PASS_REPLY", make_all< var_int_t >( CURLE_FTP_WEIRD_PASS_REPLY, src_id, idx ) );
	src->add_nativevar( "E_FTP_ACCEPT_TIMEOUT", make_all< var_int_t >( CURLE_FTP_ACCEPT_TIMEOUT, src_id, idx ) );
	src->add_nativevar( "E_FTP_WEIRD_PASV_REPLY", make_all< var_int_t >( CURLE_FTP_WEIRD_PASV_REPLY, src_id, idx ) );
	src->add_nativevar( "E_FTP_WEIRD_227_FORMAT", make_all< var_int_t >( CURLE_FTP_WEIRD_227_FORMAT, src_id, idx ) );
	src->add_nativevar( "E_FTP_CANT_GET_HOST", make_all< var_int_t >( CURLE_FTP_CANT_GET_HOST, src_id, idx ) );
	src->add_nativevar( "E_HTTP2", make_all< var_int_t >( CURLE_HTTP2, src_id, idx ) );
	src->add_nativevar( "E_FTP_COULDNT_SET_TYPE", make_all< var_int_t >( CURLE_FTP_COULDNT_SET_TYPE, src_id, idx ) );
	src->add_nativevar( "E_PARTIAL_FILE", make_all< var_int_t >( CURLE_PARTIAL_FILE, src_id, idx ) );
	src->add_nativevar( "E_FTP_COULDNT_RETR_FILE", make_all< var_int_t >( CURLE_FTP_COULDNT_RETR_FILE, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE20", make_all< var_int_t >( CURLE_OBSOLETE20, src_id, idx ) );
	src->add_nativevar( "E_QUOTE_ERROR", make_all< var_int_t >( CURLE_QUOTE_ERROR, src_id, idx ) );
	src->add_nativevar( "E_HTTP_RETURNED_ERROR", make_all< var_int_t >( CURLE_HTTP_RETURNED_ERROR, src_id, idx ) );
	src->add_nativevar( "E_WRITE_ERROR", make_all< var_int_t >( CURLE_WRITE_ERROR, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE24", make_all< var_int_t >( CURLE_OBSOLETE24, src_id, idx ) );
	src->add_nativevar( "E_UPLOAD_FAILED", make_all< var_int_t >( CURLE_UPLOAD_FAILED, src_id, idx ) );
	src->add_nativevar( "E_READ_ERROR", make_all< var_int_t >( CURLE_READ_ERROR, src_id, idx ) );
	src->add_nativevar( "E_OUT_OF_MEMORY", make_all< var_int_t >( CURLE_OUT_OF_MEMORY, src_id, idx ) );
	src->add_nativevar( "E_OPERATION_TIMEDOUT", make_all< var_int_t >( CURLE_OPERATION_TIMEDOUT, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE29", make_all< var_int_t >( CURLE_OBSOLETE29, src_id, idx ) );
	src->add_nativevar( "E_FTP_PORT_FAILED", make_all< var_int_t >( CURLE_FTP_PORT_FAILED, src_id, idx ) );
	src->add_nativevar( "E_FTP_COULDNT_USE_REST", make_all< var_int_t >( CURLE_FTP_COULDNT_USE_REST, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE32", make_all< var_int_t >( CURLE_OBSOLETE32, src_id, idx ) );
	src->add_nativevar( "E_RANGE_ERROR", make_all< var_int_t >( CURLE_RANGE_ERROR, src_id, idx ) );
	src->add_nativevar( "E_HTTP_POST_ERROR", make_all< var_int_t >( CURLE_HTTP_POST_ERROR, src_id, idx ) );
	src->add_nativevar( "E_SSL_CONNECT_ERROR", make_all< var_int_t >( CURLE_SSL_CONNECT_ERROR, src_id, idx ) );
	src->add_nativevar( "E_BAD_DOWNLOAD_RESUME", make_all< var_int_t >( CURLE_BAD_DOWNLOAD_RESUME, src_id, idx ) );
	src->add_nativevar( "E_FILE_COULDNT_READ_FILE", make_all< var_int_t >( CURLE_FILE_COULDNT_READ_FILE, src_id, idx ) );
	src->add_nativevar( "E_LDAP_CANNOT_BIND", make_all< var_int_t >( CURLE_LDAP_CANNOT_BIND, src_id, idx ) );
	src->add_nativevar( "E_LDAP_SEARCH_FAILED", make_all< var_int_t >( CURLE_LDAP_SEARCH_FAILED, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE40", make_all< var_int_t >( CURLE_OBSOLETE40, src_id, idx ) );
	src->add_nativevar( "E_FUNCTION_NOT_FOUND", make_all< var_int_t >( CURLE_FUNCTION_NOT_FOUND, src_id, idx ) );
	src->add_nativevar( "E_ABORTED_BY_CALLBACK", make_all< var_int_t >( CURLE_ABORTED_BY_CALLBACK, src_id, idx ) );
	src->add_nativevar( "E_BAD_FUNCTION_ARGUMENT", make_all< var_int_t >( CURLE_BAD_FUNCTION_ARGUMENT, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE44", make_all< var_int_t >( CURLE_OBSOLETE44, src_id, idx ) );
	src->add_nativevar( "E_INTERFACE_FAILED", make_all< var_int_t >( CURLE_INTERFACE_FAILED, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE46", make_all< var_int_t >( CURLE_OBSOLETE46, src_id, idx ) );
	src->add_nativevar( "E_TOO_MANY_REDIRECTS", make_all< var_int_t >( CURLE_TOO_MANY_REDIRECTS, src_id, idx ) );
	src->add_nativevar( "E_UNKNOWN_OPTION", make_all< var_int_t >( CURLE_UNKNOWN_OPTION, src_id, idx ) );
	src->add_nativevar( "E_TELNET_OPTION_SYNTAX", make_all< var_int_t >( CURLE_TELNET_OPTION_SYNTAX, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE50", make_all< var_int_t >( CURLE_OBSOLETE50, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE51", make_all< var_int_t >( CURLE_OBSOLETE51, src_id, idx ) );
	src->add_nativevar( "E_GOT_NOTHING", make_all< var_int_t >( CURLE_GOT_NOTHING, src_id, idx ) );
	src->add_nativevar( "E_SSL_ENGINE_NOTFOUND", make_all< var_int_t >( CURLE_SSL_ENGINE_NOTFOUND, src_id, idx ) );
	src->add_nativevar( "E_SSL_ENGINE_SETFAILED", make_all< var_int_t >( CURLE_SSL_ENGINE_SETFAILED, src_id, idx ) );
	src->add_nativevar( "E_SEND_ERROR", make_all< var_int_t >( CURLE_SEND_ERROR, src_id, idx ) );
	src->add_nativevar( "E_RECV_ERROR", make_all< var_int_t >( CURLE_RECV_ERROR, src_id, idx ) );
	src->add_nativevar( "E_OBSOLETE57", make_all< var_int_t >( CURLE_OBSOLETE57, src_id, idx ) );
	src->add_nativevar( "E_SSL_CERTPROBLEM", make_all< var_int_t >( CURLE_SSL_CERTPROBLEM, src_id, idx ) );
	src->add_nativevar( "E_SSL_CIPHER", make_all< var_int_t >( CURLE_SSL_CIPHER, src_id, idx ) );
	src->add_nativevar( "E_PEER_FAILED_VERIFICATION", make_all< var_int_t >( CURLE_PEER_FAILED_VERIFICATION, src_id, idx ) );
	src->add_nativevar( "E_BAD_CONTENT_ENCODING", make_all< var_int_t >( CURLE_BAD_CONTENT_ENCODING, src_id, idx ) );
	src->add_nativevar( "E_LDAP_INVALID_URL", make_all< var_int_t >( CURLE_LDAP_INVALID_URL, src_id, idx ) );
	src->add_nativevar( "E_FILESIZE_EXCEEDED", make_all< var_int_t >( CURLE_FILESIZE_EXCEEDED, src_id, idx ) );
	src->add_nativevar( "E_USE_SSL_FAILED", make_all< var_int_t >( CURLE_USE_SSL_FAILED, src_id, idx ) );
	src->add_nativevar( "E_SEND_FAIL_REWIND", make_all< var_int_t >( CURLE_SEND_FAIL_REWIND, src_id, idx ) );
	src->add_nativevar( "E_SSL_ENGINE_INITFAILED", make_all< var_int_t >( CURLE_SSL_ENGINE_INITFAILED, src_id, idx ) );
	src->add_nativevar( "E_LOGIN_DENIED", make_all< var_int_t >( CURLE_LOGIN_DENIED, src_id, idx ) );
	src->add_nativevar( "E_TFTP_NOTFOUND", make_all< var_int_t >( CURLE_TFTP_NOTFOUND, src_id, idx ) );
	src->add_nativevar( "E_TFTP_PERM", make_all< var_int_t >( CURLE_TFTP_PERM, src_id, idx ) );
	src->add_nativevar( "E_REMOTE_DISK_FULL", make_all< var_int_t >( CURLE_REMOTE_DISK_FULL, src_id, idx ) );
	src->add_nativevar( "E_TFTP_ILLEGAL", make_all< var_int_t >( CURLE_TFTP_ILLEGAL, src_id, idx ) );
	src->add_nativevar( "E_TFTP_UNKNOWNID", make_all< var_int_t >( CURLE_TFTP_UNKNOWNID, src_id, idx ) );
	src->add_nativevar( "E_REMOTE_FILE_EXISTS", make_all< var_int_t >( CURLE_REMOTE_FILE_EXISTS, src_id, idx ) );
	src->add_nativevar( "E_TFTP_NOSUCHUSER", make_all< var_int_t >( CURLE_TFTP_NOSUCHUSER, src_id, idx ) );
	src->add_nativevar( "E_CONV_FAILED", make_all< var_int_t >( CURLE_CONV_FAILED, src_id, idx ) );
	src->add_nativevar( "E_CONV_REQD", make_all< var_int_t >( CURLE_CONV_REQD, src_id, idx ) );
	src->add_nativevar( "E_SSL_CACERT_BADFILE", make_all< var_int_t >( CURLE_SSL_CACERT_BADFILE, src_id, idx ) );
	src->add_nativevar( "E_REMOTE_FILE_NOT_FOUND", make_all< var_int_t >( CURLE_REMOTE_FILE_NOT_FOUND, src_id, idx ) );
	src->add_nativevar( "E_SSH", make_all< var_int_t >( CURLE_SSH, src_id, idx ) );
	src->add_nativevar( "E_SSL_SHUTDOWN_FAILED", make_all< var_int_t >( CURLE_SSL_SHUTDOWN_FAILED, src_id, idx ) );
	src->add_nativevar( "E_AGAIN", make_all< var_int_t >( CURLE_AGAIN, src_id, idx ) );
	src->add_nativevar( "E_SSL_CRL_BADFILE", make_all< var_int_t >( CURLE_SSL_CRL_BADFILE, src_id, idx ) );
	src->add_nativevar( "E_SSL_ISSUER_ERROR", make_all< var_int_t >( CURLE_SSL_ISSUER_ERROR, src_id, idx ) );
	src->add_nativevar( "E_FTP_PRET_FAILED", make_all< var_int_t >( CURLE_FTP_PRET_FAILED, src_id, idx ) );
	src->add_nativevar( "E_RTSP_CSEQ_ERROR", make_all< var_int_t >( CURLE_RTSP_CSEQ_ERROR, src_id, idx ) );
	src->add_nativevar( "E_RTSP_SESSION_ERROR", make_all< var_int_t >( CURLE_RTSP_SESSION_ERROR, src_id, idx ) );
	src->add_nativevar( "E_FTP_BAD_FILE_LIST", make_all< var_int_t >( CURLE_FTP_BAD_FILE_LIST, src_id, idx ) );
	src->add_nativevar( "E_CHUNK_FAILED", make_all< var_int_t >( CURLE_CHUNK_FAILED, src_id, idx ) );
	src->add_nativevar( "E_NO_CONNECTION_AVAILABLE", make_all< var_int_t >( CURLE_NO_CONNECTION_AVAILABLE, src_id, idx ) );
	src->add_nativevar( "E_SSL_PINNEDPUBKEYNOTMATCH", make_all< var_int_t >( CURLE_SSL_PINNEDPUBKEYNOTMATCH, src_id, idx ) );
	src->add_nativevar( "E_SSL_INVALIDCERTSTATUS", make_all< var_int_t >( CURLE_SSL_INVALIDCERTSTATUS, src_id, idx ) );
	src->add_nativevar( "E_HTTP2_STREAM", make_all< var_int_t >( CURLE_HTTP2_STREAM, src_id, idx ) );
	src->add_nativevar( "E_RECURSIVE_API_CALL", make_all< var_int_t >( CURLE_RECURSIVE_API_CALL, src_id, idx ) );
/* TODO: Seem to be in different version than available
	src->add_nativevar( "E_AUTH_ERROR", make_all< var_int_t >( CURLE_AUTH_ERROR, src_id, idx ) );
	src->add_nativevar( "E_HTTP3", make_all< var_int_t >( CURLE_HTTP3, src_id, idx ) );
	src->add_nativevar( "E_QUIC_CONNECT_ERROR", make_all< var_int_t >( CURLE_QUIC_CONNECT_ERROR, src_id, idx ) );
*/

	// CURLMcode
	src->add_nativevar( "M_CALL_MULTI_PERFORM", make_all< var_int_t >( CURLM_CALL_MULTI_PERFORM, src_id, idx ) );
	src->add_nativevar( "M_OK", make_all< var_int_t >( CURLM_OK, src_id, idx ) );
	src->add_nativevar( "M_BAD_HANDLE", make_all< var_int_t >( CURLM_BAD_HANDLE, src_id, idx ) );
	src->add_nativevar( "M_BAD_EASY_HANDLE", make_all< var_int_t >( CURLM_BAD_EASY_HANDLE, src_id, idx ) );
	src->add_nativevar( "M_OUT_OF_MEMORY", make_all< var_int_t >( CURLM_OUT_OF_MEMORY, src_id, idx ) );
	src->add_nativevar( "M_INTERNAL_ERROR", make_all< var_int_t >( CURLM_INTERNAL_ERROR, src_id, idx ) );
	src->add_nativevar( "M_BAD_SOCKET", make_all< var_int_t >( CURLM_BAD_SOCKET, src_id, idx ) );
	src->add_nativevar( "M_UNKNOWN_OPTION", make_all< var_int_t >( CURLM_UNKNOWN_OPTION, src_id, idx ) );
	src->add_nativevar( "M_ADDED_ALREADY", make_all< var_int_t >( CURLM_ADDED_ALREADY, src_id, idx ) );
	src->add_nativevar( "M_RECURSIVE_API_CALL", make_all< var_int_t >( CURLM_RECURSIVE_API_CALL, src_id, idx ) );
/* TODO: Seem to be in different version than available
	src->add_nativevar( "WAKEUP_FAILURE", make_all< var_int_t >( CURLM_WAKEUP_FAILURE, src_id, idx ) );
	src->add_nativevar( "BAD_FUNCTION_ARGUMENT", make_all< var_int_t >( CURLM_BAD_FUNCTION_ARGUMENT, src_id, idx ) );
*/

	// CURLSHcode
	src->add_nativevar( "SHE_OK", make_all< var_int_t >( CURLSHE_OK, src_id, idx ) );
	src->add_nativevar( "SHE_BAD_OPTION", make_all< var_int_t >( CURLSHE_BAD_OPTION, src_id, idx ) );
	src->add_nativevar( "SHE_IN_USE", make_all< var_int_t >( CURLSHE_IN_USE, src_id, idx ) );
	src->add_nativevar( "SHE_INVALID", make_all< var_int_t >( CURLSHE_INVALID, src_id, idx ) );
	src->add_nativevar( "SHE_NOMEM", make_all< var_int_t >( CURLSHE_NOMEM, src_id, idx ) );
	src->add_nativevar( "SHE_NOT_BUILT_IN", make_all< var_int_t >( CURLSHE_NOT_BUILT_IN, src_id, idx ) );

#if CURL_AT_LEAST_VERSION(7, 62, 0)
	// CURLUcode
	src->add_nativevar( "UE_OK", make_all< var_int_t >( CURLUE_OK, src_id, idx ) );
	src->add_nativevar( "UE_BAD_HANDLE", make_all< var_int_t >( CURLUE_BAD_HANDLE, src_id, idx ) );
	src->add_nativevar( "UE_BAD_PARTPOINTER", make_all< var_int_t >( CURLUE_BAD_PARTPOINTER, src_id, idx ) );
	src->add_nativevar( "UE_MALFORMED_INPUT", make_all< var_int_t >( CURLUE_MALFORMED_INPUT, src_id, idx ) );
	src->add_nativevar( "UE_BAD_PORT_NUMBER", make_all< var_int_t >( CURLUE_BAD_PORT_NUMBER, src_id, idx ) );
	src->add_nativevar( "UE_UNSUPPORTED_SCHEME", make_all< var_int_t >( CURLUE_UNSUPPORTED_SCHEME, src_id, idx ) );
	src->add_nativevar( "UE_URLDECODE", make_all< var_int_t >( CURLUE_URLDECODE, src_id, idx ) );
	src->add_nativevar( "UE_OUT_OF_MEMORY", make_all< var_int_t >( CURLUE_OUT_OF_MEMORY, src_id, idx ) );
	src->add_nativevar( "UE_USER_NOT_ALLOWED", make_all< var_int_t >( CURLUE_USER_NOT_ALLOWED, src_id, idx ) );
	src->add_nativevar( "UE_UNKNOWN_PART", make_all< var_int_t >( CURLUE_UNKNOWN_PART, src_id, idx ) );
	src->add_nativevar( "UE_NO_SCHEME", make_all< var_int_t >( CURLUE_NO_SCHEME, src_id, idx ) );
	src->add_nativevar( "UE_NO_USER", make_all< var_int_t >( CURLUE_NO_USER, src_id, idx ) );
	src->add_nativevar( "UE_NO_PASSWORD", make_all< var_int_t >( CURLUE_NO_PASSWORD, src_id, idx ) );
	src->add_nativevar( "UE_NO_OPTIONS", make_all< var_int_t >( CURLUE_NO_OPTIONS, src_id, idx ) );
	src->add_nativevar( "UE_NO_HOST", make_all< var_int_t >( CURLUE_NO_HOST, src_id, idx ) );
	src->add_nativevar( "UE_NO_PORT", make_all< var_int_t >( CURLUE_NO_PORT, src_id, idx ) );
	src->add_nativevar( "UE_NO_QUERY", make_all< var_int_t >( CURLUE_NO_QUERY, src_id, idx ) );
	src->add_nativevar( "UE_NO_FRAGMENT", make_all< var_int_t >( CURLUE_NO_FRAGMENT, src_id, idx ) );
#endif

	// EASY_OPTS

	// BEHAVIOR OPTIONS
	src->add_nativevar( "OPT_VERBOSE", make_all< var_int_t >( CURLOPT_VERBOSE, src_id, idx ) );
	src->add_nativevar( "OPT_HEADER", make_all< var_int_t >( CURLOPT_HEADER, src_id, idx ) );
	src->add_nativevar( "OPT_NOPROGRESS", make_all< var_int_t >( CURLOPT_NOPROGRESS, src_id, idx ) );
	src->add_nativevar( "OPT_NOSIGNAL", make_all< var_int_t >( CURLOPT_NOSIGNAL, src_id, idx ) );
	src->add_nativevar( "OPT_WILDCARDMATCH", make_all< var_int_t >( CURLOPT_WILDCARDMATCH, src_id, idx ) );

	// CALLBACK OPTIONS
	src->add_nativevar( "OPT_WRITEFUNCTION", make_all< var_int_t >( CURLOPT_WRITEFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_WRITEDATA", make_all< var_int_t >( CURLOPT_WRITEDATA, src_id, idx ) );
	src->add_nativevar( "OPT_READFUNCTION", make_all< var_int_t >( CURLOPT_READFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_READDATA", make_all< var_int_t >( CURLOPT_READDATA, src_id, idx ) );
	src->add_nativevar( "OPT_IOCTLFUNCTION", make_all< var_int_t >( CURLOPT_IOCTLFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_IOCTLDATA", make_all< var_int_t >( CURLOPT_IOCTLDATA, src_id, idx ) );
	src->add_nativevar( "OPT_SEEKFUNCTION", make_all< var_int_t >( CURLOPT_SEEKFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_SEEKDATA", make_all< var_int_t >( CURLOPT_SEEKDATA, src_id, idx ) );
	src->add_nativevar( "OPT_SOCKOPTFUNCTION", make_all< var_int_t >( CURLOPT_SOCKOPTFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_SOCKOPTDATA", make_all< var_int_t >( CURLOPT_SOCKOPTDATA, src_id, idx ) );
	src->add_nativevar( "OPT_OPENSOCKETFUNCTION", make_all< var_int_t >( CURLOPT_OPENSOCKETFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_OPENSOCKETDATA", make_all< var_int_t >( CURLOPT_OPENSOCKETDATA, src_id, idx ) );
	src->add_nativevar( "OPT_CLOSESOCKETFUNCTION", make_all< var_int_t >( CURLOPT_CLOSESOCKETFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_CLOSESOCKETDATA", make_all< var_int_t >( CURLOPT_CLOSESOCKETDATA, src_id, idx ) );
	src->add_nativevar( "OPT_PROGRESSFUNCTION", make_all< var_int_t >( CURLOPT_PROGRESSFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_PROGRESSDATA", make_all< var_int_t >( CURLOPT_PROGRESSDATA, src_id, idx ) );
	src->add_nativevar( "OPT_XFERINFOFUNCTION", make_all< var_int_t >( CURLOPT_XFERINFOFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_XFERINFODATA", make_all< var_int_t >( CURLOPT_XFERINFODATA, src_id, idx ) );
	src->add_nativevar( "OPT_HEADERFUNCTION", make_all< var_int_t >( CURLOPT_HEADERFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_HEADERDATA", make_all< var_int_t >( CURLOPT_HEADERDATA, src_id, idx ) );
	src->add_nativevar( "OPT_DEBUGFUNCTION", make_all< var_int_t >( CURLOPT_DEBUGFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_DEBUGDATA", make_all< var_int_t >( CURLOPT_DEBUGDATA, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_CTX_FUNCTION", make_all< var_int_t >( CURLOPT_SSL_CTX_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_CTX_DATA", make_all< var_int_t >( CURLOPT_SSL_CTX_DATA, src_id, idx ) );
	src->add_nativevar( "OPT_CONV_TO_NETWORK_FUNCTION", make_all< var_int_t >( CURLOPT_CONV_TO_NETWORK_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_CONV_FROM_NETWORK_FUNCTION", make_all< var_int_t >( CURLOPT_CONV_FROM_NETWORK_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_CONV_FROM_UTF8_FUNCTION", make_all< var_int_t >( CURLOPT_CONV_FROM_UTF8_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_INTERLEAVEFUNCTION", make_all< var_int_t >( CURLOPT_INTERLEAVEFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_INTERLEAVEDATA", make_all< var_int_t >( CURLOPT_INTERLEAVEDATA, src_id, idx ) );
	src->add_nativevar( "OPT_CHUNK_BGN_FUNCTION", make_all< var_int_t >( CURLOPT_CHUNK_BGN_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_CHUNK_END_FUNCTION", make_all< var_int_t >( CURLOPT_CHUNK_END_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_CHUNK_DATA", make_all< var_int_t >( CURLOPT_CHUNK_DATA, src_id, idx ) );
	src->add_nativevar( "OPT_FNMATCH_FUNCTION", make_all< var_int_t >( CURLOPT_FNMATCH_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_FNMATCH_DATA", make_all< var_int_t >( CURLOPT_FNMATCH_DATA, src_id, idx ) );
	src->add_nativevar( "OPT_SUPPRESS_CONNECT_HEADERS", make_all< var_int_t >( CURLOPT_SUPPRESS_CONNECT_HEADERS, src_id, idx ) );
	src->add_nativevar( "OPT_RESOLVER_START_FUNCTION", make_all< var_int_t >( CURLOPT_RESOLVER_START_FUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_RESOLVER_START_DATA", make_all< var_int_t >( CURLOPT_RESOLVER_START_DATA, src_id, idx ) );

	// ERROR OPTIONS
	src->add_nativevar( "OPT_ERRORBUFFER", make_all< var_int_t >( CURLOPT_ERRORBUFFER, src_id, idx ) );
	src->add_nativevar( "OPT_STDERR", make_all< var_int_t >( CURLOPT_STDERR, src_id, idx ) );
	src->add_nativevar( "OPT_FAILONERROR", make_all< var_int_t >( CURLOPT_FAILONERROR, src_id, idx ) );
	src->add_nativevar( "OPT_KEEP_SENDING_ON_ERROR", make_all< var_int_t >( CURLOPT_KEEP_SENDING_ON_ERROR, src_id, idx ) );

	// NETWORK OPTIONS
	src->add_nativevar( "OPT_URL", make_all< var_int_t >( CURLOPT_URL, src_id, idx ) );
	src->add_nativevar( "OPT_PATH_AS_IS", make_all< var_int_t >( CURLOPT_PATH_AS_IS, src_id, idx ) );
	src->add_nativevar( "OPT_PROTOCOLS", make_all< var_int_t >( CURLOPT_PROTOCOLS, src_id, idx ) );
	src->add_nativevar( "OPT_REDIR_PROTOCOLS", make_all< var_int_t >( CURLOPT_REDIR_PROTOCOLS, src_id, idx ) );
	src->add_nativevar( "OPT_DEFAULT_PROTOCOL", make_all< var_int_t >( CURLOPT_DEFAULT_PROTOCOL, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY", make_all< var_int_t >( CURLOPT_PROXY, src_id, idx ) );
	src->add_nativevar( "OPT_PRE_PROXY", make_all< var_int_t >( CURLOPT_PRE_PROXY, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYPORT", make_all< var_int_t >( CURLOPT_PROXYPORT, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYTYPE", make_all< var_int_t >( CURLOPT_PROXYTYPE, src_id, idx ) );
	src->add_nativevar( "OPT_NOPROXY", make_all< var_int_t >( CURLOPT_NOPROXY, src_id, idx ) );
	src->add_nativevar( "OPT_HTTPPROXYTUNNEL", make_all< var_int_t >( CURLOPT_HTTPPROXYTUNNEL, src_id, idx ) );
	src->add_nativevar( "OPT_CONNECT_TO", make_all< var_int_t >( CURLOPT_CONNECT_TO, src_id, idx ) );
	src->add_nativevar( "OPT_SOCKS5_AUTH", make_all< var_int_t >( CURLOPT_SOCKS5_AUTH, src_id, idx ) );
	src->add_nativevar( "OPT_SOCKS5_GSSAPI_SERVICE", make_all< var_int_t >( CURLOPT_SOCKS5_GSSAPI_SERVICE, src_id, idx ) );
	src->add_nativevar( "OPT_SOCKS5_GSSAPI_NEC", make_all< var_int_t >( CURLOPT_SOCKS5_GSSAPI_NEC, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SERVICE_NAME", make_all< var_int_t >( CURLOPT_PROXY_SERVICE_NAME, src_id, idx ) );
	src->add_nativevar( "OPT_HAPROXYPROTOCOL", make_all< var_int_t >( CURLOPT_HAPROXYPROTOCOL, src_id, idx ) );
	src->add_nativevar( "OPT_SERVICE_NAME", make_all< var_int_t >( CURLOPT_SERVICE_NAME, src_id, idx ) );
	src->add_nativevar( "OPT_INTERFACE", make_all< var_int_t >( CURLOPT_INTERFACE, src_id, idx ) );
	src->add_nativevar( "OPT_LOCALPORT", make_all< var_int_t >( CURLOPT_LOCALPORT, src_id, idx ) );
	src->add_nativevar( "OPT_LOCALPORTRANGE", make_all< var_int_t >( CURLOPT_LOCALPORTRANGE, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_CACHE_TIMEOUT", make_all< var_int_t >( CURLOPT_DNS_CACHE_TIMEOUT, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_USE_GLOBAL_CACHE", make_all< var_int_t >( CURLOPT_DNS_USE_GLOBAL_CACHE, src_id, idx ) );
	src->add_nativevar( "OPT_DOH_URL", make_all< var_int_t >( CURLOPT_DOH_URL, src_id, idx ) );
	src->add_nativevar( "OPT_BUFFERSIZE", make_all< var_int_t >( CURLOPT_BUFFERSIZE, src_id, idx ) );
	src->add_nativevar( "OPT_PORT", make_all< var_int_t >( CURLOPT_PORT, src_id, idx ) );
	src->add_nativevar( "OPT_TCP_FASTOPEN", make_all< var_int_t >( CURLOPT_TCP_FASTOPEN, src_id, idx ) );
	src->add_nativevar( "OPT_TCP_NODELAY", make_all< var_int_t >( CURLOPT_TCP_NODELAY, src_id, idx ) );
	src->add_nativevar( "OPT_ADDRESS_SCOPE", make_all< var_int_t >( CURLOPT_ADDRESS_SCOPE, src_id, idx ) );
	src->add_nativevar( "OPT_TCP_KEEPALIVE", make_all< var_int_t >( CURLOPT_TCP_KEEPALIVE, src_id, idx ) );
	src->add_nativevar( "OPT_TCP_KEEPIDLE", make_all< var_int_t >( CURLOPT_TCP_KEEPIDLE, src_id, idx ) );
	src->add_nativevar( "OPT_TCP_KEEPINTVL", make_all< var_int_t >( CURLOPT_TCP_KEEPINTVL, src_id, idx ) );
	src->add_nativevar( "OPT_UNIX_SOCKET_PATH", make_all< var_int_t >( CURLOPT_UNIX_SOCKET_PATH, src_id, idx ) );
	src->add_nativevar( "OPT_ABSTRACT_UNIX_SOCKET", make_all< var_int_t >( CURLOPT_ABSTRACT_UNIX_SOCKET, src_id, idx ) );

	// NAMES and PASSWORDS OPTIONS (Authentication)
	src->add_nativevar( "OPT_NETRC", make_all< var_int_t >( CURLOPT_NETRC, src_id, idx ) );
	src->add_nativevar( "OPT_NETRC_FILE", make_all< var_int_t >( CURLOPT_NETRC_FILE, src_id, idx ) );
	src->add_nativevar( "OPT_USERPWD", make_all< var_int_t >( CURLOPT_USERPWD, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYUSERPWD", make_all< var_int_t >( CURLOPT_PROXYUSERPWD, src_id, idx ) );
	src->add_nativevar( "OPT_USERNAME", make_all< var_int_t >( CURLOPT_USERNAME, src_id, idx ) );
	src->add_nativevar( "OPT_PASSWORD", make_all< var_int_t >( CURLOPT_PASSWORD, src_id, idx ) );
	src->add_nativevar( "OPT_LOGIN_OPTIONS", make_all< var_int_t >( CURLOPT_LOGIN_OPTIONS, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYUSERNAME", make_all< var_int_t >( CURLOPT_PROXYUSERNAME, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYPASSWORD", make_all< var_int_t >( CURLOPT_PROXYPASSWORD, src_id, idx ) );
	src->add_nativevar( "OPT_HTTPAUTH", make_all< var_int_t >( CURLOPT_HTTPAUTH, src_id, idx ) );
	src->add_nativevar( "OPT_TLSAUTH_USERNAME", make_all< var_int_t >( CURLOPT_TLSAUTH_USERNAME, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_USERNAME", make_all< var_int_t >( CURLOPT_PROXY_TLSAUTH_USERNAME, src_id, idx ) );
	src->add_nativevar( "OPT_TLSAUTH_PASSWORD", make_all< var_int_t >( CURLOPT_TLSAUTH_PASSWORD, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_PASSWORD", make_all< var_int_t >( CURLOPT_PROXY_TLSAUTH_PASSWORD, src_id, idx ) );
	src->add_nativevar( "OPT_TLSAUTH_TYPE", make_all< var_int_t >( CURLOPT_TLSAUTH_TYPE, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_TYPE", make_all< var_int_t >( CURLOPT_PROXY_TLSAUTH_TYPE, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYAUTH", make_all< var_int_t >( CURLOPT_PROXYAUTH, src_id, idx ) );
/* TODO: not available
	src->add_nativevar( "OPT_SASL_AUTHZID", make_all< var_int_t >( CURLOPT_SASL_AUTHZID, src_id, idx ) );
*/
	src->add_nativevar( "OPT_SASL_IR", make_all< var_int_t >( CURLOPT_SASL_IR, src_id, idx ) );
	src->add_nativevar( "OPT_XOAUTH2_BEARER", make_all< var_int_t >( CURLOPT_XOAUTH2_BEARER, src_id, idx ) );
	src->add_nativevar( "OPT_DISALLOW_USERNAME_IN_URL", make_all< var_int_t >( CURLOPT_DISALLOW_USERNAME_IN_URL, src_id, idx ) );

	// HTTP OPTIONS
	src->add_nativevar( "OPT_AUTOREFERER", make_all< var_int_t >( CURLOPT_AUTOREFERER, src_id, idx ) );
	src->add_nativevar( "OPT_ACCEPT_ENCODING", make_all< var_int_t >( CURLOPT_ACCEPT_ENCODING, src_id, idx ) );
	src->add_nativevar( "OPT_TRANSFER_ENCODING", make_all< var_int_t >( CURLOPT_TRANSFER_ENCODING, src_id, idx ) );
	src->add_nativevar( "OPT_FOLLOWLOCATION", make_all< var_int_t >( CURLOPT_FOLLOWLOCATION, src_id, idx ) );
	src->add_nativevar( "OPT_UNRESTRICTED_AUTH", make_all< var_int_t >( CURLOPT_UNRESTRICTED_AUTH, src_id, idx ) );
	src->add_nativevar( "OPT_MAXREDIRS", make_all< var_int_t >( CURLOPT_MAXREDIRS, src_id, idx ) );
	src->add_nativevar( "OPT_POSTREDIR", make_all< var_int_t >( CURLOPT_POSTREDIR, src_id, idx ) );
	src->add_nativevar( "OPT_PUT", make_all< var_int_t >( CURLOPT_PUT, src_id, idx ) );
	src->add_nativevar( "OPT_POST", make_all< var_int_t >( CURLOPT_POST, src_id, idx ) );
	src->add_nativevar( "OPT_POSTFIELDS", make_all< var_int_t >( CURLOPT_POSTFIELDS, src_id, idx ) );
	src->add_nativevar( "OPT_POSTFIELDSIZE", make_all< var_int_t >( CURLOPT_POSTFIELDSIZE, src_id, idx ) );
	src->add_nativevar( "OPT_POSTFIELDSIZE_LARGE", make_all< var_int_t >( CURLOPT_POSTFIELDSIZE_LARGE, src_id, idx ) );
	src->add_nativevar( "OPT_COPYPOSTFIELDS", make_all< var_int_t >( CURLOPT_COPYPOSTFIELDS, src_id, idx ) );
	src->add_nativevar( "OPT_HTTPPOST", make_all< var_int_t >( CURLOPT_HTTPPOST, src_id, idx ) );
	src->add_nativevar( "OPT_REFERER", make_all< var_int_t >( CURLOPT_REFERER, src_id, idx ) );
	src->add_nativevar( "OPT_USERAGENT", make_all< var_int_t >( CURLOPT_USERAGENT, src_id, idx ) );
	src->add_nativevar( "OPT_HTTPHEADER", make_all< var_int_t >( CURLOPT_HTTPHEADER, src_id, idx ) );
	src->add_nativevar( "OPT_HEADEROPT", make_all< var_int_t >( CURLOPT_HEADEROPT, src_id, idx ) );
	src->add_nativevar( "OPT_PROXYHEADER", make_all< var_int_t >( CURLOPT_PROXYHEADER, src_id, idx ) );
	src->add_nativevar( "OPT_HTTP200ALIASES", make_all< var_int_t >( CURLOPT_HTTP200ALIASES, src_id, idx ) );
	src->add_nativevar( "OPT_COOKIE", make_all< var_int_t >( CURLOPT_COOKIE, src_id, idx ) );
	src->add_nativevar( "OPT_COOKIEFILE", make_all< var_int_t >( CURLOPT_COOKIEFILE, src_id, idx ) );
	src->add_nativevar( "OPT_COOKIEJAR", make_all< var_int_t >( CURLOPT_COOKIEJAR, src_id, idx ) );
	src->add_nativevar( "OPT_COOKIESESSION", make_all< var_int_t >( CURLOPT_COOKIESESSION, src_id, idx ) );
	src->add_nativevar( "OPT_COOKIELIST", make_all< var_int_t >( CURLOPT_COOKIELIST, src_id, idx ) );
	src->add_nativevar( "OPT_ALTSVC", make_all< var_int_t >( CURLOPT_ALTSVC, src_id, idx ) );
	src->add_nativevar( "OPT_ALTSVC_CTRL", make_all< var_int_t >( CURLOPT_ALTSVC_CTRL, src_id, idx ) );
	src->add_nativevar( "OPT_HTTPGET", make_all< var_int_t >( CURLOPT_HTTPGET, src_id, idx ) );
	src->add_nativevar( "OPT_REQUEST_TARGET", make_all< var_int_t >( CURLOPT_REQUEST_TARGET, src_id, idx ) );
	src->add_nativevar( "OPT_HTTP_VERSION", make_all< var_int_t >( CURLOPT_HTTP_VERSION, src_id, idx ) );
	src->add_nativevar( "OPT_HTTP09_ALLOWED", make_all< var_int_t >( CURLOPT_HTTP09_ALLOWED, src_id, idx ) );
	src->add_nativevar( "OPT_IGNORE_CONTENT_LENGTH", make_all< var_int_t >( CURLOPT_IGNORE_CONTENT_LENGTH, src_id, idx ) );
	src->add_nativevar( "OPT_HTTP_CONTENT_DECODING", make_all< var_int_t >( CURLOPT_HTTP_CONTENT_DECODING, src_id, idx ) );
	src->add_nativevar( "OPT_HTTP_TRANSFER_DECODING", make_all< var_int_t >( CURLOPT_HTTP_TRANSFER_DECODING, src_id, idx ) );
	src->add_nativevar( "OPT_EXPECT_100_TIMEOUT_MS", make_all< var_int_t >( CURLOPT_EXPECT_100_TIMEOUT_MS, src_id, idx ) );
	src->add_nativevar( "OPT_TRAILERFUNCTION", make_all< var_int_t >( CURLOPT_TRAILERFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_TRAILERDATA", make_all< var_int_t >( CURLOPT_TRAILERDATA, src_id, idx ) );
	src->add_nativevar( "OPT_PIPEWAIT", make_all< var_int_t >( CURLOPT_PIPEWAIT, src_id, idx ) );
	src->add_nativevar( "OPT_STREAM_DEPENDS", make_all< var_int_t >( CURLOPT_STREAM_DEPENDS, src_id, idx ) );
	src->add_nativevar( "OPT_STREAM_DEPENDS_E", make_all< var_int_t >( CURLOPT_STREAM_DEPENDS_E, src_id, idx ) );
	src->add_nativevar( "OPT_STREAM_WEIGHT", make_all< var_int_t >( CURLOPT_STREAM_WEIGHT, src_id, idx ) );

	// SMTP OPTIONS
	src->add_nativevar( "OPT_MAIL_FROM", make_all< var_int_t >( CURLOPT_MAIL_FROM, src_id, idx ) );
	src->add_nativevar( "OPT_MAIL_RCPT", make_all< var_int_t >( CURLOPT_MAIL_RCPT, src_id, idx ) );
	src->add_nativevar( "OPT_MAIL_AUTH", make_all< var_int_t >( CURLOPT_MAIL_AUTH, src_id, idx ) );
/* TODO: not available
	src->add_nativevar( "OPT_MAIL_RCPT_ALLLOWFAILS", make_all< var_int_t >( CURLOPT_MAIL_RCPT_ALLLOWFAILS, src_id, idx ) );
*/

	// TFTP OPTIONS
	src->add_nativevar( "OPT_TFTP_BLKSIZE", make_all< var_int_t >( CURLOPT_TFTP_BLKSIZE, src_id, idx ) );
	src->add_nativevar( "OPT_TFTP_NO_OPTIONS", make_all< var_int_t >( CURLOPT_TFTP_NO_OPTIONS, src_id, idx ) );

	// FTP OPTIONS
	src->add_nativevar( "OPT_FTPPORT", make_all< var_int_t >( CURLOPT_FTPPORT, src_id, idx ) );
	src->add_nativevar( "OPT_QUOTE", make_all< var_int_t >( CURLOPT_QUOTE, src_id, idx ) );
	src->add_nativevar( "OPT_POSTQUOTE", make_all< var_int_t >( CURLOPT_POSTQUOTE, src_id, idx ) );
	src->add_nativevar( "OPT_PREQUOTE", make_all< var_int_t >( CURLOPT_PREQUOTE, src_id, idx ) );
	src->add_nativevar( "OPT_APPEND", make_all< var_int_t >( CURLOPT_APPEND, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_USE_EPRT", make_all< var_int_t >( CURLOPT_FTP_USE_EPRT, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_USE_EPSV", make_all< var_int_t >( CURLOPT_FTP_USE_EPSV, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_USE_PRET", make_all< var_int_t >( CURLOPT_FTP_USE_PRET, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_CREATE_MISSING_DIRS", make_all< var_int_t >( CURLOPT_FTP_CREATE_MISSING_DIRS, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_RESPONSE_TIMEOUT", make_all< var_int_t >( CURLOPT_FTP_RESPONSE_TIMEOUT, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_ALTERNATIVE_TO_USER", make_all< var_int_t >( CURLOPT_FTP_ALTERNATIVE_TO_USER, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_SKIP_PASV_IP", make_all< var_int_t >( CURLOPT_FTP_SKIP_PASV_IP, src_id, idx ) );
	src->add_nativevar( "OPT_FTPSSLAUTH", make_all< var_int_t >( CURLOPT_FTPSSLAUTH, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_SSL_CCC", make_all< var_int_t >( CURLOPT_FTP_SSL_CCC, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_ACCOUNT", make_all< var_int_t >( CURLOPT_FTP_ACCOUNT, src_id, idx ) );
	src->add_nativevar( "OPT_FTP_FILEMETHOD", make_all< var_int_t >( CURLOPT_FTP_FILEMETHOD, src_id, idx ) );

	// RTSP OPTIONS
	src->add_nativevar( "OPT_RTSP_REQUEST", make_all< var_int_t >( CURLOPT_RTSP_REQUEST, src_id, idx ) );
	src->add_nativevar( "OPT_RTSP_SESSION_ID", make_all< var_int_t >( CURLOPT_RTSP_SESSION_ID, src_id, idx ) );
	src->add_nativevar( "OPT_RTSP_STREAM_URI", make_all< var_int_t >( CURLOPT_RTSP_STREAM_URI, src_id, idx ) );
	src->add_nativevar( "OPT_RTSP_TRANSPORT", make_all< var_int_t >( CURLOPT_RTSP_TRANSPORT, src_id, idx ) );
	src->add_nativevar( "OPT_RTSP_CLIENT_CSEQ", make_all< var_int_t >( CURLOPT_RTSP_CLIENT_CSEQ, src_id, idx ) );
	src->add_nativevar( "OPT_RTSP_SERVER_CSEQ", make_all< var_int_t >( CURLOPT_RTSP_SERVER_CSEQ, src_id, idx ) );

	// PROTOCOL OPTIONS
	src->add_nativevar( "OPT_TRANSFERTEXT", make_all< var_int_t >( CURLOPT_TRANSFERTEXT, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_TRANSFER_MODE", make_all< var_int_t >( CURLOPT_PROXY_TRANSFER_MODE, src_id, idx ) );
	src->add_nativevar( "OPT_CRLF", make_all< var_int_t >( CURLOPT_CRLF, src_id, idx ) );
	src->add_nativevar( "OPT_RANGE", make_all< var_int_t >( CURLOPT_RANGE, src_id, idx ) );
	src->add_nativevar( "OPT_RESUME_FROM", make_all< var_int_t >( CURLOPT_RESUME_FROM, src_id, idx ) );
	src->add_nativevar( "OPT_RESUME_FROM_LARGE", make_all< var_int_t >( CURLOPT_RESUME_FROM_LARGE, src_id, idx ) );
#if CURL_AT_LEAST_VERSION(7, 63, 0)
	src->add_nativevar( "OPT_CURLU", make_all< var_int_t >( CURLOPT_CURLU, src_id, idx ) );
#endif
	src->add_nativevar( "OPT_CUSTOMREQUEST", make_all< var_int_t >( CURLOPT_CUSTOMREQUEST, src_id, idx ) );
	src->add_nativevar( "OPT_FILETIME", make_all< var_int_t >( CURLOPT_FILETIME, src_id, idx ) );
	src->add_nativevar( "OPT_DIRLISTONLY", make_all< var_int_t >( CURLOPT_DIRLISTONLY, src_id, idx ) );
	src->add_nativevar( "OPT_NOBODY", make_all< var_int_t >( CURLOPT_NOBODY, src_id, idx ) );
	src->add_nativevar( "OPT_INFILESIZE", make_all< var_int_t >( CURLOPT_INFILESIZE, src_id, idx ) );
	src->add_nativevar( "OPT_INFILESIZE_LARGE", make_all< var_int_t >( CURLOPT_INFILESIZE_LARGE, src_id, idx ) );
	src->add_nativevar( "OPT_UPLOAD", make_all< var_int_t >( CURLOPT_UPLOAD, src_id, idx ) );
	src->add_nativevar( "OPT_UPLOAD_BUFFERSIZE", make_all< var_int_t >( CURLOPT_UPLOAD_BUFFERSIZE, src_id, idx ) );
	src->add_nativevar( "OPT_MIMEPOST", make_all< var_int_t >( CURLOPT_MIMEPOST, src_id, idx ) );
	src->add_nativevar( "OPT_MAXFILESIZE", make_all< var_int_t >( CURLOPT_MAXFILESIZE, src_id, idx ) );
	src->add_nativevar( "OPT_MAXFILESIZE_LARGE", make_all< var_int_t >( CURLOPT_MAXFILESIZE_LARGE, src_id, idx ) );
	src->add_nativevar( "OPT_TIMECONDITION", make_all< var_int_t >( CURLOPT_TIMECONDITION, src_id, idx ) );
	src->add_nativevar( "OPT_TIMEVALUE", make_all< var_int_t >( CURLOPT_TIMEVALUE, src_id, idx ) );
	src->add_nativevar( "OPT_TIMEVALUE_LARGE", make_all< var_int_t >( CURLOPT_TIMEVALUE_LARGE, src_id, idx ) );

	// CONNECTION OPTIONS
	src->add_nativevar( "OPT_TIMEOUT", make_all< var_int_t >( CURLOPT_TIMEOUT, src_id, idx ) );
	src->add_nativevar( "OPT_TIMEOUT_MS", make_all< var_int_t >( CURLOPT_TIMEOUT_MS, src_id, idx ) );
	src->add_nativevar( "OPT_LOW_SPEED_LIMIT", make_all< var_int_t >( CURLOPT_LOW_SPEED_LIMIT, src_id, idx ) );
	src->add_nativevar( "OPT_LOW_SPEED_TIME", make_all< var_int_t >( CURLOPT_LOW_SPEED_TIME, src_id, idx ) );
	src->add_nativevar( "OPT_MAX_SEND_SPEED_LARGE", make_all< var_int_t >( CURLOPT_MAX_SEND_SPEED_LARGE, src_id, idx ) );
	src->add_nativevar( "OPT_MAX_RECV_SPEED_LARGE", make_all< var_int_t >( CURLOPT_MAX_RECV_SPEED_LARGE, src_id, idx ) );
	src->add_nativevar( "OPT_MAXCONNECTS", make_all< var_int_t >( CURLOPT_MAXCONNECTS, src_id, idx ) );
	src->add_nativevar( "OPT_FRESH_CONNECT", make_all< var_int_t >( CURLOPT_FRESH_CONNECT, src_id, idx ) );
	src->add_nativevar( "OPT_FORBID_REUSE", make_all< var_int_t >( CURLOPT_FORBID_REUSE, src_id, idx ) );
/* TODO: not available
	src->add_nativevar( "OPT_MAXAGE_CONN", make_all< var_int_t >( CURLOPT_MAXAGE_CONN, src_id, idx ) );
*/
	src->add_nativevar( "OPT_CONNECTTIMEOUT", make_all< var_int_t >( CURLOPT_CONNECTTIMEOUT, src_id, idx ) );
	src->add_nativevar( "OPT_CONNECTTIMEOUT_MS", make_all< var_int_t >( CURLOPT_CONNECTTIMEOUT_MS, src_id, idx ) );
	src->add_nativevar( "OPT_IPRESOLVE", make_all< var_int_t >( CURLOPT_IPRESOLVE, src_id, idx ) );
	src->add_nativevar( "OPT_CONNECT_ONLY", make_all< var_int_t >( CURLOPT_CONNECT_ONLY, src_id, idx ) );
	src->add_nativevar( "OPT_USE_SSL", make_all< var_int_t >( CURLOPT_USE_SSL, src_id, idx ) );
	src->add_nativevar( "OPT_RESOLVE", make_all< var_int_t >( CURLOPT_RESOLVE, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_INTERFACE", make_all< var_int_t >( CURLOPT_DNS_INTERFACE, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_LOCAL_IP4", make_all< var_int_t >( CURLOPT_DNS_LOCAL_IP4, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_LOCAL_IP6", make_all< var_int_t >( CURLOPT_DNS_LOCAL_IP6, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_SERVERS", make_all< var_int_t >( CURLOPT_DNS_SERVERS, src_id, idx ) );
	src->add_nativevar( "OPT_DNS_SHUFFLE_ADDRESSES", make_all< var_int_t >( CURLOPT_DNS_SHUFFLE_ADDRESSES, src_id, idx ) );
	src->add_nativevar( "OPT_ACCEPTTIMEOUT_MS", make_all< var_int_t >( CURLOPT_ACCEPTTIMEOUT_MS, src_id, idx ) );
	src->add_nativevar( "OPT_HAPPY_EYEBALLS_TIMEOUT_MS", make_all< var_int_t >( CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, src_id, idx ) );
	src->add_nativevar( "OPT_UPKEEP_INTERVAL_MS", make_all< var_int_t >( CURLOPT_UPKEEP_INTERVAL_MS, src_id, idx ) );

	// SSL and SECURITY OPTIONS
	src->add_nativevar( "OPT_SSLCERT", make_all< var_int_t >( CURLOPT_SSLCERT, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSLCERT", make_all< var_int_t >( CURLOPT_PROXY_SSLCERT, src_id, idx ) );
	src->add_nativevar( "OPT_SSLCERTTYPE", make_all< var_int_t >( CURLOPT_SSLCERTTYPE, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSLCERTTYPE", make_all< var_int_t >( CURLOPT_PROXY_SSLCERTTYPE, src_id, idx ) );
	src->add_nativevar( "OPT_SSLKEY", make_all< var_int_t >( CURLOPT_SSLKEY, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSLKEY", make_all< var_int_t >( CURLOPT_PROXY_SSLKEY, src_id, idx ) );
	src->add_nativevar( "OPT_SSLKEYTYPE", make_all< var_int_t >( CURLOPT_SSLKEYTYPE, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSLKEYTYPE", make_all< var_int_t >( CURLOPT_PROXY_SSLKEYTYPE, src_id, idx ) );
	src->add_nativevar( "OPT_KEYPASSWD", make_all< var_int_t >( CURLOPT_KEYPASSWD, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_KEYPASSWD", make_all< var_int_t >( CURLOPT_PROXY_KEYPASSWD, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_ENABLE_ALPN", make_all< var_int_t >( CURLOPT_SSL_ENABLE_ALPN, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_ENABLE_NPN", make_all< var_int_t >( CURLOPT_SSL_ENABLE_NPN, src_id, idx ) );
	src->add_nativevar( "OPT_SSLENGINE", make_all< var_int_t >( CURLOPT_SSLENGINE, src_id, idx ) );
	src->add_nativevar( "OPT_SSLENGINE_DEFAULT", make_all< var_int_t >( CURLOPT_SSLENGINE_DEFAULT, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_FALSESTART", make_all< var_int_t >( CURLOPT_SSL_FALSESTART, src_id, idx ) );
	src->add_nativevar( "OPT_SSLVERSION", make_all< var_int_t >( CURLOPT_SSLVERSION, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSLVERSION", make_all< var_int_t >( CURLOPT_PROXY_SSLVERSION, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_VERIFYHOST", make_all< var_int_t >( CURLOPT_SSL_VERIFYHOST, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSL_VERIFYHOST", make_all< var_int_t >( CURLOPT_PROXY_SSL_VERIFYHOST, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_VERIFYPEER", make_all< var_int_t >( CURLOPT_SSL_VERIFYPEER, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSL_VERIFYPEER", make_all< var_int_t >( CURLOPT_PROXY_SSL_VERIFYPEER, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_VERIFYSTATUS", make_all< var_int_t >( CURLOPT_SSL_VERIFYSTATUS, src_id, idx ) );
	src->add_nativevar( "OPT_CAINFO", make_all< var_int_t >( CURLOPT_CAINFO, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_CAINFO", make_all< var_int_t >( CURLOPT_PROXY_CAINFO, src_id, idx ) );
	src->add_nativevar( "OPT_ISSUERCERT", make_all< var_int_t >( CURLOPT_ISSUERCERT, src_id, idx ) );
	src->add_nativevar( "OPT_CAPATH", make_all< var_int_t >( CURLOPT_CAPATH, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_CAPATH", make_all< var_int_t >( CURLOPT_PROXY_CAPATH, src_id, idx ) );
	src->add_nativevar( "OPT_CRLFILE", make_all< var_int_t >( CURLOPT_CRLFILE, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_CRLFILE", make_all< var_int_t >( CURLOPT_PROXY_CRLFILE, src_id, idx ) );
	src->add_nativevar( "OPT_CERTINFO", make_all< var_int_t >( CURLOPT_CERTINFO, src_id, idx ) );
	src->add_nativevar( "OPT_PINNEDPUBLICKEY", make_all< var_int_t >( CURLOPT_PINNEDPUBLICKEY, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_PINNEDPUBLICKEY", make_all< var_int_t >( CURLOPT_PROXY_PINNEDPUBLICKEY, src_id, idx ) );
	src->add_nativevar( "OPT_RANDOM_FILE", make_all< var_int_t >( CURLOPT_RANDOM_FILE, src_id, idx ) );
	src->add_nativevar( "OPT_EGDSOCKET", make_all< var_int_t >( CURLOPT_EGDSOCKET, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_CIPHER_LIST", make_all< var_int_t >( CURLOPT_SSL_CIPHER_LIST, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSL_CIPHER_LIST", make_all< var_int_t >( CURLOPT_PROXY_SSL_CIPHER_LIST, src_id, idx ) );
	src->add_nativevar( "OPT_TLS13_CIPHERS", make_all< var_int_t >( CURLOPT_TLS13_CIPHERS, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_TLS13_CIPHERS", make_all< var_int_t >( CURLOPT_PROXY_TLS13_CIPHERS, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_SESSIONID_CACHE", make_all< var_int_t >( CURLOPT_SSL_SESSIONID_CACHE, src_id, idx ) );
	src->add_nativevar( "OPT_SSL_OPTIONS", make_all< var_int_t >( CURLOPT_SSL_OPTIONS, src_id, idx ) );
	src->add_nativevar( "OPT_PROXY_SSL_OPTIONS", make_all< var_int_t >( CURLOPT_PROXY_SSL_OPTIONS, src_id, idx ) );
	src->add_nativevar( "OPT_KRBLEVEL", make_all< var_int_t >( CURLOPT_KRBLEVEL, src_id, idx ) );
	src->add_nativevar( "OPT_GSSAPI_DELEGATION", make_all< var_int_t >( CURLOPT_GSSAPI_DELEGATION, src_id, idx ) );

	// SSH OPTIONS
	src->add_nativevar( "OPT_SSH_AUTH_TYPES", make_all< var_int_t >( CURLOPT_SSH_AUTH_TYPES, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_COMPRESSION", make_all< var_int_t >( CURLOPT_SSH_COMPRESSION, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_HOST_PUBLIC_KEY_MD5", make_all< var_int_t >( CURLOPT_SSH_HOST_PUBLIC_KEY_MD5, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_PUBLIC_KEYFILE", make_all< var_int_t >( CURLOPT_SSH_PUBLIC_KEYFILE, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_PRIVATE_KEYFILE", make_all< var_int_t >( CURLOPT_SSH_PRIVATE_KEYFILE, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_KNOWNHOSTS", make_all< var_int_t >( CURLOPT_SSH_KNOWNHOSTS, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_KEYFUNCTION", make_all< var_int_t >( CURLOPT_SSH_KEYFUNCTION, src_id, idx ) );
	src->add_nativevar( "OPT_SSH_KEYDATA", make_all< var_int_t >( CURLOPT_SSH_KEYDATA, src_id, idx ) );

	// OTHER OPTIONS
	src->add_nativevar( "OPT_PRIVATE", make_all< var_int_t >( CURLOPT_PRIVATE, src_id, idx ) );
	src->add_nativevar( "OPT_SHARE", make_all< var_int_t >( CURLOPT_SHARE, src_id, idx ) );
	src->add_nativevar( "OPT_NEW_FILE_PERMS", make_all< var_int_t >( CURLOPT_NEW_FILE_PERMS, src_id, idx ) );
	src->add_nativevar( "OPT_NEW_DIRECTORY_PERMS", make_all< var_int_t >( CURLOPT_NEW_DIRECTORY_PERMS, src_id, idx ) );

	// TELNET OPTIONS
	src->add_nativevar( "OPT_TELNETOPTIONS", make_all< var_int_t >( CURLOPT_TELNETOPTIONS, src_id, idx ) );

	return true;
}

DEINIT_MODULE( curl )
{
	var_dref( progress_callback );
}

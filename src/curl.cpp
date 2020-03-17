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
	const std::string & src_name = src->src()->path();

	src->add_nativefn( "new_easy", feral_curl_easy_init );
	src->add_nativefn( "set_opt_native", feral_curl_easy_set_opt_native, { "", "", "" } );
	src->add_nativefn( "strerr", feral_curl_easy_str_err_from_int, { "" } );
	src->add_nativefn( "set_default_progress_func_native", feral_curl_set_default_progress_func, { "" } );
	src->add_nativefn( "set_default_progress_func_tick_native", feral_curl_set_default_progress_func_tick, { "" } );

	// get the type id for curl type (register_type)
	curl_typeid = vm.register_new_type( "var_curl_t", "curl_t" );

	vm.add_typefn( curl_typeid, "perform", new var_fn_t( src_name, {}, {}, { .native = feral_curl_easy_perform }, 0, 0 ), false );

	// all the enum values

	// CURLcode
	src->add_nativevar( "E_OK", make< var_int_t >( CURLE_OK ) );
	src->add_nativevar( "E_UNSUPPORTED_PROTOCOL", make< var_int_t >( CURLE_UNSUPPORTED_PROTOCOL ) );
	src->add_nativevar( "E_FAILED_INIT", make< var_int_t >( CURLE_FAILED_INIT ) );
	src->add_nativevar( "E_URL_MALFORMAT", make< var_int_t >( CURLE_URL_MALFORMAT ) );
	src->add_nativevar( "E_NOT_BUILT_IN", make< var_int_t >( CURLE_NOT_BUILT_IN ) );
	src->add_nativevar( "E_COULDNT_RESOLVE_PROXY", make< var_int_t >( CURLE_COULDNT_RESOLVE_PROXY ) );
	src->add_nativevar( "E_COULDNT_RESOLVE_HOST", make< var_int_t >( CURLE_COULDNT_RESOLVE_HOST ) );
	src->add_nativevar( "E_COULDNT_CONNECT", make< var_int_t >( CURLE_COULDNT_CONNECT ) );
	src->add_nativevar( "E_WEIRD_SERVER_REPLY", make< var_int_t >( CURLE_WEIRD_SERVER_REPLY ) );
	src->add_nativevar( "E_REMOTE_ACCESS_DENIED", make< var_int_t >( CURLE_REMOTE_ACCESS_DENIED ) );
	src->add_nativevar( "E_FTP_ACCEPT_FAILED", make< var_int_t >( CURLE_FTP_ACCEPT_FAILED ) );
	src->add_nativevar( "E_FTP_WEIRD_PASS_REPLY", make< var_int_t >( CURLE_FTP_WEIRD_PASS_REPLY ) );
	src->add_nativevar( "E_FTP_ACCEPT_TIMEOUT", make< var_int_t >( CURLE_FTP_ACCEPT_TIMEOUT ) );
	src->add_nativevar( "E_FTP_WEIRD_PASV_REPLY", make< var_int_t >( CURLE_FTP_WEIRD_PASV_REPLY ) );
	src->add_nativevar( "E_FTP_WEIRD_227_FORMAT", make< var_int_t >( CURLE_FTP_WEIRD_227_FORMAT ) );
	src->add_nativevar( "E_FTP_CANT_GET_HOST", make< var_int_t >( CURLE_FTP_CANT_GET_HOST ) );
	src->add_nativevar( "E_HTTP2", make< var_int_t >( CURLE_HTTP2 ) );
	src->add_nativevar( "E_FTP_COULDNT_SET_TYPE", make< var_int_t >( CURLE_FTP_COULDNT_SET_TYPE ) );
	src->add_nativevar( "E_PARTIAL_FILE", make< var_int_t >( CURLE_PARTIAL_FILE ) );
	src->add_nativevar( "E_FTP_COULDNT_RETR_FILE", make< var_int_t >( CURLE_FTP_COULDNT_RETR_FILE ) );
	src->add_nativevar( "E_OBSOLETE20", make< var_int_t >( CURLE_OBSOLETE20 ) );
	src->add_nativevar( "E_QUOTE_ERROR", make< var_int_t >( CURLE_QUOTE_ERROR ) );
	src->add_nativevar( "E_HTTP_RETURNED_ERROR", make< var_int_t >( CURLE_HTTP_RETURNED_ERROR ) );
	src->add_nativevar( "E_WRITE_ERROR", make< var_int_t >( CURLE_WRITE_ERROR ) );
	src->add_nativevar( "E_OBSOLETE24", make< var_int_t >( CURLE_OBSOLETE24 ) );
	src->add_nativevar( "E_UPLOAD_FAILED", make< var_int_t >( CURLE_UPLOAD_FAILED ) );
	src->add_nativevar( "E_READ_ERROR", make< var_int_t >( CURLE_READ_ERROR ) );
	src->add_nativevar( "E_OUT_OF_MEMORY", make< var_int_t >( CURLE_OUT_OF_MEMORY ) );
	src->add_nativevar( "E_OPERATION_TIMEDOUT", make< var_int_t >( CURLE_OPERATION_TIMEDOUT ) );
	src->add_nativevar( "E_OBSOLETE29", make< var_int_t >( CURLE_OBSOLETE29 ) );
	src->add_nativevar( "E_FTP_PORT_FAILED", make< var_int_t >( CURLE_FTP_PORT_FAILED ) );
	src->add_nativevar( "E_FTP_COULDNT_USE_REST", make< var_int_t >( CURLE_FTP_COULDNT_USE_REST ) );
	src->add_nativevar( "E_OBSOLETE32", make< var_int_t >( CURLE_OBSOLETE32 ) );
	src->add_nativevar( "E_RANGE_ERROR", make< var_int_t >( CURLE_RANGE_ERROR ) );
	src->add_nativevar( "E_HTTP_POST_ERROR", make< var_int_t >( CURLE_HTTP_POST_ERROR ) );
	src->add_nativevar( "E_SSL_CONNECT_ERROR", make< var_int_t >( CURLE_SSL_CONNECT_ERROR ) );
	src->add_nativevar( "E_BAD_DOWNLOAD_RESUME", make< var_int_t >( CURLE_BAD_DOWNLOAD_RESUME ) );
	src->add_nativevar( "E_FILE_COULDNT_READ_FILE", make< var_int_t >( CURLE_FILE_COULDNT_READ_FILE ) );
	src->add_nativevar( "E_LDAP_CANNOT_BIND", make< var_int_t >( CURLE_LDAP_CANNOT_BIND ) );
	src->add_nativevar( "E_LDAP_SEARCH_FAILED", make< var_int_t >( CURLE_LDAP_SEARCH_FAILED ) );
	src->add_nativevar( "E_OBSOLETE40", make< var_int_t >( CURLE_OBSOLETE40 ) );
	src->add_nativevar( "E_FUNCTION_NOT_FOUND", make< var_int_t >( CURLE_FUNCTION_NOT_FOUND ) );
	src->add_nativevar( "E_ABORTED_BY_CALLBACK", make< var_int_t >( CURLE_ABORTED_BY_CALLBACK ) );
	src->add_nativevar( "E_BAD_FUNCTION_ARGUMENT", make< var_int_t >( CURLE_BAD_FUNCTION_ARGUMENT ) );
	src->add_nativevar( "E_OBSOLETE44", make< var_int_t >( CURLE_OBSOLETE44 ) );
	src->add_nativevar( "E_INTERFACE_FAILED", make< var_int_t >( CURLE_INTERFACE_FAILED ) );
	src->add_nativevar( "E_OBSOLETE46", make< var_int_t >( CURLE_OBSOLETE46 ) );
	src->add_nativevar( "E_TOO_MANY_REDIRECTS", make< var_int_t >( CURLE_TOO_MANY_REDIRECTS ) );
	src->add_nativevar( "E_UNKNOWN_OPTION", make< var_int_t >( CURLE_UNKNOWN_OPTION ) );
	src->add_nativevar( "E_TELNET_OPTION_SYNTAX", make< var_int_t >( CURLE_TELNET_OPTION_SYNTAX ) );
	src->add_nativevar( "E_OBSOLETE50", make< var_int_t >( CURLE_OBSOLETE50 ) );
	src->add_nativevar( "E_OBSOLETE51", make< var_int_t >( CURLE_OBSOLETE51 ) );
	src->add_nativevar( "E_GOT_NOTHING", make< var_int_t >( CURLE_GOT_NOTHING ) );
	src->add_nativevar( "E_SSL_ENGINE_NOTFOUND", make< var_int_t >( CURLE_SSL_ENGINE_NOTFOUND ) );
	src->add_nativevar( "E_SSL_ENGINE_SETFAILED", make< var_int_t >( CURLE_SSL_ENGINE_SETFAILED ) );
	src->add_nativevar( "E_SEND_ERROR", make< var_int_t >( CURLE_SEND_ERROR ) );
	src->add_nativevar( "E_RECV_ERROR", make< var_int_t >( CURLE_RECV_ERROR ) );
	src->add_nativevar( "E_OBSOLETE57", make< var_int_t >( CURLE_OBSOLETE57 ) );
	src->add_nativevar( "E_SSL_CERTPROBLEM", make< var_int_t >( CURLE_SSL_CERTPROBLEM ) );
	src->add_nativevar( "E_SSL_CIPHER", make< var_int_t >( CURLE_SSL_CIPHER ) );
	src->add_nativevar( "E_PEER_FAILED_VERIFICATION", make< var_int_t >( CURLE_PEER_FAILED_VERIFICATION ) );
	src->add_nativevar( "E_BAD_CONTENT_ENCODING", make< var_int_t >( CURLE_BAD_CONTENT_ENCODING ) );
	src->add_nativevar( "E_LDAP_INVALID_URL", make< var_int_t >( CURLE_LDAP_INVALID_URL ) );
	src->add_nativevar( "E_FILESIZE_EXCEEDED", make< var_int_t >( CURLE_FILESIZE_EXCEEDED ) );
	src->add_nativevar( "E_USE_SSL_FAILED", make< var_int_t >( CURLE_USE_SSL_FAILED ) );
	src->add_nativevar( "E_SEND_FAIL_REWIND", make< var_int_t >( CURLE_SEND_FAIL_REWIND ) );
	src->add_nativevar( "E_SSL_ENGINE_INITFAILED", make< var_int_t >( CURLE_SSL_ENGINE_INITFAILED ) );
	src->add_nativevar( "E_LOGIN_DENIED", make< var_int_t >( CURLE_LOGIN_DENIED ) );
	src->add_nativevar( "E_TFTP_NOTFOUND", make< var_int_t >( CURLE_TFTP_NOTFOUND ) );
	src->add_nativevar( "E_TFTP_PERM", make< var_int_t >( CURLE_TFTP_PERM ) );
	src->add_nativevar( "E_REMOTE_DISK_FULL", make< var_int_t >( CURLE_REMOTE_DISK_FULL ) );
	src->add_nativevar( "E_TFTP_ILLEGAL", make< var_int_t >( CURLE_TFTP_ILLEGAL ) );
	src->add_nativevar( "E_TFTP_UNKNOWNID", make< var_int_t >( CURLE_TFTP_UNKNOWNID ) );
	src->add_nativevar( "E_REMOTE_FILE_EXISTS", make< var_int_t >( CURLE_REMOTE_FILE_EXISTS ) );
	src->add_nativevar( "E_TFTP_NOSUCHUSER", make< var_int_t >( CURLE_TFTP_NOSUCHUSER ) );
	src->add_nativevar( "E_CONV_FAILED", make< var_int_t >( CURLE_CONV_FAILED ) );
	src->add_nativevar( "E_CONV_REQD", make< var_int_t >( CURLE_CONV_REQD ) );
	src->add_nativevar( "E_SSL_CACERT_BADFILE", make< var_int_t >( CURLE_SSL_CACERT_BADFILE ) );
	src->add_nativevar( "E_REMOTE_FILE_NOT_FOUND", make< var_int_t >( CURLE_REMOTE_FILE_NOT_FOUND ) );
	src->add_nativevar( "E_SSH", make< var_int_t >( CURLE_SSH ) );
	src->add_nativevar( "E_SSL_SHUTDOWN_FAILED", make< var_int_t >( CURLE_SSL_SHUTDOWN_FAILED ) );
	src->add_nativevar( "E_AGAIN", make< var_int_t >( CURLE_AGAIN ) );
	src->add_nativevar( "E_SSL_CRL_BADFILE", make< var_int_t >( CURLE_SSL_CRL_BADFILE ) );
	src->add_nativevar( "E_SSL_ISSUER_ERROR", make< var_int_t >( CURLE_SSL_ISSUER_ERROR ) );
	src->add_nativevar( "E_FTP_PRET_FAILED", make< var_int_t >( CURLE_FTP_PRET_FAILED ) );
	src->add_nativevar( "E_RTSP_CSEQ_ERROR", make< var_int_t >( CURLE_RTSP_CSEQ_ERROR ) );
	src->add_nativevar( "E_RTSP_SESSION_ERROR", make< var_int_t >( CURLE_RTSP_SESSION_ERROR ) );
	src->add_nativevar( "E_FTP_BAD_FILE_LIST", make< var_int_t >( CURLE_FTP_BAD_FILE_LIST ) );
	src->add_nativevar( "E_CHUNK_FAILED", make< var_int_t >( CURLE_CHUNK_FAILED ) );
	src->add_nativevar( "E_NO_CONNECTION_AVAILABLE", make< var_int_t >( CURLE_NO_CONNECTION_AVAILABLE ) );
	src->add_nativevar( "E_SSL_PINNEDPUBKEYNOTMATCH", make< var_int_t >( CURLE_SSL_PINNEDPUBKEYNOTMATCH ) );
	src->add_nativevar( "E_SSL_INVALIDCERTSTATUS", make< var_int_t >( CURLE_SSL_INVALIDCERTSTATUS ) );
	src->add_nativevar( "E_HTTP2_STREAM", make< var_int_t >( CURLE_HTTP2_STREAM ) );
	src->add_nativevar( "E_RECURSIVE_API_CALL", make< var_int_t >( CURLE_RECURSIVE_API_CALL ) );
/* TODO: Seem to be in different version than available
	src->add_nativevar( "E_AUTH_ERROR", make< var_int_t >( CURLE_AUTH_ERROR ) );
	src->add_nativevar( "E_HTTP3", make< var_int_t >( CURLE_HTTP3 ) );
	src->add_nativevar( "E_QUIC_CONNECT_ERROR", make< var_int_t >( CURLE_QUIC_CONNECT_ERROR ) );
*/

	// CURLMcode
	src->add_nativevar( "M_CALL_MULTI_PERFORM", make< var_int_t >( CURLM_CALL_MULTI_PERFORM ) );
	src->add_nativevar( "M_OK", make< var_int_t >( CURLM_OK ) );
	src->add_nativevar( "M_BAD_HANDLE", make< var_int_t >( CURLM_BAD_HANDLE ) );
	src->add_nativevar( "M_BAD_EASY_HANDLE", make< var_int_t >( CURLM_BAD_EASY_HANDLE ) );
	src->add_nativevar( "M_OUT_OF_MEMORY", make< var_int_t >( CURLM_OUT_OF_MEMORY ) );
	src->add_nativevar( "M_INTERNAL_ERROR", make< var_int_t >( CURLM_INTERNAL_ERROR ) );
	src->add_nativevar( "M_BAD_SOCKET", make< var_int_t >( CURLM_BAD_SOCKET ) );
	src->add_nativevar( "M_UNKNOWN_OPTION", make< var_int_t >( CURLM_UNKNOWN_OPTION ) );
	src->add_nativevar( "M_ADDED_ALREADY", make< var_int_t >( CURLM_ADDED_ALREADY ) );
	src->add_nativevar( "M_RECURSIVE_API_CALL", make< var_int_t >( CURLM_RECURSIVE_API_CALL ) );
/* TODO: Seem to be in different version than available
	src->add_nativevar( "WAKEUP_FAILURE", make< var_int_t >( CURLM_WAKEUP_FAILURE ) );
	src->add_nativevar( "BAD_FUNCTION_ARGUMENT", make< var_int_t >( CURLM_BAD_FUNCTION_ARGUMENT ) );
*/

	// CURLSHcode
	src->add_nativevar( "SHE_OK", make< var_int_t >( CURLSHE_OK ) );
	src->add_nativevar( "SHE_BAD_OPTION", make< var_int_t >( CURLSHE_BAD_OPTION ) );
	src->add_nativevar( "SHE_IN_USE", make< var_int_t >( CURLSHE_IN_USE ) );
	src->add_nativevar( "SHE_INVALID", make< var_int_t >( CURLSHE_INVALID ) );
	src->add_nativevar( "SHE_NOMEM", make< var_int_t >( CURLSHE_NOMEM ) );
	src->add_nativevar( "SHE_NOT_BUILT_IN", make< var_int_t >( CURLSHE_NOT_BUILT_IN ) );

	// CURLUcode
	src->add_nativevar( "UE_OK", make< var_int_t >( CURLUE_OK ) );
	src->add_nativevar( "UE_BAD_HANDLE", make< var_int_t >( CURLUE_BAD_HANDLE ) );
	src->add_nativevar( "UE_BAD_PARTPOINTER", make< var_int_t >( CURLUE_BAD_PARTPOINTER ) );
	src->add_nativevar( "UE_MALFORMED_INPUT", make< var_int_t >( CURLUE_MALFORMED_INPUT ) );
	src->add_nativevar( "UE_BAD_PORT_NUMBER", make< var_int_t >( CURLUE_BAD_PORT_NUMBER ) );
	src->add_nativevar( "UE_UNSUPPORTED_SCHEME", make< var_int_t >( CURLUE_UNSUPPORTED_SCHEME ) );
	src->add_nativevar( "UE_URLDECODE", make< var_int_t >( CURLUE_URLDECODE ) );
	src->add_nativevar( "UE_OUT_OF_MEMORY", make< var_int_t >( CURLUE_OUT_OF_MEMORY ) );
	src->add_nativevar( "UE_USER_NOT_ALLOWED", make< var_int_t >( CURLUE_USER_NOT_ALLOWED ) );
	src->add_nativevar( "UE_UNKNOWN_PART", make< var_int_t >( CURLUE_UNKNOWN_PART ) );
	src->add_nativevar( "UE_NO_SCHEME", make< var_int_t >( CURLUE_NO_SCHEME ) );
	src->add_nativevar( "UE_NO_USER", make< var_int_t >( CURLUE_NO_USER ) );
	src->add_nativevar( "UE_NO_PASSWORD", make< var_int_t >( CURLUE_NO_PASSWORD ) );
	src->add_nativevar( "UE_NO_OPTIONS", make< var_int_t >( CURLUE_NO_OPTIONS ) );
	src->add_nativevar( "UE_NO_HOST", make< var_int_t >( CURLUE_NO_HOST ) );
	src->add_nativevar( "UE_NO_PORT", make< var_int_t >( CURLUE_NO_PORT ) );
	src->add_nativevar( "UE_NO_QUERY", make< var_int_t >( CURLUE_NO_QUERY ) );
	src->add_nativevar( "UE_NO_FRAGMENT", make< var_int_t >( CURLUE_NO_FRAGMENT ) );

	// EASY_OPTS

	// BEHAVIOR OPTIONS
	src->add_nativevar( "OPT_VERBOSE", make< var_int_t >( CURLOPT_VERBOSE ) );
	src->add_nativevar( "OPT_HEADER", make< var_int_t >( CURLOPT_HEADER ) );
	src->add_nativevar( "OPT_NOPROGRESS", make< var_int_t >( CURLOPT_NOPROGRESS ) );
	src->add_nativevar( "OPT_NOSIGNAL", make< var_int_t >( CURLOPT_NOSIGNAL ) );
	src->add_nativevar( "OPT_WILDCARDMATCH", make< var_int_t >( CURLOPT_WILDCARDMATCH ) );

	// CALLBACK OPTIONS
	src->add_nativevar( "OPT_WRITEFUNCTION", make< var_int_t >( CURLOPT_WRITEFUNCTION ) );
	src->add_nativevar( "OPT_WRITEDATA", make< var_int_t >( CURLOPT_WRITEDATA ) );
	src->add_nativevar( "OPT_READFUNCTION", make< var_int_t >( CURLOPT_READFUNCTION ) );
	src->add_nativevar( "OPT_READDATA", make< var_int_t >( CURLOPT_READDATA ) );
	src->add_nativevar( "OPT_IOCTLFUNCTION", make< var_int_t >( CURLOPT_IOCTLFUNCTION ) );
	src->add_nativevar( "OPT_IOCTLDATA", make< var_int_t >( CURLOPT_IOCTLDATA ) );
	src->add_nativevar( "OPT_SEEKFUNCTION", make< var_int_t >( CURLOPT_SEEKFUNCTION ) );
	src->add_nativevar( "OPT_SEEKDATA", make< var_int_t >( CURLOPT_SEEKDATA ) );
	src->add_nativevar( "OPT_SOCKOPTFUNCTION", make< var_int_t >( CURLOPT_SOCKOPTFUNCTION ) );
	src->add_nativevar( "OPT_SOCKOPTDATA", make< var_int_t >( CURLOPT_SOCKOPTDATA ) );
	src->add_nativevar( "OPT_OPENSOCKETFUNCTION", make< var_int_t >( CURLOPT_OPENSOCKETFUNCTION ) );
	src->add_nativevar( "OPT_OPENSOCKETDATA", make< var_int_t >( CURLOPT_OPENSOCKETDATA ) );
	src->add_nativevar( "OPT_CLOSESOCKETFUNCTION", make< var_int_t >( CURLOPT_CLOSESOCKETFUNCTION ) );
	src->add_nativevar( "OPT_CLOSESOCKETDATA", make< var_int_t >( CURLOPT_CLOSESOCKETDATA ) );
	src->add_nativevar( "OPT_PROGRESSFUNCTION", make< var_int_t >( CURLOPT_PROGRESSFUNCTION ) );
	src->add_nativevar( "OPT_PROGRESSDATA", make< var_int_t >( CURLOPT_PROGRESSDATA ) );
	src->add_nativevar( "OPT_XFERINFOFUNCTION", make< var_int_t >( CURLOPT_XFERINFOFUNCTION ) );
	src->add_nativevar( "OPT_XFERINFODATA", make< var_int_t >( CURLOPT_XFERINFODATA ) );
	src->add_nativevar( "OPT_HEADERFUNCTION", make< var_int_t >( CURLOPT_HEADERFUNCTION ) );
	src->add_nativevar( "OPT_HEADERDATA", make< var_int_t >( CURLOPT_HEADERDATA ) );
	src->add_nativevar( "OPT_DEBUGFUNCTION", make< var_int_t >( CURLOPT_DEBUGFUNCTION ) );
	src->add_nativevar( "OPT_DEBUGDATA", make< var_int_t >( CURLOPT_DEBUGDATA ) );
	src->add_nativevar( "OPT_SSL_CTX_FUNCTION", make< var_int_t >( CURLOPT_SSL_CTX_FUNCTION ) );
	src->add_nativevar( "OPT_SSL_CTX_DATA", make< var_int_t >( CURLOPT_SSL_CTX_DATA ) );
	src->add_nativevar( "OPT_CONV_TO_NETWORK_FUNCTION", make< var_int_t >( CURLOPT_CONV_TO_NETWORK_FUNCTION ) );
	src->add_nativevar( "OPT_CONV_FROM_NETWORK_FUNCTION", make< var_int_t >( CURLOPT_CONV_FROM_NETWORK_FUNCTION ) );
	src->add_nativevar( "OPT_CONV_FROM_UTF8_FUNCTION", make< var_int_t >( CURLOPT_CONV_FROM_UTF8_FUNCTION ) );
	src->add_nativevar( "OPT_INTERLEAVEFUNCTION", make< var_int_t >( CURLOPT_INTERLEAVEFUNCTION ) );
	src->add_nativevar( "OPT_INTERLEAVEDATA", make< var_int_t >( CURLOPT_INTERLEAVEDATA ) );
	src->add_nativevar( "OPT_CHUNK_BGN_FUNCTION", make< var_int_t >( CURLOPT_CHUNK_BGN_FUNCTION ) );
	src->add_nativevar( "OPT_CHUNK_END_FUNCTION", make< var_int_t >( CURLOPT_CHUNK_END_FUNCTION ) );
	src->add_nativevar( "OPT_CHUNK_DATA", make< var_int_t >( CURLOPT_CHUNK_DATA ) );
	src->add_nativevar( "OPT_FNMATCH_FUNCTION", make< var_int_t >( CURLOPT_FNMATCH_FUNCTION ) );
	src->add_nativevar( "OPT_FNMATCH_DATA", make< var_int_t >( CURLOPT_FNMATCH_DATA ) );
	src->add_nativevar( "OPT_SUPPRESS_CONNECT_HEADERS", make< var_int_t >( CURLOPT_SUPPRESS_CONNECT_HEADERS ) );
	src->add_nativevar( "OPT_RESOLVER_START_FUNCTION", make< var_int_t >( CURLOPT_RESOLVER_START_FUNCTION ) );
	src->add_nativevar( "OPT_RESOLVER_START_DATA", make< var_int_t >( CURLOPT_RESOLVER_START_DATA ) );

	// ERROR OPTIONS
	src->add_nativevar( "OPT_ERRORBUFFER", make< var_int_t >( CURLOPT_ERRORBUFFER ) );
	src->add_nativevar( "OPT_STDERR", make< var_int_t >( CURLOPT_STDERR ) );
	src->add_nativevar( "OPT_FAILONERROR", make< var_int_t >( CURLOPT_FAILONERROR ) );
	src->add_nativevar( "OPT_KEEP_SENDING_ON_ERROR", make< var_int_t >( CURLOPT_KEEP_SENDING_ON_ERROR ) );

	// NETWORK OPTIONS
	src->add_nativevar( "OPT_URL", make< var_int_t >( CURLOPT_URL ) );
	src->add_nativevar( "OPT_PATH_AS_IS", make< var_int_t >( CURLOPT_PATH_AS_IS ) );
	src->add_nativevar( "OPT_PROTOCOLS", make< var_int_t >( CURLOPT_PROTOCOLS ) );
	src->add_nativevar( "OPT_REDIR_PROTOCOLS", make< var_int_t >( CURLOPT_REDIR_PROTOCOLS ) );
	src->add_nativevar( "OPT_DEFAULT_PROTOCOL", make< var_int_t >( CURLOPT_DEFAULT_PROTOCOL ) );
	src->add_nativevar( "OPT_PROXY", make< var_int_t >( CURLOPT_PROXY ) );
	src->add_nativevar( "OPT_PRE_PROXY", make< var_int_t >( CURLOPT_PRE_PROXY ) );
	src->add_nativevar( "OPT_PROXYPORT", make< var_int_t >( CURLOPT_PROXYPORT ) );
	src->add_nativevar( "OPT_PROXYTYPE", make< var_int_t >( CURLOPT_PROXYTYPE ) );
	src->add_nativevar( "OPT_NOPROXY", make< var_int_t >( CURLOPT_NOPROXY ) );
	src->add_nativevar( "OPT_HTTPPROXYTUNNEL", make< var_int_t >( CURLOPT_HTTPPROXYTUNNEL ) );
	src->add_nativevar( "OPT_CONNECT_TO", make< var_int_t >( CURLOPT_CONNECT_TO ) );
	src->add_nativevar( "OPT_SOCKS5_AUTH", make< var_int_t >( CURLOPT_SOCKS5_AUTH ) );
	src->add_nativevar( "OPT_SOCKS5_GSSAPI_SERVICE", make< var_int_t >( CURLOPT_SOCKS5_GSSAPI_SERVICE ) );
	src->add_nativevar( "OPT_SOCKS5_GSSAPI_NEC", make< var_int_t >( CURLOPT_SOCKS5_GSSAPI_NEC ) );
	src->add_nativevar( "OPT_PROXY_SERVICE_NAME", make< var_int_t >( CURLOPT_PROXY_SERVICE_NAME ) );
	src->add_nativevar( "OPT_HAPROXYPROTOCOL", make< var_int_t >( CURLOPT_HAPROXYPROTOCOL ) );
	src->add_nativevar( "OPT_SERVICE_NAME", make< var_int_t >( CURLOPT_SERVICE_NAME ) );
	src->add_nativevar( "OPT_INTERFACE", make< var_int_t >( CURLOPT_INTERFACE ) );
	src->add_nativevar( "OPT_LOCALPORT", make< var_int_t >( CURLOPT_LOCALPORT ) );
	src->add_nativevar( "OPT_LOCALPORTRANGE", make< var_int_t >( CURLOPT_LOCALPORTRANGE ) );
	src->add_nativevar( "OPT_DNS_CACHE_TIMEOUT", make< var_int_t >( CURLOPT_DNS_CACHE_TIMEOUT ) );
	src->add_nativevar( "OPT_DNS_USE_GLOBAL_CACHE", make< var_int_t >( CURLOPT_DNS_USE_GLOBAL_CACHE ) );
	src->add_nativevar( "OPT_DOH_URL", make< var_int_t >( CURLOPT_DOH_URL ) );
	src->add_nativevar( "OPT_BUFFERSIZE", make< var_int_t >( CURLOPT_BUFFERSIZE ) );
	src->add_nativevar( "OPT_PORT", make< var_int_t >( CURLOPT_PORT ) );
	src->add_nativevar( "OPT_TCP_FASTOPEN", make< var_int_t >( CURLOPT_TCP_FASTOPEN ) );
	src->add_nativevar( "OPT_TCP_NODELAY", make< var_int_t >( CURLOPT_TCP_NODELAY ) );
	src->add_nativevar( "OPT_ADDRESS_SCOPE", make< var_int_t >( CURLOPT_ADDRESS_SCOPE ) );
	src->add_nativevar( "OPT_TCP_KEEPALIVE", make< var_int_t >( CURLOPT_TCP_KEEPALIVE ) );
	src->add_nativevar( "OPT_TCP_KEEPIDLE", make< var_int_t >( CURLOPT_TCP_KEEPIDLE ) );
	src->add_nativevar( "OPT_TCP_KEEPINTVL", make< var_int_t >( CURLOPT_TCP_KEEPINTVL ) );
	src->add_nativevar( "OPT_UNIX_SOCKET_PATH", make< var_int_t >( CURLOPT_UNIX_SOCKET_PATH ) );
	src->add_nativevar( "OPT_ABSTRACT_UNIX_SOCKET", make< var_int_t >( CURLOPT_ABSTRACT_UNIX_SOCKET ) );

	// NAMES and PASSWORDS OPTIONS (Authentication)
	src->add_nativevar( "OPT_NETRC", make< var_int_t >( CURLOPT_NETRC ) );
	src->add_nativevar( "OPT_NETRC_FILE", make< var_int_t >( CURLOPT_NETRC_FILE ) );
	src->add_nativevar( "OPT_USERPWD", make< var_int_t >( CURLOPT_USERPWD ) );
	src->add_nativevar( "OPT_PROXYUSERPWD", make< var_int_t >( CURLOPT_PROXYUSERPWD ) );
	src->add_nativevar( "OPT_USERNAME", make< var_int_t >( CURLOPT_USERNAME ) );
	src->add_nativevar( "OPT_PASSWORD", make< var_int_t >( CURLOPT_PASSWORD ) );
	src->add_nativevar( "OPT_LOGIN_OPTIONS", make< var_int_t >( CURLOPT_LOGIN_OPTIONS ) );
	src->add_nativevar( "OPT_PROXYUSERNAME", make< var_int_t >( CURLOPT_PROXYUSERNAME ) );
	src->add_nativevar( "OPT_PROXYPASSWORD", make< var_int_t >( CURLOPT_PROXYPASSWORD ) );
	src->add_nativevar( "OPT_HTTPAUTH", make< var_int_t >( CURLOPT_HTTPAUTH ) );
	src->add_nativevar( "OPT_TLSAUTH_USERNAME", make< var_int_t >( CURLOPT_TLSAUTH_USERNAME ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_USERNAME", make< var_int_t >( CURLOPT_PROXY_TLSAUTH_USERNAME ) );
	src->add_nativevar( "OPT_TLSAUTH_PASSWORD", make< var_int_t >( CURLOPT_TLSAUTH_PASSWORD ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_PASSWORD", make< var_int_t >( CURLOPT_PROXY_TLSAUTH_PASSWORD ) );
	src->add_nativevar( "OPT_TLSAUTH_TYPE", make< var_int_t >( CURLOPT_TLSAUTH_TYPE ) );
	src->add_nativevar( "OPT_PROXY_TLSAUTH_TYPE", make< var_int_t >( CURLOPT_PROXY_TLSAUTH_TYPE ) );
	src->add_nativevar( "OPT_PROXYAUTH", make< var_int_t >( CURLOPT_PROXYAUTH ) );
/* TODO: not available
	src->add_nativevar( "OPT_SASL_AUTHZID", make< var_int_t >( CURLOPT_SASL_AUTHZID ) );
*/
	src->add_nativevar( "OPT_SASL_IR", make< var_int_t >( CURLOPT_SASL_IR ) );
	src->add_nativevar( "OPT_XOAUTH2_BEARER", make< var_int_t >( CURLOPT_XOAUTH2_BEARER ) );
	src->add_nativevar( "OPT_DISALLOW_USERNAME_IN_URL", make< var_int_t >( CURLOPT_DISALLOW_USERNAME_IN_URL ) );

	// HTTP OPTIONS
	src->add_nativevar( "OPT_AUTOREFERER", make< var_int_t >( CURLOPT_AUTOREFERER ) );
	src->add_nativevar( "OPT_ACCEPT_ENCODING", make< var_int_t >( CURLOPT_ACCEPT_ENCODING ) );
	src->add_nativevar( "OPT_TRANSFER_ENCODING", make< var_int_t >( CURLOPT_TRANSFER_ENCODING ) );
	src->add_nativevar( "OPT_FOLLOWLOCATION", make< var_int_t >( CURLOPT_FOLLOWLOCATION ) );
	src->add_nativevar( "OPT_UNRESTRICTED_AUTH", make< var_int_t >( CURLOPT_UNRESTRICTED_AUTH ) );
	src->add_nativevar( "OPT_MAXREDIRS", make< var_int_t >( CURLOPT_MAXREDIRS ) );
	src->add_nativevar( "OPT_POSTREDIR", make< var_int_t >( CURLOPT_POSTREDIR ) );
	src->add_nativevar( "OPT_PUT", make< var_int_t >( CURLOPT_PUT ) );
	src->add_nativevar( "OPT_POST", make< var_int_t >( CURLOPT_POST ) );
	src->add_nativevar( "OPT_POSTFIELDS", make< var_int_t >( CURLOPT_POSTFIELDS ) );
	src->add_nativevar( "OPT_POSTFIELDSIZE", make< var_int_t >( CURLOPT_POSTFIELDSIZE ) );
	src->add_nativevar( "OPT_POSTFIELDSIZE_LARGE", make< var_int_t >( CURLOPT_POSTFIELDSIZE_LARGE ) );
	src->add_nativevar( "OPT_COPYPOSTFIELDS", make< var_int_t >( CURLOPT_COPYPOSTFIELDS ) );
	src->add_nativevar( "OPT_HTTPPOST", make< var_int_t >( CURLOPT_HTTPPOST ) );
	src->add_nativevar( "OPT_REFERER", make< var_int_t >( CURLOPT_REFERER ) );
	src->add_nativevar( "OPT_USERAGENT", make< var_int_t >( CURLOPT_USERAGENT ) );
	src->add_nativevar( "OPT_HTTPHEADER", make< var_int_t >( CURLOPT_HTTPHEADER ) );
	src->add_nativevar( "OPT_HEADEROPT", make< var_int_t >( CURLOPT_HEADEROPT ) );
	src->add_nativevar( "OPT_PROXYHEADER", make< var_int_t >( CURLOPT_PROXYHEADER ) );
	src->add_nativevar( "OPT_HTTP200ALIASES", make< var_int_t >( CURLOPT_HTTP200ALIASES ) );
	src->add_nativevar( "OPT_COOKIE", make< var_int_t >( CURLOPT_COOKIE ) );
	src->add_nativevar( "OPT_COOKIEFILE", make< var_int_t >( CURLOPT_COOKIEFILE ) );
	src->add_nativevar( "OPT_COOKIEJAR", make< var_int_t >( CURLOPT_COOKIEJAR ) );
	src->add_nativevar( "OPT_COOKIESESSION", make< var_int_t >( CURLOPT_COOKIESESSION ) );
	src->add_nativevar( "OPT_COOKIELIST", make< var_int_t >( CURLOPT_COOKIELIST ) );
	src->add_nativevar( "OPT_ALTSVC", make< var_int_t >( CURLOPT_ALTSVC ) );
	src->add_nativevar( "OPT_ALTSVC_CTRL", make< var_int_t >( CURLOPT_ALTSVC_CTRL ) );
	src->add_nativevar( "OPT_HTTPGET", make< var_int_t >( CURLOPT_HTTPGET ) );
	src->add_nativevar( "OPT_REQUEST_TARGET", make< var_int_t >( CURLOPT_REQUEST_TARGET ) );
	src->add_nativevar( "OPT_HTTP_VERSION", make< var_int_t >( CURLOPT_HTTP_VERSION ) );
	src->add_nativevar( "OPT_HTTP09_ALLOWED", make< var_int_t >( CURLOPT_HTTP09_ALLOWED ) );
	src->add_nativevar( "OPT_IGNORE_CONTENT_LENGTH", make< var_int_t >( CURLOPT_IGNORE_CONTENT_LENGTH ) );
	src->add_nativevar( "OPT_HTTP_CONTENT_DECODING", make< var_int_t >( CURLOPT_HTTP_CONTENT_DECODING ) );
	src->add_nativevar( "OPT_HTTP_TRANSFER_DECODING", make< var_int_t >( CURLOPT_HTTP_TRANSFER_DECODING ) );
	src->add_nativevar( "OPT_EXPECT_100_TIMEOUT_MS", make< var_int_t >( CURLOPT_EXPECT_100_TIMEOUT_MS ) );
	src->add_nativevar( "OPT_TRAILERFUNCTION", make< var_int_t >( CURLOPT_TRAILERFUNCTION ) );
	src->add_nativevar( "OPT_TRAILERDATA", make< var_int_t >( CURLOPT_TRAILERDATA ) );
	src->add_nativevar( "OPT_PIPEWAIT", make< var_int_t >( CURLOPT_PIPEWAIT ) );
	src->add_nativevar( "OPT_STREAM_DEPENDS", make< var_int_t >( CURLOPT_STREAM_DEPENDS ) );
	src->add_nativevar( "OPT_STREAM_DEPENDS_E", make< var_int_t >( CURLOPT_STREAM_DEPENDS_E ) );
	src->add_nativevar( "OPT_STREAM_WEIGHT", make< var_int_t >( CURLOPT_STREAM_WEIGHT ) );

	// SMTP OPTIONS
	src->add_nativevar( "OPT_MAIL_FROM", make< var_int_t >( CURLOPT_MAIL_FROM ) );
	src->add_nativevar( "OPT_MAIL_RCPT", make< var_int_t >( CURLOPT_MAIL_RCPT ) );
	src->add_nativevar( "OPT_MAIL_AUTH", make< var_int_t >( CURLOPT_MAIL_AUTH ) );
/* TODO: not available
	src->add_nativevar( "OPT_MAIL_RCPT_ALLLOWFAILS", make< var_int_t >( CURLOPT_MAIL_RCPT_ALLLOWFAILS ) );
*/

	// TFTP OPTIONS
	src->add_nativevar( "OPT_TFTP_BLKSIZE", make< var_int_t >( CURLOPT_TFTP_BLKSIZE ) );
	src->add_nativevar( "OPT_TFTP_NO_OPTIONS", make< var_int_t >( CURLOPT_TFTP_NO_OPTIONS ) );

	// FTP OPTIONS
	src->add_nativevar( "OPT_FTPPORT", make< var_int_t >( CURLOPT_FTPPORT ) );
	src->add_nativevar( "OPT_QUOTE", make< var_int_t >( CURLOPT_QUOTE ) );
	src->add_nativevar( "OPT_POSTQUOTE", make< var_int_t >( CURLOPT_POSTQUOTE ) );
	src->add_nativevar( "OPT_PREQUOTE", make< var_int_t >( CURLOPT_PREQUOTE ) );
	src->add_nativevar( "OPT_APPEND", make< var_int_t >( CURLOPT_APPEND ) );
	src->add_nativevar( "OPT_FTP_USE_EPRT", make< var_int_t >( CURLOPT_FTP_USE_EPRT ) );
	src->add_nativevar( "OPT_FTP_USE_EPSV", make< var_int_t >( CURLOPT_FTP_USE_EPSV ) );
	src->add_nativevar( "OPT_FTP_USE_PRET", make< var_int_t >( CURLOPT_FTP_USE_PRET ) );
	src->add_nativevar( "OPT_FTP_CREATE_MISSING_DIRS", make< var_int_t >( CURLOPT_FTP_CREATE_MISSING_DIRS ) );
	src->add_nativevar( "OPT_FTP_RESPONSE_TIMEOUT", make< var_int_t >( CURLOPT_FTP_RESPONSE_TIMEOUT ) );
	src->add_nativevar( "OPT_FTP_ALTERNATIVE_TO_USER", make< var_int_t >( CURLOPT_FTP_ALTERNATIVE_TO_USER ) );
	src->add_nativevar( "OPT_FTP_SKIP_PASV_IP", make< var_int_t >( CURLOPT_FTP_SKIP_PASV_IP ) );
	src->add_nativevar( "OPT_FTPSSLAUTH", make< var_int_t >( CURLOPT_FTPSSLAUTH ) );
	src->add_nativevar( "OPT_FTP_SSL_CCC", make< var_int_t >( CURLOPT_FTP_SSL_CCC ) );
	src->add_nativevar( "OPT_FTP_ACCOUNT", make< var_int_t >( CURLOPT_FTP_ACCOUNT ) );
	src->add_nativevar( "OPT_FTP_FILEMETHOD", make< var_int_t >( CURLOPT_FTP_FILEMETHOD ) );

	// RTSP OPTIONS
	src->add_nativevar( "OPT_RTSP_REQUEST", make< var_int_t >( CURLOPT_RTSP_REQUEST ) );
	src->add_nativevar( "OPT_RTSP_SESSION_ID", make< var_int_t >( CURLOPT_RTSP_SESSION_ID ) );
	src->add_nativevar( "OPT_RTSP_STREAM_URI", make< var_int_t >( CURLOPT_RTSP_STREAM_URI ) );
	src->add_nativevar( "OPT_RTSP_TRANSPORT", make< var_int_t >( CURLOPT_RTSP_TRANSPORT ) );
	src->add_nativevar( "OPT_RTSP_CLIENT_CSEQ", make< var_int_t >( CURLOPT_RTSP_CLIENT_CSEQ ) );
	src->add_nativevar( "OPT_RTSP_SERVER_CSEQ", make< var_int_t >( CURLOPT_RTSP_SERVER_CSEQ ) );

	// PROTOCOL OPTIONS
	src->add_nativevar( "OPT_TRANSFERTEXT", make< var_int_t >( CURLOPT_TRANSFERTEXT ) );
	src->add_nativevar( "OPT_PROXY_TRANSFER_MODE", make< var_int_t >( CURLOPT_PROXY_TRANSFER_MODE ) );
	src->add_nativevar( "OPT_CRLF", make< var_int_t >( CURLOPT_CRLF ) );
	src->add_nativevar( "OPT_RANGE", make< var_int_t >( CURLOPT_RANGE ) );
	src->add_nativevar( "OPT_RESUME_FROM", make< var_int_t >( CURLOPT_RESUME_FROM ) );
	src->add_nativevar( "OPT_RESUME_FROM_LARGE", make< var_int_t >( CURLOPT_RESUME_FROM_LARGE ) );
	src->add_nativevar( "OPT_CURLU", make< var_int_t >( CURLOPT_CURLU ) );
	src->add_nativevar( "OPT_CUSTOMREQUEST", make< var_int_t >( CURLOPT_CUSTOMREQUEST ) );
	src->add_nativevar( "OPT_FILETIME", make< var_int_t >( CURLOPT_FILETIME ) );
	src->add_nativevar( "OPT_DIRLISTONLY", make< var_int_t >( CURLOPT_DIRLISTONLY ) );
	src->add_nativevar( "OPT_NOBODY", make< var_int_t >( CURLOPT_NOBODY ) );
	src->add_nativevar( "OPT_INFILESIZE", make< var_int_t >( CURLOPT_INFILESIZE ) );
	src->add_nativevar( "OPT_INFILESIZE_LARGE", make< var_int_t >( CURLOPT_INFILESIZE_LARGE ) );
	src->add_nativevar( "OPT_UPLOAD", make< var_int_t >( CURLOPT_UPLOAD ) );
	src->add_nativevar( "OPT_UPLOAD_BUFFERSIZE", make< var_int_t >( CURLOPT_UPLOAD_BUFFERSIZE ) );
	src->add_nativevar( "OPT_MIMEPOST", make< var_int_t >( CURLOPT_MIMEPOST ) );
	src->add_nativevar( "OPT_MAXFILESIZE", make< var_int_t >( CURLOPT_MAXFILESIZE ) );
	src->add_nativevar( "OPT_MAXFILESIZE_LARGE", make< var_int_t >( CURLOPT_MAXFILESIZE_LARGE ) );
	src->add_nativevar( "OPT_TIMECONDITION", make< var_int_t >( CURLOPT_TIMECONDITION ) );
	src->add_nativevar( "OPT_TIMEVALUE", make< var_int_t >( CURLOPT_TIMEVALUE ) );
	src->add_nativevar( "OPT_TIMEVALUE_LARGE", make< var_int_t >( CURLOPT_TIMEVALUE_LARGE ) );

	// CONNECTION OPTIONS
	src->add_nativevar( "OPT_TIMEOUT", make< var_int_t >( CURLOPT_TIMEOUT ) );
	src->add_nativevar( "OPT_TIMEOUT_MS", make< var_int_t >( CURLOPT_TIMEOUT_MS ) );
	src->add_nativevar( "OPT_LOW_SPEED_LIMIT", make< var_int_t >( CURLOPT_LOW_SPEED_LIMIT ) );
	src->add_nativevar( "OPT_LOW_SPEED_TIME", make< var_int_t >( CURLOPT_LOW_SPEED_TIME ) );
	src->add_nativevar( "OPT_MAX_SEND_SPEED_LARGE", make< var_int_t >( CURLOPT_MAX_SEND_SPEED_LARGE ) );
	src->add_nativevar( "OPT_MAX_RECV_SPEED_LARGE", make< var_int_t >( CURLOPT_MAX_RECV_SPEED_LARGE ) );
	src->add_nativevar( "OPT_MAXCONNECTS", make< var_int_t >( CURLOPT_MAXCONNECTS ) );
	src->add_nativevar( "OPT_FRESH_CONNECT", make< var_int_t >( CURLOPT_FRESH_CONNECT ) );
	src->add_nativevar( "OPT_FORBID_REUSE", make< var_int_t >( CURLOPT_FORBID_REUSE ) );
/* TODO: not available
	src->add_nativevar( "OPT_MAXAGE_CONN", make< var_int_t >( CURLOPT_MAXAGE_CONN ) );
*/
	src->add_nativevar( "OPT_CONNECTTIMEOUT", make< var_int_t >( CURLOPT_CONNECTTIMEOUT ) );
	src->add_nativevar( "OPT_CONNECTTIMEOUT_MS", make< var_int_t >( CURLOPT_CONNECTTIMEOUT_MS ) );
	src->add_nativevar( "OPT_IPRESOLVE", make< var_int_t >( CURLOPT_IPRESOLVE ) );
	src->add_nativevar( "OPT_CONNECT_ONLY", make< var_int_t >( CURLOPT_CONNECT_ONLY ) );
	src->add_nativevar( "OPT_USE_SSL", make< var_int_t >( CURLOPT_USE_SSL ) );
	src->add_nativevar( "OPT_RESOLVE", make< var_int_t >( CURLOPT_RESOLVE ) );
	src->add_nativevar( "OPT_DNS_INTERFACE", make< var_int_t >( CURLOPT_DNS_INTERFACE ) );
	src->add_nativevar( "OPT_DNS_LOCAL_IP4", make< var_int_t >( CURLOPT_DNS_LOCAL_IP4 ) );
	src->add_nativevar( "OPT_DNS_LOCAL_IP6", make< var_int_t >( CURLOPT_DNS_LOCAL_IP6 ) );
	src->add_nativevar( "OPT_DNS_SERVERS", make< var_int_t >( CURLOPT_DNS_SERVERS ) );
	src->add_nativevar( "OPT_DNS_SHUFFLE_ADDRESSES", make< var_int_t >( CURLOPT_DNS_SHUFFLE_ADDRESSES ) );
	src->add_nativevar( "OPT_ACCEPTTIMEOUT_MS", make< var_int_t >( CURLOPT_ACCEPTTIMEOUT_MS ) );
	src->add_nativevar( "OPT_HAPPY_EYEBALLS_TIMEOUT_MS", make< var_int_t >( CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS ) );
	src->add_nativevar( "OPT_UPKEEP_INTERVAL_MS", make< var_int_t >( CURLOPT_UPKEEP_INTERVAL_MS ) );

	// SSL and SECURITY OPTIONS
	src->add_nativevar( "OPT_SSLCERT", make< var_int_t >( CURLOPT_SSLCERT ) );
	src->add_nativevar( "OPT_PROXY_SSLCERT", make< var_int_t >( CURLOPT_PROXY_SSLCERT ) );
	src->add_nativevar( "OPT_SSLCERTTYPE", make< var_int_t >( CURLOPT_SSLCERTTYPE ) );
	src->add_nativevar( "OPT_PROXY_SSLCERTTYPE", make< var_int_t >( CURLOPT_PROXY_SSLCERTTYPE ) );
	src->add_nativevar( "OPT_SSLKEY", make< var_int_t >( CURLOPT_SSLKEY ) );
	src->add_nativevar( "OPT_PROXY_SSLKEY", make< var_int_t >( CURLOPT_PROXY_SSLKEY ) );
	src->add_nativevar( "OPT_SSLKEYTYPE", make< var_int_t >( CURLOPT_SSLKEYTYPE ) );
	src->add_nativevar( "OPT_PROXY_SSLKEYTYPE", make< var_int_t >( CURLOPT_PROXY_SSLKEYTYPE ) );
	src->add_nativevar( "OPT_KEYPASSWD", make< var_int_t >( CURLOPT_KEYPASSWD ) );
	src->add_nativevar( "OPT_PROXY_KEYPASSWD", make< var_int_t >( CURLOPT_PROXY_KEYPASSWD ) );
	src->add_nativevar( "OPT_SSL_ENABLE_ALPN", make< var_int_t >( CURLOPT_SSL_ENABLE_ALPN ) );
	src->add_nativevar( "OPT_SSL_ENABLE_NPN", make< var_int_t >( CURLOPT_SSL_ENABLE_NPN ) );
	src->add_nativevar( "OPT_SSLENGINE", make< var_int_t >( CURLOPT_SSLENGINE ) );
	src->add_nativevar( "OPT_SSLENGINE_DEFAULT", make< var_int_t >( CURLOPT_SSLENGINE_DEFAULT ) );
	src->add_nativevar( "OPT_SSL_FALSESTART", make< var_int_t >( CURLOPT_SSL_FALSESTART ) );
	src->add_nativevar( "OPT_SSLVERSION", make< var_int_t >( CURLOPT_SSLVERSION ) );
	src->add_nativevar( "OPT_PROXY_SSLVERSION", make< var_int_t >( CURLOPT_PROXY_SSLVERSION ) );
	src->add_nativevar( "OPT_SSL_VERIFYHOST", make< var_int_t >( CURLOPT_SSL_VERIFYHOST ) );
	src->add_nativevar( "OPT_PROXY_SSL_VERIFYHOST", make< var_int_t >( CURLOPT_PROXY_SSL_VERIFYHOST ) );
	src->add_nativevar( "OPT_SSL_VERIFYPEER", make< var_int_t >( CURLOPT_SSL_VERIFYPEER ) );
	src->add_nativevar( "OPT_PROXY_SSL_VERIFYPEER", make< var_int_t >( CURLOPT_PROXY_SSL_VERIFYPEER ) );
	src->add_nativevar( "OPT_SSL_VERIFYSTATUS", make< var_int_t >( CURLOPT_SSL_VERIFYSTATUS ) );
	src->add_nativevar( "OPT_CAINFO", make< var_int_t >( CURLOPT_CAINFO ) );
	src->add_nativevar( "OPT_PROXY_CAINFO", make< var_int_t >( CURLOPT_PROXY_CAINFO ) );
	src->add_nativevar( "OPT_ISSUERCERT", make< var_int_t >( CURLOPT_ISSUERCERT ) );
	src->add_nativevar( "OPT_CAPATH", make< var_int_t >( CURLOPT_CAPATH ) );
	src->add_nativevar( "OPT_PROXY_CAPATH", make< var_int_t >( CURLOPT_PROXY_CAPATH ) );
	src->add_nativevar( "OPT_CRLFILE", make< var_int_t >( CURLOPT_CRLFILE ) );
	src->add_nativevar( "OPT_PROXY_CRLFILE", make< var_int_t >( CURLOPT_PROXY_CRLFILE ) );
	src->add_nativevar( "OPT_CERTINFO", make< var_int_t >( CURLOPT_CERTINFO ) );
	src->add_nativevar( "OPT_PINNEDPUBLICKEY", make< var_int_t >( CURLOPT_PINNEDPUBLICKEY ) );
	src->add_nativevar( "OPT_PROXY_PINNEDPUBLICKEY", make< var_int_t >( CURLOPT_PROXY_PINNEDPUBLICKEY ) );
	src->add_nativevar( "OPT_RANDOM_FILE", make< var_int_t >( CURLOPT_RANDOM_FILE ) );
	src->add_nativevar( "OPT_EGDSOCKET", make< var_int_t >( CURLOPT_EGDSOCKET ) );
	src->add_nativevar( "OPT_SSL_CIPHER_LIST", make< var_int_t >( CURLOPT_SSL_CIPHER_LIST ) );
	src->add_nativevar( "OPT_PROXY_SSL_CIPHER_LIST", make< var_int_t >( CURLOPT_PROXY_SSL_CIPHER_LIST ) );
	src->add_nativevar( "OPT_TLS13_CIPHERS", make< var_int_t >( CURLOPT_TLS13_CIPHERS ) );
	src->add_nativevar( "OPT_PROXY_TLS13_CIPHERS", make< var_int_t >( CURLOPT_PROXY_TLS13_CIPHERS ) );
	src->add_nativevar( "OPT_SSL_SESSIONID_CACHE", make< var_int_t >( CURLOPT_SSL_SESSIONID_CACHE ) );
	src->add_nativevar( "OPT_SSL_OPTIONS", make< var_int_t >( CURLOPT_SSL_OPTIONS ) );
	src->add_nativevar( "OPT_PROXY_SSL_OPTIONS", make< var_int_t >( CURLOPT_PROXY_SSL_OPTIONS ) );
	src->add_nativevar( "OPT_KRBLEVEL", make< var_int_t >( CURLOPT_KRBLEVEL ) );
	src->add_nativevar( "OPT_GSSAPI_DELEGATION", make< var_int_t >( CURLOPT_GSSAPI_DELEGATION ) );

	// SSH OPTIONS
	src->add_nativevar( "OPT_SSH_AUTH_TYPES", make< var_int_t >( CURLOPT_SSH_AUTH_TYPES ) );
	src->add_nativevar( "OPT_SSH_COMPRESSION", make< var_int_t >( CURLOPT_SSH_COMPRESSION ) );
	src->add_nativevar( "OPT_SSH_HOST_PUBLIC_KEY_MD5", make< var_int_t >( CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 ) );
	src->add_nativevar( "OPT_SSH_PUBLIC_KEYFILE", make< var_int_t >( CURLOPT_SSH_PUBLIC_KEYFILE ) );
	src->add_nativevar( "OPT_SSH_PRIVATE_KEYFILE", make< var_int_t >( CURLOPT_SSH_PRIVATE_KEYFILE ) );
	src->add_nativevar( "OPT_SSH_KNOWNHOSTS", make< var_int_t >( CURLOPT_SSH_KNOWNHOSTS ) );
	src->add_nativevar( "OPT_SSH_KEYFUNCTION", make< var_int_t >( CURLOPT_SSH_KEYFUNCTION ) );
	src->add_nativevar( "OPT_SSH_KEYDATA", make< var_int_t >( CURLOPT_SSH_KEYDATA ) );

	// OTHER OPTIONS
	src->add_nativevar( "OPT_PRIVATE", make< var_int_t >( CURLOPT_PRIVATE ) );
	src->add_nativevar( "OPT_SHARE", make< var_int_t >( CURLOPT_SHARE ) );
	src->add_nativevar( "OPT_NEW_FILE_PERMS", make< var_int_t >( CURLOPT_NEW_FILE_PERMS ) );
	src->add_nativevar( "OPT_NEW_DIRECTORY_PERMS", make< var_int_t >( CURLOPT_NEW_DIRECTORY_PERMS ) );

	// TELNET OPTIONS
	src->add_nativevar( "OPT_TELNETOPTIONS", make< var_int_t >( CURLOPT_TELNETOPTIONS ) );

	return true;
}

DEINIT_MODULE( curl )
{
	var_dref( progress_callback );
}
#include <curl/curl.h>

#include "CurlBase.hpp"
#include "CurlType.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

Var *feralCurlEasyInit(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
		       const Map<String, AssnArgData> &assn_args)
{
	CURL *curl = curl_easy_init();
	if(!curl) {
		vm.fail(loc, "failed to run curl_easy_init()");
		return nullptr;
	}

	return vm.makeVar<VarCurl>(loc, curl);
}

Var *feralCurlEasyPerform(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			  const Map<String, AssnArgData> &assn_args)
{
	CURL *curl = as<VarCurl>(args[0])->get();
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, loc);
	return vm.makeVar<VarInt>(loc, curl_easy_perform(curl));
}

Var *feralCurlEasyStrErrFromInt(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
				const Map<String, AssnArgData> &assn_args)
{
	if(!args[1]->is<VarInt>()) {
		vm.fail(args[1]->getLoc(), "expected error code to be of type 'int', found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	CURLcode code = (CURLcode)as<VarInt>(args[1])->get();
	return vm.makeVar<VarStr>(loc, curl_easy_strerror(code));
}

INIT_MODULE(Curl)
{
	cbVM = &vm;

	VarModule *mod = vm.getCurrModule();

	mod->addNativeFn("newEasy", feralCurlEasyInit);
	mod->addNativeFn("strerr", feralCurlEasyStrErrFromInt, 1);
	mod->addNativeFn("setWriteCallback", feralCurlSetWriteCallback, 1);
	mod->addNativeFn("setProgressCallback", feralCurlSetProgressCallback, 1);
	mod->addNativeFn("setProgressCallbackTick", feralCurlSetProgressCallbackTick, 1);

	// register the curl type (register_type)
	vm.registerType<VarCurl>(loc, "Curl");

	vm.addNativeTypeFn<VarCurl>(loc, "newMime", feralCurlMimeNew, 0);
	vm.addNativeTypeFn<VarCurl>(loc, "setOptNative", feralCurlEasySetOptNative, 2);
	vm.addNativeTypeFn<VarCurl>(loc, "perform", feralCurlEasyPerform, 0);

	vm.addNativeTypeFn<VarCurlMime>(loc, "addPartData", feralCurlMimePartAddData, 2);
	vm.addNativeTypeFn<VarCurlMime>(loc, "addPartFile", feralCurlMimePartAddFile, 2);

	// all the enum values

	// CURLcode
	mod->addNativeVar("E_OK", vm.makeVar<VarInt>(loc, CURLE_OK));
	mod->addNativeVar("E_UNSUPPORTED_PROTOCOL",
			  vm.makeVar<VarInt>(loc, CURLE_UNSUPPORTED_PROTOCOL));
	mod->addNativeVar("E_FAILED_INIT", vm.makeVar<VarInt>(loc, CURLE_FAILED_INIT));
	mod->addNativeVar("E_URL_MALFORMAT", vm.makeVar<VarInt>(loc, CURLE_URL_MALFORMAT));
	mod->addNativeVar("E_NOT_BUILT_IN", vm.makeVar<VarInt>(loc, CURLE_NOT_BUILT_IN));
	mod->addNativeVar("E_COULDNT_RESOLVE_PROXY",
			  vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_PROXY));
	mod->addNativeVar("E_COULDNT_RESOLVE_HOST",
			  vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_HOST));
	mod->addNativeVar("E_COULDNT_CONNECT", vm.makeVar<VarInt>(loc, CURLE_COULDNT_CONNECT));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
	mod->addNativeVar("E_WEIRD_SERVER_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_WEIRD_SERVER_REPLY));
#else
	mod->addNativeVar("E_FTP_WEIRD_SERVER_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_SERVER_REPLY));
#endif
	mod->addNativeVar("E_REMOTE_ACCESS_DENIED",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_ACCESS_DENIED));
	mod->addNativeVar("E_FTP_ACCEPT_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_FAILED));
	mod->addNativeVar("E_FTP_WEIRD_PASS_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASS_REPLY));
	mod->addNativeVar("E_FTP_ACCEPT_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_TIMEOUT));
	mod->addNativeVar("E_FTP_WEIRD_PASV_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASV_REPLY));
	mod->addNativeVar("E_FTP_WEIRD_227_FORMAT",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_227_FORMAT));
	mod->addNativeVar("E_FTP_CANT_GET_HOST", vm.makeVar<VarInt>(loc, CURLE_FTP_CANT_GET_HOST));
	mod->addNativeVar("E_HTTP2", vm.makeVar<VarInt>(loc, CURLE_HTTP2));
	mod->addNativeVar("E_FTP_COULDNT_SET_TYPE",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_SET_TYPE));
	mod->addNativeVar("E_PARTIAL_FILE", vm.makeVar<VarInt>(loc, CURLE_PARTIAL_FILE));
	mod->addNativeVar("E_FTP_COULDNT_RETR_FILE",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_RETR_FILE));
	mod->addNativeVar("E_OBSOLETE20", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE20));
	mod->addNativeVar("E_QUOTE_ERROR", vm.makeVar<VarInt>(loc, CURLE_QUOTE_ERROR));
	mod->addNativeVar("E_HTTP_RETURNED_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_HTTP_RETURNED_ERROR));
	mod->addNativeVar("E_WRITE_ERROR", vm.makeVar<VarInt>(loc, CURLE_WRITE_ERROR));
	mod->addNativeVar("E_OBSOLETE24", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE24));
	mod->addNativeVar("E_UPLOAD_FAILED", vm.makeVar<VarInt>(loc, CURLE_UPLOAD_FAILED));
	mod->addNativeVar("E_READ_ERROR", vm.makeVar<VarInt>(loc, CURLE_READ_ERROR));
	mod->addNativeVar("E_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLE_OUT_OF_MEMORY));
	mod->addNativeVar("E_OPERATION_TIMEDOUT",
			  vm.makeVar<VarInt>(loc, CURLE_OPERATION_TIMEDOUT));
	mod->addNativeVar("E_OBSOLETE29", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE29));
	mod->addNativeVar("E_FTP_PORT_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_PORT_FAILED));
	mod->addNativeVar("E_FTP_COULDNT_USE_REST",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_USE_REST));
	mod->addNativeVar("E_OBSOLETE32", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE32));
	mod->addNativeVar("E_RANGE_ERROR", vm.makeVar<VarInt>(loc, CURLE_RANGE_ERROR));
	mod->addNativeVar("E_HTTP_POST_ERROR", vm.makeVar<VarInt>(loc, CURLE_HTTP_POST_ERROR));
	mod->addNativeVar("E_SSL_CONNECT_ERROR", vm.makeVar<VarInt>(loc, CURLE_SSL_CONNECT_ERROR));
	mod->addNativeVar("E_BAD_DOWNLOAD_RESUME",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_DOWNLOAD_RESUME));
	mod->addNativeVar("E_FILE_COULDNT_READ_FILE",
			  vm.makeVar<VarInt>(loc, CURLE_FILE_COULDNT_READ_FILE));
	mod->addNativeVar("E_LDAP_CANNOT_BIND", vm.makeVar<VarInt>(loc, CURLE_LDAP_CANNOT_BIND));
	mod->addNativeVar("E_LDAP_SEARCH_FAILED",
			  vm.makeVar<VarInt>(loc, CURLE_LDAP_SEARCH_FAILED));
	mod->addNativeVar("E_OBSOLETE40", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE40));
	mod->addNativeVar("E_FUNCTION_NOT_FOUND",
			  vm.makeVar<VarInt>(loc, CURLE_FUNCTION_NOT_FOUND));
	mod->addNativeVar("E_ABORTED_BY_CALLBACK",
			  vm.makeVar<VarInt>(loc, CURLE_ABORTED_BY_CALLBACK));
	mod->addNativeVar("E_BAD_FUNCTION_ARGUMENT",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_FUNCTION_ARGUMENT));
	mod->addNativeVar("E_OBSOLETE44", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE44));
	mod->addNativeVar("E_INTERFACE_FAILED", vm.makeVar<VarInt>(loc, CURLE_INTERFACE_FAILED));
	mod->addNativeVar("E_OBSOLETE46", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE46));
	mod->addNativeVar("E_TOO_MANY_REDIRECTS",
			  vm.makeVar<VarInt>(loc, CURLE_TOO_MANY_REDIRECTS));
	mod->addNativeVar("E_UNKNOWN_OPTION", vm.makeVar<VarInt>(loc, CURLE_UNKNOWN_OPTION));
	mod->addNativeVar("E_TELNET_OPTION_SYNTAX",
			  vm.makeVar<VarInt>(loc, CURLE_TELNET_OPTION_SYNTAX));
	mod->addNativeVar("E_OBSOLETE50", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE50));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("E_OBSOLETE51", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE51));
#endif
	mod->addNativeVar("E_GOT_NOTHING", vm.makeVar<VarInt>(loc, CURLE_GOT_NOTHING));
	mod->addNativeVar("E_SSL_ENGINE_NOTFOUND",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_NOTFOUND));
	mod->addNativeVar("E_SSL_ENGINE_SETFAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_SETFAILED));
	mod->addNativeVar("E_SEND_ERROR", vm.makeVar<VarInt>(loc, CURLE_SEND_ERROR));
	mod->addNativeVar("E_RECV_ERROR", vm.makeVar<VarInt>(loc, CURLE_RECV_ERROR));
	mod->addNativeVar("E_OBSOLETE57", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE57));
	mod->addNativeVar("E_SSL_CERTPROBLEM", vm.makeVar<VarInt>(loc, CURLE_SSL_CERTPROBLEM));
	mod->addNativeVar("E_SSL_CIPHER", vm.makeVar<VarInt>(loc, CURLE_SSL_CIPHER));
	mod->addNativeVar("E_PEER_FAILED_VERIFICATION",
			  vm.makeVar<VarInt>(loc, CURLE_PEER_FAILED_VERIFICATION));
	mod->addNativeVar("E_BAD_CONTENT_ENCODING",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_CONTENT_ENCODING));
	mod->addNativeVar("E_LDAP_INVALID_URL", vm.makeVar<VarInt>(loc, CURLE_LDAP_INVALID_URL));
	mod->addNativeVar("E_FILESIZE_EXCEEDED", vm.makeVar<VarInt>(loc, CURLE_FILESIZE_EXCEEDED));
	mod->addNativeVar("E_USE_SSL_FAILED", vm.makeVar<VarInt>(loc, CURLE_USE_SSL_FAILED));
	mod->addNativeVar("E_SEND_FAIL_REWIND", vm.makeVar<VarInt>(loc, CURLE_SEND_FAIL_REWIND));
	mod->addNativeVar("E_SSL_ENGINE_INITFAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_INITFAILED));
	mod->addNativeVar("E_LOGIN_DENIED", vm.makeVar<VarInt>(loc, CURLE_LOGIN_DENIED));
	mod->addNativeVar("E_TFTP_NOTFOUND", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOTFOUND));
	mod->addNativeVar("E_TFTP_PERM", vm.makeVar<VarInt>(loc, CURLE_TFTP_PERM));
	mod->addNativeVar("E_REMOTE_DISK_FULL", vm.makeVar<VarInt>(loc, CURLE_REMOTE_DISK_FULL));
	mod->addNativeVar("E_TFTP_ILLEGAL", vm.makeVar<VarInt>(loc, CURLE_TFTP_ILLEGAL));
	mod->addNativeVar("E_TFTP_UNKNOWNID", vm.makeVar<VarInt>(loc, CURLE_TFTP_UNKNOWNID));
	mod->addNativeVar("E_REMOTE_FILE_EXISTS",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_EXISTS));
	mod->addNativeVar("E_TFTP_NOSUCHUSER", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOSUCHUSER));
	mod->addNativeVar("E_CONV_FAILED", vm.makeVar<VarInt>(loc, CURLE_CONV_FAILED));
	mod->addNativeVar("E_CONV_REQD", vm.makeVar<VarInt>(loc, CURLE_CONV_REQD));
	mod->addNativeVar("E_SSL_CACERT_BADFILE",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_CACERT_BADFILE));
	mod->addNativeVar("E_REMOTE_FILE_NOT_FOUND",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_NOT_FOUND));
	mod->addNativeVar("E_SSH", vm.makeVar<VarInt>(loc, CURLE_SSH));
	mod->addNativeVar("E_SSL_SHUTDOWN_FAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_SHUTDOWN_FAILED));
	mod->addNativeVar("E_AGAIN", vm.makeVar<VarInt>(loc, CURLE_AGAIN));
	mod->addNativeVar("E_SSL_CRL_BADFILE", vm.makeVar<VarInt>(loc, CURLE_SSL_CRL_BADFILE));
	mod->addNativeVar("E_SSL_ISSUER_ERROR", vm.makeVar<VarInt>(loc, CURLE_SSL_ISSUER_ERROR));
	mod->addNativeVar("E_FTP_PRET_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_PRET_FAILED));
	mod->addNativeVar("E_RTSP_CSEQ_ERROR", vm.makeVar<VarInt>(loc, CURLE_RTSP_CSEQ_ERROR));
	mod->addNativeVar("E_RTSP_SESSION_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_RTSP_SESSION_ERROR));
	mod->addNativeVar("E_FTP_BAD_FILE_LIST", vm.makeVar<VarInt>(loc, CURLE_FTP_BAD_FILE_LIST));
	mod->addNativeVar("E_CHUNK_FAILED", vm.makeVar<VarInt>(loc, CURLE_CHUNK_FAILED));
	mod->addNativeVar("E_NO_CONNECTION_AVAILABLE",
			  vm.makeVar<VarInt>(loc, CURLE_NO_CONNECTION_AVAILABLE));
	mod->addNativeVar("E_SSL_PINNEDPUBKEYNOTMATCH",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_PINNEDPUBKEYNOTMATCH));
	mod->addNativeVar("E_SSL_INVALIDCERTSTATUS",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_INVALIDCERTSTATUS));
#if CURL_AT_LEAST_VERSION(7, 50, 2)
	mod->addNativeVar("E_HTTP2_STREAM", vm.makeVar<VarInt>(loc, CURLE_HTTP2_STREAM));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("E_RECURSIVE_API_CALL",
			  vm.makeVar<VarInt>(loc, CURLE_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 66, 0)
	mod->addNativeVar("E_AUTH_ERROR", vm.makeVar<VarInt>(loc, CURLE_AUTH_ERROR));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
	mod->addNativeVar("E_HTTP3", vm.makeVar<VarInt>(loc, CURLE_HTTP3));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("E_QUIC_CONNECT_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_QUIC_CONNECT_ERROR));
#endif

	// CURLMcode
	mod->addNativeVar("M_CALL_MULTI_PERFORM",
			  vm.makeVar<VarInt>(loc, CURLM_CALL_MULTI_PERFORM));
	mod->addNativeVar("M_OK", vm.makeVar<VarInt>(loc, CURLM_OK));
	mod->addNativeVar("M_BAD_HANDLE", vm.makeVar<VarInt>(loc, CURLM_BAD_HANDLE));
	mod->addNativeVar("M_BAD_EASY_HANDLE", vm.makeVar<VarInt>(loc, CURLM_BAD_EASY_HANDLE));
	mod->addNativeVar("M_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLM_OUT_OF_MEMORY));
	mod->addNativeVar("M_INTERNAL_ERROR", vm.makeVar<VarInt>(loc, CURLM_INTERNAL_ERROR));
	mod->addNativeVar("M_BAD_SOCKET", vm.makeVar<VarInt>(loc, CURLM_BAD_SOCKET));
	mod->addNativeVar("M_UNKNOWN_OPTION", vm.makeVar<VarInt>(loc, CURLM_UNKNOWN_OPTION));
	mod->addNativeVar("M_ADDED_ALREADY", vm.makeVar<VarInt>(loc, CURLM_ADDED_ALREADY));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("M_RECURSIVE_API_CALL",
			  vm.makeVar<VarInt>(loc, CURLM_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
	mod->addNativeVar("WAKEUP_FAILURE", vm.makeVar<VarInt>(loc, CURLM_WAKEUP_FAILURE));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("BAD_FUNCTION_ARGUMENT",
			  vm.makeVar<VarInt>(loc, CURLM_BAD_FUNCTION_ARGUMENT));
#endif

	// CURLSHcode
	mod->addNativeVar("SHE_OK", vm.makeVar<VarInt>(loc, CURLSHE_OK));
	mod->addNativeVar("SHE_BAD_OPTION", vm.makeVar<VarInt>(loc, CURLSHE_BAD_OPTION));
	mod->addNativeVar("SHE_IN_USE", vm.makeVar<VarInt>(loc, CURLSHE_IN_USE));
	mod->addNativeVar("SHE_INVALID", vm.makeVar<VarInt>(loc, CURLSHE_INVALID));
	mod->addNativeVar("SHE_NOMEM", vm.makeVar<VarInt>(loc, CURLSHE_NOMEM));
	mod->addNativeVar("SHE_NOT_BUILT_IN", vm.makeVar<VarInt>(loc, CURLSHE_NOT_BUILT_IN));

#if CURL_AT_LEAST_VERSION(7, 62, 0)
	// CURLUcode
	mod->addNativeVar("UE_OK", vm.makeVar<VarInt>(loc, CURLUE_OK));
	mod->addNativeVar("UE_BAD_HANDLE", vm.makeVar<VarInt>(loc, CURLUE_BAD_HANDLE));
	mod->addNativeVar("UE_BAD_PARTPOINTER", vm.makeVar<VarInt>(loc, CURLUE_BAD_PARTPOINTER));
	mod->addNativeVar("UE_MALFORMED_INPUT", vm.makeVar<VarInt>(loc, CURLUE_MALFORMED_INPUT));
	mod->addNativeVar("UE_BAD_PORT_NUMBER", vm.makeVar<VarInt>(loc, CURLUE_BAD_PORT_NUMBER));
	mod->addNativeVar("UE_UNSUPPORTED_SCHEME",
			  vm.makeVar<VarInt>(loc, CURLUE_UNSUPPORTED_SCHEME));
	mod->addNativeVar("UE_URLDECODE", vm.makeVar<VarInt>(loc, CURLUE_URLDECODE));
	mod->addNativeVar("UE_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLUE_OUT_OF_MEMORY));
	mod->addNativeVar("UE_USER_NOT_ALLOWED", vm.makeVar<VarInt>(loc, CURLUE_USER_NOT_ALLOWED));
	mod->addNativeVar("UE_UNKNOWN_PART", vm.makeVar<VarInt>(loc, CURLUE_UNKNOWN_PART));
	mod->addNativeVar("UE_NO_SCHEME", vm.makeVar<VarInt>(loc, CURLUE_NO_SCHEME));
	mod->addNativeVar("UE_NO_USER", vm.makeVar<VarInt>(loc, CURLUE_NO_USER));
	mod->addNativeVar("UE_NO_PASSWORD", vm.makeVar<VarInt>(loc, CURLUE_NO_PASSWORD));
	mod->addNativeVar("UE_NO_OPTIONS", vm.makeVar<VarInt>(loc, CURLUE_NO_OPTIONS));
	mod->addNativeVar("UE_NO_HOST", vm.makeVar<VarInt>(loc, CURLUE_NO_HOST));
	mod->addNativeVar("UE_NO_PORT", vm.makeVar<VarInt>(loc, CURLUE_NO_PORT));
	mod->addNativeVar("UE_NO_QUERY", vm.makeVar<VarInt>(loc, CURLUE_NO_QUERY));
	mod->addNativeVar("UE_NO_FRAGMENT", vm.makeVar<VarInt>(loc, CURLUE_NO_FRAGMENT));
#endif

	// EASY_OPTS

	// BEHAVIOR OPTIONS
	mod->addNativeVar("OPT_VERBOSE", vm.makeVar<VarInt>(loc, CURLOPT_VERBOSE));
	mod->addNativeVar("OPT_HEADER", vm.makeVar<VarInt>(loc, CURLOPT_HEADER));
	mod->addNativeVar("OPT_NOPROGRESS", vm.makeVar<VarInt>(loc, CURLOPT_NOPROGRESS));
	mod->addNativeVar("OPT_NOSIGNAL", vm.makeVar<VarInt>(loc, CURLOPT_NOSIGNAL));
	mod->addNativeVar("OPT_WILDCARDMATCH", vm.makeVar<VarInt>(loc, CURLOPT_WILDCARDMATCH));

	// CALLBACK OPTIONS
	mod->addNativeVar("OPT_WRITEFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_WRITEFUNCTION));
	mod->addNativeVar("OPT_WRITEDATA", vm.makeVar<VarInt>(loc, CURLOPT_WRITEDATA));
	mod->addNativeVar("OPT_READFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_READFUNCTION));
	mod->addNativeVar("OPT_READDATA", vm.makeVar<VarInt>(loc, CURLOPT_READDATA));
	mod->addNativeVar("OPT_SEEKFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SEEKFUNCTION));
	mod->addNativeVar("OPT_SEEKDATA", vm.makeVar<VarInt>(loc, CURLOPT_SEEKDATA));
	mod->addNativeVar("OPT_SOCKOPTFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTFUNCTION));
	mod->addNativeVar("OPT_SOCKOPTDATA", vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTDATA));
	mod->addNativeVar("OPT_OPENSOCKETFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETFUNCTION));
	mod->addNativeVar("OPT_OPENSOCKETDATA", vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETDATA));
	mod->addNativeVar("OPT_CLOSESOCKETFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETFUNCTION));
	mod->addNativeVar("OPT_CLOSESOCKETDATA", vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETDATA));
	mod->addNativeVar("OPT_PROGRESSDATA", vm.makeVar<VarInt>(loc, CURLOPT_PROGRESSDATA));
	mod->addNativeVar("OPT_XFERINFOFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_XFERINFOFUNCTION));
	mod->addNativeVar("OPT_XFERINFODATA", vm.makeVar<VarInt>(loc, CURLOPT_XFERINFODATA));
	mod->addNativeVar("OPT_HEADERFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_HEADERFUNCTION));
	mod->addNativeVar("OPT_HEADERDATA", vm.makeVar<VarInt>(loc, CURLOPT_HEADERDATA));
	mod->addNativeVar("OPT_DEBUGFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGFUNCTION));
	mod->addNativeVar("OPT_DEBUGDATA", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGDATA));
	mod->addNativeVar("OPT_SSL_CTX_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_FUNCTION));
	mod->addNativeVar("OPT_SSL_CTX_DATA", vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_DATA));
	mod->addNativeVar("OPT_INTERLEAVEFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEFUNCTION));
	mod->addNativeVar("OPT_INTERLEAVEDATA", vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEDATA));
	mod->addNativeVar("OPT_CHUNK_BGN_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_BGN_FUNCTION));
	mod->addNativeVar("OPT_CHUNK_END_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_END_FUNCTION));
	mod->addNativeVar("OPT_CHUNK_DATA", vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_DATA));
	mod->addNativeVar("OPT_FNMATCH_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_FUNCTION));
	mod->addNativeVar("OPT_FNMATCH_DATA", vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_DATA));
#if CURL_AT_LEAST_VERSION(7, 54, 0)
	mod->addNativeVar("OPT_SUPPRESS_CONNECT_HEADERS",
			  vm.makeVar<VarInt>(loc, CURLOPT_SUPPRESS_CONNECT_HEADERS));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_RESOLVER_START_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_FUNCTION));
	mod->addNativeVar("OPT_RESOLVER_START_DATA",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_DATA));
#endif

	// ERROR OPTIONS
	mod->addNativeVar("OPT_ERRORBUFFER", vm.makeVar<VarInt>(loc, CURLOPT_ERRORBUFFER));
	mod->addNativeVar("OPT_STDERR", vm.makeVar<VarInt>(loc, CURLOPT_STDERR));
	mod->addNativeVar("OPT_FAILONERROR", vm.makeVar<VarInt>(loc, CURLOPT_FAILONERROR));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
	mod->addNativeVar("OPT_KEEP_SENDING_ON_ERROR",
			  vm.makeVar<VarInt>(loc, CURLOPT_KEEP_SENDING_ON_ERROR));
#endif

	// NETWORK OPTIONS
	mod->addNativeVar("OPT_URL", vm.makeVar<VarInt>(loc, CURLOPT_URL));
	mod->addNativeVar("OPT_PATH_AS_IS", vm.makeVar<VarInt>(loc, CURLOPT_PATH_AS_IS));
	mod->addNativeVar("OPT_PROTOCOLS_STR", vm.makeVar<VarInt>(loc, CURLOPT_PROTOCOLS_STR));
	mod->addNativeVar("OPT_REDIR_PROTOCOLS_STR",
			  vm.makeVar<VarInt>(loc, CURLOPT_REDIR_PROTOCOLS_STR));
	mod->addNativeVar("OPT_DEFAULT_PROTOCOL",
			  vm.makeVar<VarInt>(loc, CURLOPT_DEFAULT_PROTOCOL));
	mod->addNativeVar("OPT_PROXY", vm.makeVar<VarInt>(loc, CURLOPT_PROXY));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PRE_PROXY", vm.makeVar<VarInt>(loc, CURLOPT_PRE_PROXY));
#endif
	mod->addNativeVar("OPT_PROXYPORT", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPORT));
	mod->addNativeVar("OPT_PROXYTYPE", vm.makeVar<VarInt>(loc, CURLOPT_PROXYTYPE));
	mod->addNativeVar("OPT_NOPROXY", vm.makeVar<VarInt>(loc, CURLOPT_NOPROXY));
	mod->addNativeVar("OPT_HTTPPROXYTUNNEL", vm.makeVar<VarInt>(loc, CURLOPT_HTTPPROXYTUNNEL));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
	mod->addNativeVar("OPT_CONNECT_TO", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_TO));
#endif
#if CURL_AT_LEAST_VERSION(7, 55, 0)
	mod->addNativeVar("OPT_SOCKS5_AUTH", vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_AUTH));
#endif
	mod->addNativeVar("OPT_SOCKS5_GSSAPI_NEC",
			  vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_GSSAPI_NEC));
	mod->addNativeVar("OPT_PROXY_SERVICE_NAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SERVICE_NAME));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
	mod->addNativeVar("OPT_HAPROXYPROTOCOL", vm.makeVar<VarInt>(loc, CURLOPT_HAPROXYPROTOCOL));
#endif
	mod->addNativeVar("OPT_SERVICE_NAME", vm.makeVar<VarInt>(loc, CURLOPT_SERVICE_NAME));
	mod->addNativeVar("OPT_INTERFACE", vm.makeVar<VarInt>(loc, CURLOPT_INTERFACE));
	mod->addNativeVar("OPT_LOCALPORT", vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORT));
	mod->addNativeVar("OPT_LOCALPORTRANGE", vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORTRANGE));
	mod->addNativeVar("OPT_DNS_CACHE_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLOPT_DNS_CACHE_TIMEOUT));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_DOH_URL", vm.makeVar<VarInt>(loc, CURLOPT_DOH_URL));
#endif
	mod->addNativeVar("OPT_BUFFERSIZE", vm.makeVar<VarInt>(loc, CURLOPT_BUFFERSIZE));
	mod->addNativeVar("OPT_PORT", vm.makeVar<VarInt>(loc, CURLOPT_PORT));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
	mod->addNativeVar("OPT_TCP_FASTOPEN", vm.makeVar<VarInt>(loc, CURLOPT_TCP_FASTOPEN));
#endif
	mod->addNativeVar("OPT_TCP_NODELAY", vm.makeVar<VarInt>(loc, CURLOPT_TCP_NODELAY));
	mod->addNativeVar("OPT_ADDRESS_SCOPE", vm.makeVar<VarInt>(loc, CURLOPT_ADDRESS_SCOPE));
	mod->addNativeVar("OPT_TCP_KEEPALIVE", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPALIVE));
	mod->addNativeVar("OPT_TCP_KEEPIDLE", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPIDLE));
	mod->addNativeVar("OPT_TCP_KEEPINTVL", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPINTVL));
	mod->addNativeVar("OPT_UNIX_SOCKET_PATH",
			  vm.makeVar<VarInt>(loc, CURLOPT_UNIX_SOCKET_PATH));
#if CURL_AT_LEAST_VERSION(7, 53, 0)
	mod->addNativeVar("OPT_ABSTRACT_UNIX_SOCKET",
			  vm.makeVar<VarInt>(loc, CURLOPT_ABSTRACT_UNIX_SOCKET));
#endif

	// NAMES and PASSWORDS OPTIONS (Authentication)
	mod->addNativeVar("OPT_NETRC", vm.makeVar<VarInt>(loc, CURLOPT_NETRC));
	mod->addNativeVar("OPT_NETRC_FILE", vm.makeVar<VarInt>(loc, CURLOPT_NETRC_FILE));
	mod->addNativeVar("OPT_USERPWD", vm.makeVar<VarInt>(loc, CURLOPT_USERPWD));
	mod->addNativeVar("OPT_PROXYUSERPWD", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERPWD));
	mod->addNativeVar("OPT_USERNAME", vm.makeVar<VarInt>(loc, CURLOPT_USERNAME));
	mod->addNativeVar("OPT_PASSWORD", vm.makeVar<VarInt>(loc, CURLOPT_PASSWORD));
	mod->addNativeVar("OPT_LOGIN_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_LOGIN_OPTIONS));
	mod->addNativeVar("OPT_PROXYUSERNAME", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERNAME));
	mod->addNativeVar("OPT_PROXYPASSWORD", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPASSWORD));
	mod->addNativeVar("OPT_HTTPAUTH", vm.makeVar<VarInt>(loc, CURLOPT_HTTPAUTH));
	mod->addNativeVar("OPT_TLSAUTH_USERNAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_USERNAME));
	mod->addNativeVar("OPT_TLSAUTH_PASSWORD",
			  vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_PASSWORD));
	mod->addNativeVar("OPT_TLSAUTH_TYPE", vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_TYPE));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PROXY_TLSAUTH_USERNAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_USERNAME));
	mod->addNativeVar("OPT_PROXY_TLSAUTH_PASSWORD",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_PASSWORD));
	mod->addNativeVar("OPT_PROXY_TLSAUTH_TYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_TYPE));
#endif
	mod->addNativeVar("OPT_PROXYAUTH", vm.makeVar<VarInt>(loc, CURLOPT_PROXYAUTH));
#if CURL_AT_LEAST_VERSION(7, 66, 0)
	mod->addNativeVar("OPT_SASL_AUTHZID", vm.makeVar<VarInt>(loc, CURLOPT_SASL_AUTHZID));
#endif
#if CURL_AT_LEAST_VERSION(7, 61, 0)
	mod->addNativeVar("OPT_SASL_IR", vm.makeVar<VarInt>(loc, CURLOPT_SASL_IR));
	mod->addNativeVar("OPT_DISALLOW_USERNAME_IN_URL",
			  vm.makeVar<VarInt>(loc, CURLOPT_DISALLOW_USERNAME_IN_URL));
#endif
	mod->addNativeVar("OPT_XOAUTH2_BEARER", vm.makeVar<VarInt>(loc, CURLOPT_XOAUTH2_BEARER));

	// HTTP OPTIONS
	mod->addNativeVar("OPT_AUTOREFERER", vm.makeVar<VarInt>(loc, CURLOPT_AUTOREFERER));
	mod->addNativeVar("OPT_ACCEPT_ENCODING", vm.makeVar<VarInt>(loc, CURLOPT_ACCEPT_ENCODING));
	mod->addNativeVar("OPT_TRANSFER_ENCODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_TRANSFER_ENCODING));
	mod->addNativeVar("OPT_FOLLOWLOCATION", vm.makeVar<VarInt>(loc, CURLOPT_FOLLOWLOCATION));
	mod->addNativeVar("OPT_UNRESTRICTED_AUTH",
			  vm.makeVar<VarInt>(loc, CURLOPT_UNRESTRICTED_AUTH));
	mod->addNativeVar("OPT_MAXREDIRS", vm.makeVar<VarInt>(loc, CURLOPT_MAXREDIRS));
	mod->addNativeVar("OPT_POSTREDIR", vm.makeVar<VarInt>(loc, CURLOPT_POSTREDIR));
	mod->addNativeVar("OPT_POST", vm.makeVar<VarInt>(loc, CURLOPT_POST));
	mod->addNativeVar("OPT_POSTFIELDS", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDS));
	mod->addNativeVar("OPT_POSTFIELDSIZE", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE));
	mod->addNativeVar("OPT_POSTFIELDSIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE_LARGE));
	mod->addNativeVar("OPT_COPYPOSTFIELDS", vm.makeVar<VarInt>(loc, CURLOPT_COPYPOSTFIELDS));
	mod->addNativeVar("OPT_REFERER", vm.makeVar<VarInt>(loc, CURLOPT_REFERER));
	mod->addNativeVar("OPT_USERAGENT", vm.makeVar<VarInt>(loc, CURLOPT_USERAGENT));
	mod->addNativeVar("OPT_HTTPHEADER", vm.makeVar<VarInt>(loc, CURLOPT_HTTPHEADER));
	mod->addNativeVar("OPT_HEADEROPT", vm.makeVar<VarInt>(loc, CURLOPT_HEADEROPT));
	mod->addNativeVar("OPT_PROXYHEADER", vm.makeVar<VarInt>(loc, CURLOPT_PROXYHEADER));
	mod->addNativeVar("OPT_HTTP200ALIASES", vm.makeVar<VarInt>(loc, CURLOPT_HTTP200ALIASES));
	mod->addNativeVar("OPT_COOKIE", vm.makeVar<VarInt>(loc, CURLOPT_COOKIE));
	mod->addNativeVar("OPT_COOKIEFILE", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEFILE));
	mod->addNativeVar("OPT_COOKIEJAR", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEJAR));
	mod->addNativeVar("OPT_COOKIESESSION", vm.makeVar<VarInt>(loc, CURLOPT_COOKIESESSION));
	mod->addNativeVar("OPT_COOKIELIST", vm.makeVar<VarInt>(loc, CURLOPT_COOKIELIST));
#if CURL_AT_LEAST_VERSION(7, 64, 1)
	mod->addNativeVar("OPT_ALTSVC", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC));
	mod->addNativeVar("OPT_ALTSVC_CTRL", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC_CTRL));
#endif
	mod->addNativeVar("OPT_HTTPGET", vm.makeVar<VarInt>(loc, CURLOPT_HTTPGET));
#if CURL_AT_LEAST_VERSION(7, 55, 0)
	mod->addNativeVar("OPT_REQUEST_TARGET", vm.makeVar<VarInt>(loc, CURLOPT_REQUEST_TARGET));
#endif
	mod->addNativeVar("OPT_HTTP_VERSION", vm.makeVar<VarInt>(loc, CURLOPT_HTTP_VERSION));
#if CURL_AT_LEAST_VERSION(7, 64, 0)
	mod->addNativeVar("OPT_HTTP09_ALLOWED", vm.makeVar<VarInt>(loc, CURLOPT_HTTP09_ALLOWED));
	mod->addNativeVar("OPT_TRAILERFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_TRAILERFUNCTION));
	mod->addNativeVar("OPT_TRAILERDATA", vm.makeVar<VarInt>(loc, CURLOPT_TRAILERDATA));
#endif
	mod->addNativeVar("OPT_IGNORE_CONTENT_LENGTH",
			  vm.makeVar<VarInt>(loc, CURLOPT_IGNORE_CONTENT_LENGTH));
	mod->addNativeVar("OPT_HTTP_CONTENT_DECODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_HTTP_CONTENT_DECODING));
	mod->addNativeVar("OPT_HTTP_TRANSFER_DECODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_HTTP_TRANSFER_DECODING));
	mod->addNativeVar("OPT_EXPECT_100_TIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_EXPECT_100_TIMEOUT_MS));
	mod->addNativeVar("OPT_PIPEWAIT", vm.makeVar<VarInt>(loc, CURLOPT_PIPEWAIT));
	mod->addNativeVar("OPT_STREAM_DEPENDS", vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS));
	mod->addNativeVar("OPT_STREAM_DEPENDS_E",
			  vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS_E));
	mod->addNativeVar("OPT_STREAM_WEIGHT", vm.makeVar<VarInt>(loc, CURLOPT_STREAM_WEIGHT));

	// SMTP OPTIONS
	mod->addNativeVar("OPT_MAIL_FROM", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_FROM));
	mod->addNativeVar("OPT_MAIL_RCPT", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT));
	mod->addNativeVar("OPT_MAIL_AUTH", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_AUTH));
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("OPT_MAIL_RCPT_ALLLOWFAILS",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT_ALLLOWFAILS));
#endif

	// TFTP OPTIONS
	mod->addNativeVar("OPT_TFTP_BLKSIZE", vm.makeVar<VarInt>(loc, CURLOPT_TFTP_BLKSIZE));
#if CURL_AT_LEAST_VERSION(7, 48, 0)
	mod->addNativeVar("OPT_TFTP_NO_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_TFTP_NO_OPTIONS));
#endif

	// FTP OPTIONS
	mod->addNativeVar("OPT_FTPPORT", vm.makeVar<VarInt>(loc, CURLOPT_FTPPORT));
	mod->addNativeVar("OPT_QUOTE", vm.makeVar<VarInt>(loc, CURLOPT_QUOTE));
	mod->addNativeVar("OPT_POSTQUOTE", vm.makeVar<VarInt>(loc, CURLOPT_POSTQUOTE));
	mod->addNativeVar("OPT_PREQUOTE", vm.makeVar<VarInt>(loc, CURLOPT_PREQUOTE));
	mod->addNativeVar("OPT_APPEND", vm.makeVar<VarInt>(loc, CURLOPT_APPEND));
	mod->addNativeVar("OPT_FTP_USE_EPRT", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPRT));
	mod->addNativeVar("OPT_FTP_USE_EPSV", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPSV));
	mod->addNativeVar("OPT_FTP_USE_PRET", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_PRET));
	mod->addNativeVar("OPT_FTP_CREATE_MISSING_DIRS",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_CREATE_MISSING_DIRS));
	mod->addNativeVar("OPT_FTP_RESPONSE_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_RESPONSE_TIMEOUT));
	mod->addNativeVar("OPT_FTP_ALTERNATIVE_TO_USER",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_ALTERNATIVE_TO_USER));
	mod->addNativeVar("OPT_FTP_SKIP_PASV_IP",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_SKIP_PASV_IP));
	mod->addNativeVar("OPT_FTPSSLAUTH", vm.makeVar<VarInt>(loc, CURLOPT_FTPSSLAUTH));
	mod->addNativeVar("OPT_FTP_SSL_CCC", vm.makeVar<VarInt>(loc, CURLOPT_FTP_SSL_CCC));
	mod->addNativeVar("OPT_FTP_ACCOUNT", vm.makeVar<VarInt>(loc, CURLOPT_FTP_ACCOUNT));
	mod->addNativeVar("OPT_FTP_FILEMETHOD", vm.makeVar<VarInt>(loc, CURLOPT_FTP_FILEMETHOD));

	// RTSP OPTIONS
	mod->addNativeVar("OPT_RTSP_REQUEST", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_REQUEST));
	mod->addNativeVar("OPT_RTSP_SESSION_ID", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SESSION_ID));
	mod->addNativeVar("OPT_RTSP_STREAM_URI", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_STREAM_URI));
	mod->addNativeVar("OPT_RTSP_TRANSPORT", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_TRANSPORT));
	mod->addNativeVar("OPT_RTSP_CLIENT_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLOPT_RTSP_CLIENT_CSEQ));
	mod->addNativeVar("OPT_RTSP_SERVER_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SERVER_CSEQ));

	// PROTOCOL OPTIONS
	mod->addNativeVar("OPT_TRANSFERTEXT", vm.makeVar<VarInt>(loc, CURLOPT_TRANSFERTEXT));
	mod->addNativeVar("OPT_PROXY_TRANSFER_MODE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TRANSFER_MODE));
	mod->addNativeVar("OPT_CRLF", vm.makeVar<VarInt>(loc, CURLOPT_CRLF));
	mod->addNativeVar("OPT_RANGE", vm.makeVar<VarInt>(loc, CURLOPT_RANGE));
	mod->addNativeVar("OPT_RESUME_FROM", vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM));
	mod->addNativeVar("OPT_RESUME_FROM_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM_LARGE));
#if CURL_AT_LEAST_VERSION(7, 63, 0)
	mod->addNativeVar("OPT_CURLU", vm.makeVar<VarInt>(loc, CURLOPT_CURLU));
#endif
	mod->addNativeVar("OPT_CUSTOMREQUEST", vm.makeVar<VarInt>(loc, CURLOPT_CUSTOMREQUEST));
	mod->addNativeVar("OPT_FILETIME", vm.makeVar<VarInt>(loc, CURLOPT_FILETIME));
	mod->addNativeVar("OPT_DIRLISTONLY", vm.makeVar<VarInt>(loc, CURLOPT_DIRLISTONLY));
	mod->addNativeVar("OPT_NOBODY", vm.makeVar<VarInt>(loc, CURLOPT_NOBODY));
	mod->addNativeVar("OPT_INFILESIZE", vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE));
	mod->addNativeVar("OPT_INFILESIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE_LARGE));
	mod->addNativeVar("OPT_UPLOAD", vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_UPLOAD_BUFFERSIZE",
			  vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD_BUFFERSIZE));
#endif
#if CURL_AT_LEAST_VERSION(7, 56, 0)
	mod->addNativeVar("OPT_MIMEPOST", vm.makeVar<VarInt>(loc, CURLOPT_MIMEPOST));
#endif
	mod->addNativeVar("OPT_MAXFILESIZE", vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE));
	mod->addNativeVar("OPT_MAXFILESIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE_LARGE));
	mod->addNativeVar("OPT_TIMECONDITION", vm.makeVar<VarInt>(loc, CURLOPT_TIMECONDITION));
	mod->addNativeVar("OPT_TIMEVALUE", vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_TIMEVALUE_LARGE", vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE_LARGE));
#endif

	// CONNECTION OPTIONS
	mod->addNativeVar("OPT_TIMEOUT", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT));
	mod->addNativeVar("OPT_TIMEOUT_MS", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT_MS));
	mod->addNativeVar("OPT_LOW_SPEED_LIMIT", vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_LIMIT));
	mod->addNativeVar("OPT_LOW_SPEED_TIME", vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_TIME));
	mod->addNativeVar("OPT_MAX_SEND_SPEED_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAX_SEND_SPEED_LARGE));
	mod->addNativeVar("OPT_MAX_RECV_SPEED_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAX_RECV_SPEED_LARGE));
	mod->addNativeVar("OPT_MAXCONNECTS", vm.makeVar<VarInt>(loc, CURLOPT_MAXCONNECTS));
	mod->addNativeVar("OPT_FRESH_CONNECT", vm.makeVar<VarInt>(loc, CURLOPT_FRESH_CONNECT));
	mod->addNativeVar("OPT_FORBID_REUSE", vm.makeVar<VarInt>(loc, CURLOPT_FORBID_REUSE));
#if CURL_AT_LEAST_VERSION(7, 65, 0)
	mod->addNativeVar("OPT_MAXAGE_CONN", vm.makeVar<VarInt>(loc, CURLOPT_MAXAGE_CONN));
#endif
	mod->addNativeVar("OPT_CONNECTTIMEOUT", vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT));
	mod->addNativeVar("OPT_CONNECTTIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT_MS));
	mod->addNativeVar("OPT_IPRESOLVE", vm.makeVar<VarInt>(loc, CURLOPT_IPRESOLVE));
	mod->addNativeVar("OPT_CONNECT_ONLY", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_ONLY));
	mod->addNativeVar("OPT_USE_SSL", vm.makeVar<VarInt>(loc, CURLOPT_USE_SSL));
	mod->addNativeVar("OPT_RESOLVE", vm.makeVar<VarInt>(loc, CURLOPT_RESOLVE));
	mod->addNativeVar("OPT_DNS_INTERFACE", vm.makeVar<VarInt>(loc, CURLOPT_DNS_INTERFACE));
	mod->addNativeVar("OPT_DNS_LOCAL_IP4", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP4));
	mod->addNativeVar("OPT_DNS_LOCAL_IP6", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP6));
	mod->addNativeVar("OPT_DNS_SERVERS", vm.makeVar<VarInt>(loc, CURLOPT_DNS_SERVERS));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
	mod->addNativeVar("OPT_DNS_SHUFFLE_ADDRESSES",
			  vm.makeVar<VarInt>(loc, CURLOPT_DNS_SHUFFLE_ADDRESSES));
#endif
	mod->addNativeVar("OPT_ACCEPTTIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_ACCEPTTIMEOUT_MS));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_HAPPY_EYEBALLS_TIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS));
#endif
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_UPKEEP_INTERVAL_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_UPKEEP_INTERVAL_MS));
#endif

	// SSL and SECURITY OPTIONS
	mod->addNativeVar("OPT_SSLCERT", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERT));
	mod->addNativeVar("OPT_SSLCERTTYPE", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERTTYPE));
	mod->addNativeVar("OPT_SSLKEY", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEY));
	mod->addNativeVar("OPT_SSLKEYTYPE", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEYTYPE));
	mod->addNativeVar("OPT_KEYPASSWD", vm.makeVar<VarInt>(loc, CURLOPT_KEYPASSWD));
	mod->addNativeVar("OPT_SSL_ENABLE_ALPN", vm.makeVar<VarInt>(loc, CURLOPT_SSL_ENABLE_ALPN));
	mod->addNativeVar("OPT_SSLENGINE", vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE));
	mod->addNativeVar("OPT_SSLENGINE_DEFAULT",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE_DEFAULT));
	mod->addNativeVar("OPT_SSL_FALSESTART", vm.makeVar<VarInt>(loc, CURLOPT_SSL_FALSESTART));
	mod->addNativeVar("OPT_SSLVERSION", vm.makeVar<VarInt>(loc, CURLOPT_SSLVERSION));
	mod->addNativeVar("OPT_SSL_VERIFYPEER", vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYPEER));
	mod->addNativeVar("OPT_SSL_VERIFYHOST", vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYHOST));
	mod->addNativeVar("OPT_SSL_VERIFYSTATUS",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYSTATUS));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PROXY_CAINFO", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAINFO));
	mod->addNativeVar("OPT_PROXY_CAPATH", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAPATH));
	mod->addNativeVar("OPT_PROXY_CRLFILE", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CRLFILE));
	mod->addNativeVar("OPT_PROXY_KEYPASSWD", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_KEYPASSWD));
	mod->addNativeVar("OPT_PROXY_PINNEDPUBLICKEY",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_PINNEDPUBLICKEY));
	mod->addNativeVar("OPT_PROXY_SSLCERT", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERT));
	mod->addNativeVar("OPT_PROXY_SSLCERTTYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERTTYPE));
	mod->addNativeVar("OPT_PROXY_SSLKEY", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEY));
	mod->addNativeVar("OPT_PROXY_SSLKEYTYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEYTYPE));
	mod->addNativeVar("OPT_PROXY_SSLVERSION",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLVERSION));
	mod->addNativeVar("OPT_PROXY_SSL_CIPHER_LIST",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_CIPHER_LIST));
	mod->addNativeVar("OPT_PROXY_SSL_OPTIONS",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_OPTIONS));
	mod->addNativeVar("OPT_PROXY_SSL_VERIFYHOST",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYHOST));
	mod->addNativeVar("OPT_PROXY_SSL_VERIFYPEER",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYPEER));
#endif
	mod->addNativeVar("OPT_CAINFO", vm.makeVar<VarInt>(loc, CURLOPT_CAINFO));
	mod->addNativeVar("OPT_ISSUERCERT", vm.makeVar<VarInt>(loc, CURLOPT_ISSUERCERT));
	mod->addNativeVar("OPT_CAPATH", vm.makeVar<VarInt>(loc, CURLOPT_CAPATH));
	mod->addNativeVar("OPT_CRLFILE", vm.makeVar<VarInt>(loc, CURLOPT_CRLFILE));
	mod->addNativeVar("OPT_CERTINFO", vm.makeVar<VarInt>(loc, CURLOPT_CERTINFO));
	mod->addNativeVar("OPT_PINNEDPUBLICKEY", vm.makeVar<VarInt>(loc, CURLOPT_PINNEDPUBLICKEY));
	mod->addNativeVar("OPT_SSL_CIPHER_LIST", vm.makeVar<VarInt>(loc, CURLOPT_SSL_CIPHER_LIST));
#if CURL_AT_LEAST_VERSION(7, 61, 0)
	mod->addNativeVar("OPT_TLS13_CIPHERS", vm.makeVar<VarInt>(loc, CURLOPT_TLS13_CIPHERS));
	mod->addNativeVar("OPT_PROXY_TLS13_CIPHERS",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLS13_CIPHERS));
#endif
	mod->addNativeVar("OPT_SSL_SESSIONID_CACHE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_SESSIONID_CACHE));
	mod->addNativeVar("OPT_SSL_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_SSL_OPTIONS));
	mod->addNativeVar("OPT_KRBLEVEL", vm.makeVar<VarInt>(loc, CURLOPT_KRBLEVEL));
	mod->addNativeVar("OPT_GSSAPI_DELEGATION",
			  vm.makeVar<VarInt>(loc, CURLOPT_GSSAPI_DELEGATION));

	// SSH OPTIONS
	mod->addNativeVar("OPT_SSH_AUTH_TYPES", vm.makeVar<VarInt>(loc, CURLOPT_SSH_AUTH_TYPES));
#if CURL_AT_LEAST_VERSION(7, 56, 0)
	mod->addNativeVar("OPT_SSH_COMPRESSION", vm.makeVar<VarInt>(loc, CURLOPT_SSH_COMPRESSION));
#endif
	mod->addNativeVar("OPT_SSH_HOST_PUBLIC_KEY_MD5",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5));
	mod->addNativeVar("OPT_SSH_PUBLIC_KEYFILE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_PUBLIC_KEYFILE));
	mod->addNativeVar("OPT_SSH_PRIVATE_KEYFILE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_PRIVATE_KEYFILE));
	mod->addNativeVar("OPT_SSH_KNOWNHOSTS", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KNOWNHOSTS));
	mod->addNativeVar("OPT_SSH_KEYFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYFUNCTION));
	mod->addNativeVar("OPT_SSH_KEYDATA", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYDATA));

	// OTHER OPTIONS
	mod->addNativeVar("OPT_PRIVATE", vm.makeVar<VarInt>(loc, CURLOPT_PRIVATE));
	mod->addNativeVar("OPT_SHARE", vm.makeVar<VarInt>(loc, CURLOPT_SHARE));
	mod->addNativeVar("OPT_NEW_FILE_PERMS", vm.makeVar<VarInt>(loc, CURLOPT_NEW_FILE_PERMS));
	mod->addNativeVar("OPT_NEW_DIRECTORY_PERMS",
			  vm.makeVar<VarInt>(loc, CURLOPT_NEW_DIRECTORY_PERMS));

	// TELNET OPTIONS
	mod->addNativeVar("OPT_TELNETOPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_TELNETOPTIONS));

	curl_global_init(CURL_GLOBAL_ALL);

	return true;
}

DEINIT_MODULE(Curl)
{
	decref(writeCallback);
	decref(progressCallback);
	for(auto &e : hss) curl_slist_free_all(e);
	curl_global_cleanup();
}

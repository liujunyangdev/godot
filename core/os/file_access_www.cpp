/*************************************************************************/
/*  file_access_unix.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "file_access_www.h"

#include "core/os/os.h"
#include "core/io/http_client.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include "core/os/os.h"

void FileAccessWwwClient::lock_mutex() {

	mutex.lock();
	lockcount++;
}

void FileAccessWwwClient::unlock_mutex() {

	lockcount--;
	mutex.unlock();
}



int FileAccessWwwClient::poll_back(uint8_t *p_buf, int pos,int p_size) {

	return fileAccessInfo.read(p_buf,p_size,true);
}


Error FileAccessWwwClient::connect(const String &p_path) {
	ERR_FAIL_COND_V_MSG(p_path.empty(), ERR_CANT_CREATE, "http_request url is empty");
	path_src = p_path;
	pos = 0;
	String scheme;
	Error err = p_path.parse_url(scheme, url, port, request_string);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Error parsing URL: " + p_path + ".");

	if(p_path.begins_with("https")){
		port = 443;
		p_ssl = true;
	}

	List<String> rheaders;

	PoolVector<uint8_t> rb;
	
	Vector<String> header;

	String hRang = "Range: bytes=0-1";

	header.push_back(hRang); 

	http_request(header,rb,rheaders);

	for (const List<String>::Element *E = rheaders.front(); E; E = E->next()) {
		String s = E->get();
		if(s.begins_with("Content-Range")){
			int idnexnum = s.find("/");
			String sizenum = s.substr( idnexnum + 1 , -1);
			total_size = sizenum.to_int64();
			break;
		}
	}

	fileAccessInfo.resize(24);
	
	thread.start(_thread_func, this);

	return OK;
}

void FileAccessWwwClient::_thread_func() {
	while (!quit) {
		if(fileAccessInfo.space_left() > p_lenght && isfileAccessInfo){
			lock_mutex();
			get_buffer_data();
			unlock_mutex();
			OS::get_singleton()->delay_usec(10000); //防止线程独占。每次循环延迟0.1秒
		}
	}
}

void FileAccessWwwClient::_thread_func(void *s) {

	FileAccessWwwClient *self = (FileAccessWwwClient *)s;

	self->_thread_func();
}

Error FileAccessWwwClient::http_request( Vector<String> &header,PoolVector<uint8_t> &rb,List<String> &rheaders) const {
	ERR_FAIL_COND_V_MSG(url.empty(), ERR_CANT_CREATE, "http_request url is empty");
	ERR_FAIL_COND_V_MSG(port == NULL, ERR_CANT_CREATE, "http_request port is empty");
	ERR_FAIL_COND_V_MSG(path_src.empty(), ERR_CANT_CREATE, "http_request path_src is empty");


	if(hc->get_status() != HTTPClient::Status::STATUS_CONNECTED){
		hc = new HTTPClient();
		Error ee = hc->connect_to_host(url,port,p_ssl,false);
		ERR_FAIL_COND_V_MSG(ee != OK, ee, "Error parsing URL: " + path_src + ".");
		while (hc->get_status() == HTTPClient::Status::STATUS_CONNECTING || hc->get_status() == HTTPClient::Status::STATUS_RESOLVING){
			hc->poll();
		}
	}

	header.push_back("connection: keep-alive"); 

	PoolByteArray body;

	hc->request_raw(HTTPClient::METHOD_GET,path_src,header,body);

	while(hc->get_status() == HTTPClient::Status::STATUS_REQUESTING){
		hc->poll();
	}

	ERR_FAIL_COND_V_MSG(!hc->has_response(), Error(FAILED), "Error has_response: false " + path_src + ".");

	while(hc->get_status() == HTTPClient::Status::STATUS_BODY){
		hc->poll();
		PoolVector<uint8_t> chunk = hc->read_response_body_chunk();
		rb.append_array(chunk);
	}

	Error eeee = hc->get_response_headers(&rheaders);

	ERR_FAIL_COND_V_MSG(eeee != OK, eeee, "Error get_response_headers: " + path_src + ".");

	hc->close();
	
	return OK;
}

void FileAccessWwwClient::get_buffer_data() {
	List<String> rheaders;

	PoolVector<uint8_t> rb;
	
	Vector<String> header;

	String hRang = "Range: bytes="+String::num_int64(pos) + "-" + String::num_int64(p_lenght + pos -1);

	header.push_back(hRang); 

	http_request(header,rb,rheaders);

	if(rb.size() > 0){
		PoolByteArray::Read r = rb.read();
		fileAccessInfo.write(r.ptr(),rb.size());
		pos = (pos + p_lenght)<total_size?(pos + p_lenght):total_size;
		sem.post();
	}else{
		isfileAccessInfo = false;
	}
}


FileAccessWwwClient *FileAccessWwwClient::singleton = NULL;

FileAccessWwwClient::FileAccessWwwClient() {

	quit = false;
	singleton = this;
	lockcount = 0;
	isfileAccessInfo = true;
	posStart = 0;
}

FileAccessWwwClient::~FileAccessWwwClient() {

	if (thread.is_started()) {
		quit = true;
		thread.wait_to_finish();
	}
}




void FileAccessWww::check_errors() const {

	last_error = ERR_FILE_EOF;
}

Error FileAccessWww::_open(const String &p_path, int p_mode_flags){

	FileAccessWwwClient *ss = memnew(FileAccessWwwClient);

	Error ee = ss->connect(p_path);

	ERR_FAIL_COND_V_MSG(ee != OK, ERR_CANT_CREATE, "FileAccessWwwClient connect faild");
	
	total_size = ss->total_size;

	return OK;
}

void FileAccessWww::close() {
}

bool FileAccessWww::is_open() const {

	return true;
}

String FileAccessWww::get_path() const {

	return path_src;
}

String FileAccessWww::get_path_absolute() const {

	return path_src;
}

void FileAccessWww::seek(size_t p_position) {

	buffer_mutex.lock();
	if (p_position >= total_size) {
		p_position = total_size;
	}
	pos = p_position;
	FileAccessWwwClient *fwc = FileAccessWwwClient::singleton;
	fwc->lock_mutex();
	fwc->pos = pos;
	fwc->isfileAccessInfo = true;
	fwc->fileAccessInfo.clear();
	fwc->sem.clear();
	fwc->unlock_mutex();
	buffer_mutex.unlock();
}

void FileAccessWww::seek_end(int64_t p_position) {
	
	seek(total_size + p_position);
}

size_t FileAccessWww::get_position() const {

	return pos;
}

size_t FileAccessWww::get_len() const {

	return total_size;
}

bool FileAccessWww::eof_reached() const {

	return last_error == ERR_FILE_EOF;
}
uint8_t FileAccessWww::get_8() const {

	return 1;
}

int FileAccessWww::get_buffer(uint8_t *p_dst, int p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_COND_V(p_length < 0, -1);

	buffer_mutex.lock();
	FileAccessWwwClient *fwc = FileAccessWwwClient::singleton;
	if (pos != total_size){
		fwc->sem.wait();
	}
	int size = fwc->poll_back(p_dst,pos,p_length);

	pos = pos + size;
	buffer_mutex.unlock();

	return size;
};
Error FileAccessWww::get_error() const {

	return last_error;
}

void FileAccessWww::flush() {

}

void FileAccessWww::store_8(uint8_t p_dest) {

}

void FileAccessWww::store_buffer(const uint8_t *p_src, int p_length) {

}

bool FileAccessWww::file_exists(const String &p_path) {

	return true;
}

uint64_t FileAccessWww::_get_modified_time(const String &p_file) {
	return 1;
}

uint32_t FileAccessWww::_get_unix_permissions(const String &p_file) {
	return 1;
}

Error FileAccessWww::_set_unix_permissions(const String &p_file, uint32_t p_permissions) {
	return OK;
}

FileAccess *FileAccessWww::create_libc() {

	return memnew(FileAccessWww);
}

CloseNotificationFunc FileAccessWww::close_notification_func = NULL;

FileAccessWww::FileAccessWww() :
		last_error(OK) {
}

FileAccessWww::~FileAccessWww() {
	close();
}


// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * FUNCTION DECLARATION
 *		size_t find(const string& str, size_t pos=0);
 *		size_t find(cosnt char *s, size_t pos=0); // from pos, find first s
 *		string substr(int pos=0, int n=npos);  // return n characters from pos
 *
 * extent_client
 *		extent_protocol::status get(id, buf) --> read_file(id, &buf, &size)
 */

yfs_client::yfs_client()
{
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

// name to inum
yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

yfs_client::dirTable::dirTable(std::string path)
{
	// string syncopate
	// dirtable initial
	size_t cur = 0;
	size_t next;
	
	while (cur < path.size())
	{
		next = path.find(':', cur);
		std::string name = path.substr(cur, next-cur); //get file name
		cur = next + 1;
		
		next = path.find('/', cur);
		inum id = n2i(path.substr(cur, next-cur)); // name to inum
		cur = next + 1;

		table.insert(std::pair<std::string, inum>(name, id));
	}
}

std::string
yfs_client::dirTable::dump()
{
	// my own dir format
	std::string content = "";
	for (std::map<std::string, inum>::iterator it = table.begin(); it != table.end(); it++) {
		content = content + it->first + ":" + filename(it->second) + "/";
	}
	return content;
}

void 
yfs_client::dirTable::insert(std::string name, inum id)
{
	// insert to map
	table.insert(std::pair<std::string, inum>(name, id));
}

void 
yfs_client::dirTable::remove(std::string name)
{
	// remove from map
	table.erase(name);
}

bool
yfs_client::dirTable::lookup(std::string name, inum& id)
{
	// lookup name
	std::map<std::string, inum>::iterator it = table.find(name); //iterator
	
	if (it != table.end()){ //if find name
		id = it->second;
		return true;
	} else {
		return false;
	}
}

void
yfs_client::dirTable::list(std::list<dirent> &list)
{
	// list dirent
	for (std::map<std::string, inum>::iterator it = table.begin(); it != table.end(); it++)
	{
		struct dirent entry; //{name, inum}
		entry.name = it->first;
		entry.inum = it->second;
		list.push_back(entry); // put to back
	}
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    return ! isfile(inum);
}

bool
yfs_client::issymlink(inum inum)
{
	return true;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// part 2-B
// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    return r;
}

// part 2-A
int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
	
	// extent_protocol::extentd_t &eid --> inum

	std::string buf;
    r = ec->get(parent, buf); // read dir name, buf=dirname
    if (r != OK){
        printf("\t create : parent directory do not exist \n");
        return r;
    }

	inum id;
	dirTable table(buf); //init
	if (table.lookup((std::string)name, id)){
        printf("\t create : file already exist \n");
        return EXIST;
	}
	
	// extent_protocol.h 
	// enum types { T_DIR=1, T_FILE };	

	r = ec->create(extent_protocol::T_FILE, ino_out);
	if (r != OK){
        printf("\t create : create fails \n");
        return r;
    }
	
	table.insert(std::string(name), ino_out);
	r = ec->put(parent, table.dump());  // write_file, table.dump() is own format
	if (r != OK){
        printf("\t create : parent directory update fails \n");
        return r;
    }

    return r;
}

// part 2-C
int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    return r;
}

// part 2-A
int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
	
	std::string buf;
	r = ec->get(parent, buf); // read dir name
	if (r != OK){
		printf("\t lookup : parent directory do not exist \n");
		return r;
	}
	
	dirTable table(buf);
	found = table.lookup((std::string)name, ino_out); //coercion

    return r;
}

// part 2-A
int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

	std::string buf;
	r = ec->get(dir, buf);
	if (r != OK){
        printf("\t readdir : parent directory do not exist \n");
        return r;
    }
	
	dirTable table(buf);
	table.list(list);	

    return r;
}

// part 2-B
int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    return r;
}

// part 2-B
int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    return r;
}

// part 2-C
int 
yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    return r;
}

// part 2-D
int
yfs_client::readlink(inum ino, std::string &data)
{
	int r = OK;
	// code here

	return r;
}


// part 2-D
int 
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
	int r = OK;
	// code here

	return r;
}










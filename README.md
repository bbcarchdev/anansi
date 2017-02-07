This is Anansi, a Linked Open Data (LOD) web crawler developed as part of
the [Research & Education Space](https://bbcarchdev.github.io/res/) (RES)
project.

## Dependencies

* POSIX threads
* OpenSSL
* cURL
* libxml2
* Redland (librdf)
* libuuid
* [Jansson](http://www.digip.org/jansson/)
* [liburi](https://github.com/bbcarchdev/liburi)
* [libsql](https://github.com/bbcarchdev/libsql)
* [libcluster](https://github.com/bbcarchdev/libcluster)
* [libawsclient](https://github.com/bbcarchdev/libawsclient)

## Clustering

To run multiple threads or instances of anansi you will need to
install etcd and add a [cluster] section to the crawl.conf file.

eg.
[cluster]
environment=development
name=anansi
registry=http://etcd:2379/

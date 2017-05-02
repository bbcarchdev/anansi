# Anansi

A web crawler and crawling framework

[![Current build status][travis]](https://travis-ci.org/bbcarchdev/anansi)
[![Apache 2.0 licensed][license]](#license)
![Implemented in C][language]
[![Follow @RES_Project][twitter]](https://twitter.com/RES_Project)

[Anansi](https://github.com/anansi/) is a web crawler which includes specific
support for [Linked Data](http://linkeddata.org) and can be operated
out-of-the-box or used as a framework for developing your own crawling
applications.

This software was developed as part of the [Research & Education Space project](https://bbcarchdev.github.io/res/) and is actively maintained by a development team within BBC Design and Engineering. We hope you’ll find this project useful!

## Table of Contents

* [Requirements](#requirements)
* [Using Anansi](#using-anansi)
* [Bugs and feature requests](#bugs-and-feature-requests)
* [Building from source](#building-from-source)
* [Automated builds](#automated-builds)
* [Contributing](#contributing)
* [Information for BBC Staff](#information-for-bbc-staff)
* [License](#license)

## Requirements

* A working build environment, including a C compiler such as [Clang](https://clang.llvm.org), along with [Autoconf](https://www.gnu.org/software/autoconf/), [Automake](https://www.gnu.org/software/automake/) and [Libtool](https://www.gnu.org/software/libtool/)
* The [Jansson](http://www.digip.org/jansson/) JSON library
* [libcurl](https://curl.haxx.se/libcurl/)
* [libxml2](http://xmlsoft.org/)
* The [Redland](http://librdf.org) RDF library (a more up-to-date version than your operating system provides may be required for some components to function correctly)
* The following other BBC projects:—
* * [liburi](https://github.com/bbcarchdev/liburi)
* * [libsql](https://github.com/bbcarchdev/libsql)
* * [libcluster](https://github.com/bbcarchdev/libcluster)
* * [libawsclient](https://github.com/bbcarchdev/libawsclient)

Optionally, you may also wish to install:—

* [`xsltproc`](http://xmlsoft.org/xslt/) and the [DocBook 5 XSL Stylesheets](http://wiki.docbook.org/DocBookXslStylesheets)

On Debian-based systems, the following will install those required packages
which are generally-available in APT repositories:

	  sudo apt-get install -qq libjansson-dev libmysqlclient-dev libcurl4-gnutls-dev libxml2-dev librdf0-dev libltdl-dev uuid-dev automake autoconf libtool pkg-config clang build-essential xsltproc docbook-xsl-ns

Anansi has not yet been ported to non-Unix-like environments, and will install
as shared libraries and command-line tools on macOS rather than frameworks and
LaunchDaemons.

It ought to build inside Cygwin on Windows, but this is untested.

[Contributions](#contributing) for building properly with Visual Studio or
Xcode, and so on, are welcome (provided they do not significantly
complicate the standard build logic).

## Using Anansi

### Configuring the crawler

The first time you install Anansi, an example [`crawl.conf`](crawler/crawl.conf)
will be installed to `$(sysconfdir)` (by default, `/usr/local/etc`).

### Invoking the crawler

The crawl daemon is installed by default as `$(sbindir)/crawld`, which will
typically be `/usr/local/sbin/crawld`.

After you’ve initially configured the crawler, you should perform any
database schema updates which may be required:

    $ /usr/local/sbin/crawld -S

This happens automatically when you launch it, but the `-S` option will
give you an opportunity to see the results of a first run without
examining log files, and will cause the daemon to terminate after
ensuring the schema is up to date.

To run the crawler in the foreground, with debugging enabled:

    $ /usr/local/sbin/crawld -d

Or to run it in the foreground, without debug-level verbosity:

    $ /usr/local/sbin/crawld -f

Alternatively, to run in the background:

    $ /usr/local/sbin/crawld -f

If you want to perform a single test fetch of a URI using your current
configuration, you can do this with:

    $ /usr/local/sbin/crawld -t http://example.com/somelocation

Once you’ve configured the crawler, you can add a URI to its queue
using the `crawler-add` utility, installed as `$(bindir)/crawler-add`
(typically `/usr/local/bin/crawler-add`). Note that `crawld` does not
have to be running in order to add URIs to the queue.

### Components

* [crawler](crawler) - the crawler daemon, its components, and command-line tools
* [libspider](libspider) - a high-level library which implements the core of the Anansi crawler
* [libcrawl](libcrawl) - a low-level library implementing the basic logic of a crawler
* [libsupport](libsupport) - utility library providing support for configuration files and logging

## Bugs and feature requests

If you’ve found a bug, or have thought of a feature that you would like to
see added, you can [file a new issue](https://github.com/bbcarchdev/anansi/issues). A member of the development team will triage it and add it to our internal prioritised backlog for development—but in the meantime we [welcome contributions](#contributing) and [encourage forking](https://github.com/bbcarchdev/anansi/fork).

## Building from source

You will need git, automake, autoconf and libtool. Also see the [Requirements](#requirements)
section.

    $ git clone git://github.com/bbcarchdev/anansi.git
    $ cd anansi
    $ git submodule update --init --recursive
    $ autoreconf -i
    $ ./configure --prefix=/some/path
    $ make
    $ make check
    $ sudo make install

## Automated builds

We have configured [Travis](https://travis-ci.org/bbcarchdev/anansi) to automatically build and invoke the tests on Anansi for new commits on each branch. See [`.travis.yml`](.travis.yml) for the details.

You may wish to do similar for your own forks, if you intend to maintain them.

The [`debian`](debian) directory contains the logic required to build a Debian
package for Anansi, except for the `changelog`. This is used by the system
that auto-deploys packages for the production [Research & Education Space](https://bbcarchdev.github.io/res/),
and so if you need a modified version to suit your own deployment needs,
it’s probably easiest to maintain a fork of this repository with your changes
in.

## Contributing

If you’d like to contribute to Anansi, [fork this repository](https://github.com/bbcarchdev/anansi/fork) and commit your changes to the
`develop` branch.

For larger changes, you should create a feature branch with
a meaningful name, for example one derived from the [issue number](https://github.com/bbcarchdev/anansi/issues/).

Once you are satisfied with your contribution, open a pull request and describe
the changes you’ve made and a member of the development team will take a look.

## Information for BBC Staff

This is an open source project which is actively maintained and developed
by a team within Design and Engineering. Please bear in mind the following:—

* Bugs and feature requests **must** be filed in [GitHub Issues](https://github.com/bbcarchdev/anansi/issues): this is the authoratitive list of backlog tasks.
* Issues with the label [triaged](https://github.com/bbcarchdev/anansi/issues?q=is%3Aopen+is%3Aissue+label%3Atriaged) have been prioritised and added to the team’s internal backlog for development. Feel free to comment on the GitHub Issue in either case!
* You should never add nor remove the *triaged* label to yours or anybody else’s Github Issues.
* [Forking](https://github.com/bbcarchdev/anansi/fork) is encouraged! See the “[Contributing](#contributing)” section.
* Under **no** circumstances may you commit directly to this repository, even if you have push permission in GitHub.
* If you’re joining the development team, contact *“Archive Development Operations”* in the GAL to request access to GitLab (although your line manager should have done this for you in advance).

Finally, thanks for taking a look at this project! We hope it’ll be useful, do get in touch with us if we can help with anything (*“RES-BBC”* in the GAL, and we have staff in BC and PQ).

## License

**Anansi** is licensed under the terms of the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)

* Copyright © 2013 Mo McRoberts
* Copyright © 2014-2017 BBC

[travis]: https://img.shields.io/travis/bbcarchdev/anansi.svg
[license]: https://img.shields.io/badge/license-Apache%202.0-blue.svg
[language]: https://img.shields.io/badge/implemented%20in-C-yellow.svg 
[twitter]: https://img.shields.io/twitter/url/http/shields.io.svg?style=social&label=Follow%20@RES_Project

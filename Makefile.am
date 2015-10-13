## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2015 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

dist_noinst_DATA = \
	anansi.xml libcrawl.3.xml

dist_anansihtdoc_DATA = index.html libcrawl.3.html local.css

index.html: $(srcdir)/anansi.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<

libcrawl.3.html: $(srcdir)/libcrawl.3.xml $(XSLTFILES) $(NAV) $(LINKS)
	$(XML2HTML) $<
